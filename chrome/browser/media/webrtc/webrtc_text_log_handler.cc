// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_text_log_handler.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/cpu.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/audio_service_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/media/webrtc_logging.mojom.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/webrtc_log.h"
#include "content/public/common/content_features.h"
#include "gpu/config/gpu_info.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "media/webrtc/webrtc_features.h"
#include "net/base/ip_address.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "services/network/public/mojom/network_service.mojom.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/linux_util.h"
#include "base/task/thread_pool.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/statistics_provider.h"
#endif

using base::NumberToString;

namespace {

void ForwardMessageViaTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::RepeatingCallback<void(const std::string&)> callback,
    const std::string& message) {
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(callback), message));
}

std::string Format(const std::string& message,
                   base::Time timestamp,
                   base::Time start_time) {
  int32_t interval_ms =
      static_cast<int32_t>((timestamp - start_time).InMilliseconds());
  // Log start time (current time).
  const std::string now =
      base::UnlocalizedTimeFormatWithPattern(base::Time::Now(), "HH:mm:ss.SSS");
  return base::StringPrintf("[%03d:%03d, %s] %s", interval_ms / 1000,
                            interval_ms % 1000, now.c_str(), message.c_str());
}

std::string FormatMetaDataAsLogMessage(const WebRtcLogMetaDataMap& meta_data) {
  std::string message;
  for (auto& kv : meta_data) {
    message += kv.first + ": " + kv.second + '\n';
  }
  // Remove last '\n'.
  if (!message.empty())
    message.erase(message.size() - 1, 1);  // TODO(terelius): Use pop_back()
  return message;
}

// For privacy reasons when logging IP addresses. The returned "sensitive
// string" is for release builds a string with the end stripped away. Last
// octet for IPv4 and last 80 bits (5 groups) for IPv6. String will be
// "1.2.3.x" and "1.2.3::" respectively. For debug builds, the string is
// not stripped.
std::string IPAddressToSensitiveString(const net::IPAddress& address) {
#if defined(NDEBUG)
  std::string sensitive_address;
  switch (address.size()) {
    case net::IPAddress::kIPv4AddressSize: {
      sensitive_address = address.ToString();
      size_t find_pos = sensitive_address.rfind('.');
      if (find_pos == std::string::npos)
        return std::string();
      sensitive_address.resize(find_pos);
      sensitive_address += ".x";
      break;
    }
    case net::IPAddress::kIPv6AddressSize: {
      // TODO(grunell): Create a string of format "1:2:3:x:x:x:x:x" to clarify
      // that the end has been stripped out.
      net::IPAddressBytes stripped = address.bytes();
      std::fill(stripped.begin() + 6, stripped.end(), 0);
      sensitive_address = net::IPAddress(stripped).ToString();
      break;
    }
    default: {
      break;
    }
  }
  return sensitive_address;
#else
  return address.ToString();
#endif
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class StartError {
  kRendererClosing = 0,
  kLogAlreadyOpen = 1,
  kApplyForStartFailed = 2,
  kCancelled = 3,
  kRendererClosingInStartDone = 4,
  kMaxValue = kRendererClosingInStartDone,
};

void RecordStartError(StartError error) {
  base::UmaHistogramEnumeration("WebRtcTextLogging.StartError", error);
}

}  // namespace

WebRtcTextLogHandler::WebRtcTextLogHandler(int render_process_id)
    : render_process_id_(render_process_id), logging_state_(CLOSED) {}

WebRtcTextLogHandler::~WebRtcTextLogHandler() {
  // If the log isn't closed that means we haven't decremented the log count
  // in the LogUploader.
  DCHECK(logging_state_ == CLOSED || channel_is_closing_);
  DCHECK(!log_buffer_);
}

WebRtcTextLogHandler::LoggingState WebRtcTextLogHandler::GetState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return logging_state_;
}

bool WebRtcTextLogHandler::GetChannelIsClosing() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return channel_is_closing_;
}

void WebRtcTextLogHandler::SetMetaData(
    std::unique_ptr<WebRtcLogMetaDataMap> meta_data,
    GenericDoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  if (channel_is_closing_) {
    FireGenericDoneCallback(std::move(callback), false,
                            "The renderer is closing.");
    return;
  }

  if (logging_state_ != CLOSED && logging_state_ != STARTED) {
    FireGenericDoneCallback(std::move(callback), false,
                            "Meta data must be set before stop or upload.");
    return;
  }

  if (logging_state_ == LoggingState::STARTED) {
    std::string meta_data_message = FormatMetaDataAsLogMessage(*meta_data);
    LogToCircularBuffer(meta_data_message);
  }

  if (!meta_data_) {
    // If no meta data has been set previously, set it now.
    meta_data_ = std::move(meta_data);
  } else if (meta_data) {
    // If there is existing meta data, update it and any new fields. The meta
    // data is kept around to be uploaded separately from the log.
    for (const auto& it : *meta_data)
      (*meta_data_)[it.first] = it.second;
  }

  FireGenericDoneCallback(std::move(callback), true, "");
}

bool WebRtcTextLogHandler::StartLogging(GenericDoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  base::UmaHistogramBoolean("WebRtcTextLogging.StartCalled", true);

  if (channel_is_closing_) {
    FireGenericDoneCallback(std::move(callback), false,
                            "The renderer is closing.");
    RecordStartError(StartError::kRendererClosing);
    return false;
  }

  if (logging_state_ != CLOSED) {
    FireGenericDoneCallback(std::move(callback), false,
                            "A log is already open.");
    RecordStartError(StartError::kLogAlreadyOpen);
    return false;
  }

  WebRtcLogUploader* log_uploader = WebRtcLogUploader::GetInstance();
  if (!log_uploader->ApplyForStartLogging()) {
    FireGenericDoneCallback(std::move(callback), false,
                            "Cannot start, maybe the maximum number of "
                            "simultaneuos logs has been reached.");
    RecordStartError(StartError::kApplyForStartFailed);
    return false;
  }

  logging_state_ = STARTING;

  DCHECK(!log_buffer_);
  log_buffer_ = std::make_unique<WebRtcLogBuffer>();
  if (!meta_data_)
    meta_data_ = std::make_unique<WebRtcLogMetaDataMap>();

  content::GetNetworkService()->GetNetworkList(
      net::EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
      base::BindOnce(&WebRtcTextLogHandler::OnGetNetworkInterfaceList,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  return true;
}

void WebRtcTextLogHandler::StartDone(GenericDoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  if (channel_is_closing_) {
    FireGenericDoneCallback(std::move(callback), false,
                            "Failed to start log. Renderer is closing.");
    RecordStartError(StartError::kRendererClosingInStartDone);
    return;
  }

  DCHECK_EQ(STARTING, logging_state_);

  base::UmaHistogramSparse("WebRtcTextLogging.Started", web_app_id_);

  logging_started_time_ = base::Time::Now();
  logging_state_ = STARTED;
  FireGenericDoneCallback(std::move(callback), true, "");
}

bool WebRtcTextLogHandler::StopLogging(GenericDoneCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  if (channel_is_closing_) {
    FireGenericDoneCallback(std::move(callback), false,
                            "Can't stop log. Renderer is closing.");
    return false;
  }

  if (logging_state_ != STARTED) {
    FireGenericDoneCallback(std::move(callback), false, "Logging not started.");
    return false;
  }

  stop_callback_ = std::move(callback);
  logging_state_ = STOPPING;

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&content::WebRtcLog::ClearLogMessageCallback,
                                render_process_id_));
  return true;
}

void WebRtcTextLogHandler::StopDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stop_callback_);

  if (channel_is_closing_) {
    FireGenericDoneCallback(std::move(stop_callback_), false,
                            "Failed to stop log. Renderer is closing.");
    return;
  }

  // If we aren't in STOPPING state, then there is a bug in the caller, since
  // it is responsible for checking the state before making the call. If we do
  // enter here in a bad state, then we can't use the stop_callback_ or we
  // might fire the same callback multiple times.
  DCHECK_EQ(STOPPING, logging_state_);
  if (logging_state_ == STOPPING) {
    logging_started_time_ = base::Time();
    logging_state_ = STOPPED;
    FireGenericDoneCallback(std::move(stop_callback_), true, "");
  }
}

void WebRtcTextLogHandler::ChannelClosing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (logging_state_ == STARTING || logging_state_ == STARTED) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&content::WebRtcLog::ClearLogMessageCallback,
                                  render_process_id_));
  }
  channel_is_closing_ = true;
}

void WebRtcTextLogHandler::DiscardLog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(logging_state_ == STOPPED ||
         (channel_is_closing_ && logging_state_ != CLOSED));

  base::UmaHistogramSparse("WebRtcTextLogging.Discard", web_app_id_);

  log_buffer_.reset();
  meta_data_.reset();
  logging_state_ = LoggingState::CLOSED;
}

void WebRtcTextLogHandler::ReleaseLog(
    std::unique_ptr<WebRtcLogBuffer>* log_buffer,
    std::unique_ptr<WebRtcLogMetaDataMap>* meta_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(logging_state_ == STOPPED ||
         (channel_is_closing_ && logging_state_ != CLOSED));
  DCHECK(log_buffer_);
  DCHECK(meta_data_);

  // Checking log_buffer_ here due to seeing some crashes out in the wild.
  // See crbug/699960 for more details.
  // TODO(crbug.com/41368009): Remove if condition.
  if (log_buffer_) {
    log_buffer_->SetComplete();
    *log_buffer = std::move(log_buffer_);
  }

  if (meta_data_)
    *meta_data = std::move(meta_data_);

  logging_state_ = LoggingState::CLOSED;
}

void WebRtcTextLogHandler::LogToCircularBuffer(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(logging_state_, CLOSED);
  if (log_buffer_) {
    log_buffer_->Log(message);
  }
}

void WebRtcTextLogHandler::LogMessage(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (logging_state_ == STARTED && !channel_is_closing_) {
    LogToCircularBuffer(
        Format(message, base::Time::Now(), logging_started_time_));
  }
}

void WebRtcTextLogHandler::LogWebRtcLoggingMessage(
    const chrome::mojom::WebRtcLoggingMessage* message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogToCircularBuffer(
      Format(message->data, message->timestamp, logging_started_time_));
}

bool WebRtcTextLogHandler::ExpectLoggingStateStopped(
    GenericDoneCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (logging_state_ != STOPPED) {
    FireGenericDoneCallback(std::move(*callback), false,
                            "Logging not stopped or no log open.");
    return false;
  }
  return true;
}

void WebRtcTextLogHandler::FireGenericDoneCallback(
    GenericDoneCallback callback,
    bool success,
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  if (error_message.empty()) {
    DCHECK(success);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), success, error_message));
    return;
  }

  DCHECK(!success);

  // Add current logging state to error message.
  auto state_string = [&] {
    switch (logging_state_) {
      case LoggingState::CLOSED:
        return "closed";
      case LoggingState::STARTING:
        return "starting";
      case LoggingState::STARTED:
        return "started";
      case LoggingState::STOPPING:
        return "stopping";
      case LoggingState::STOPPED:
        return "stopped";
    }
    NOTREACHED();
  };

  std::string error_message_with_state =
      base::StrCat({error_message, ". State=", state_string(), ". Channel is ",
                    channel_is_closing_ ? "" : "not ", "closing."});

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), success, error_message_with_state));
}

void WebRtcTextLogHandler::SetWebAppId(int web_app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  web_app_id_ = web_app_id;
}

void WebRtcTextLogHandler::OnGetNetworkInterfaceList(
    GenericDoneCallback callback,
    const std::optional<net::NetworkInterfaceList>& networks) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Hop to a background thread to get the distro string, which can block.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&base::GetLinuxDistro),
      base::BindOnce(&WebRtcTextLogHandler::OnGetNetworkInterfaceListFinish,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     networks));
#else
  OnGetNetworkInterfaceListFinish(std::move(callback), networks, "");
#endif
}

void WebRtcTextLogHandler::OnGetNetworkInterfaceListFinish(
    GenericDoneCallback callback,
    const std::optional<net::NetworkInterfaceList>& networks,
    const std::string& linux_distro) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (logging_state_ != STARTING || channel_is_closing_) {
    FireGenericDoneCallback(std::move(callback), false, "Logging cancelled.");
    RecordStartError(StartError::kCancelled);
    return;
  }

  // Log start time (current time).
  LogToCircularBuffer(base::UnlocalizedTimeFormatWithPattern(
      base::Time::Now(), "'Start 'y-MM-dd HH:mm:ss"));

  // Write metadata if received before logging started.
  if (meta_data_ && !meta_data_->empty()) {
    std::string info = FormatMetaDataAsLogMessage(*meta_data_);
    LogToCircularBuffer(info);
  }

  // Chrome version
  LogToCircularBuffer(
      base::StrCat({"Chrome version: ", version_info::GetVersionNumber(), " ",
                    chrome::GetChannelName(chrome::WithExtendedStable(true))}));

  // OS
  LogToCircularBuffer(base::SysInfo::OperatingSystemName() + " " +
                      base::SysInfo::OperatingSystemVersion() + " " +
                      base::SysInfo::OperatingSystemArchitecture());
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  { LogToCircularBuffer("Linux distribution: " + linux_distro); }
#endif

  // CPU
  base::CPU cpu;
  LogToCircularBuffer(
      "Cpu: " + NumberToString(cpu.family()) + "." +
      NumberToString(cpu.model()) + "." + NumberToString(cpu.stepping()) +
      ", x" + NumberToString(base::SysInfo::NumberOfProcessors()) + ", " +
      NumberToString(base::SysInfo::AmountOfPhysicalMemoryMB()) + "MB");
  LogToCircularBuffer("Cpu brand: " + cpu.cpu_brand());

  // Computer model
  std::string computer_model = "Not available";
#if BUILDFLAG(IS_MAC)
  computer_model = base::SysInfo::HardwareModelName();
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  if (const std::optional<std::string_view> computer_model_statistic =
          ash::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
              ash::system::kHardwareClassKey)) {
    computer_model = std::string(computer_model_statistic.value());
  }
#endif
  LogToCircularBuffer("Computer model: " + computer_model);

  // GPU
  gpu::GPUInfo gpu_info = content::GpuDataManager::GetInstance()->GetGPUInfo();
  const gpu::GPUInfo::GPUDevice& active_gpu = gpu_info.active_gpu();
  LogToCircularBuffer(
      "Gpu: machine-model-name=" + gpu_info.machine_model_name +
      ", machine-model-version=" + gpu_info.machine_model_version +
      ", vendor-id=" + base::NumberToString(active_gpu.vendor_id) +
      ", device-id=" + base::NumberToString(active_gpu.device_id) +
      ", driver-vendor=" + active_gpu.driver_vendor +
      ", driver-version=" + active_gpu.driver_version);
  LogToCircularBuffer("OpenGL: gl-vendor=" + gpu_info.gl_vendor +
                      ", gl-renderer=" + gpu_info.gl_renderer +
                      ", gl-version=" + gpu_info.gl_version);

  // AudioService features
  auto enabled_or_disabled_feature_string = [](auto& feature) {
    return base::FeatureList::IsEnabled(feature) ? "enabled" : "disabled";
  };
  auto enabled_or_disabled_bool_string = [](bool value) {
    return value ? "enabled" : "disabled";
  };
  LogToCircularBuffer(base::StrCat(
      {"AudioService: OutOfProcess=",
       enabled_or_disabled_feature_string(features::kAudioServiceOutOfProcess),
       ", LaunchOnStartup=",
       enabled_or_disabled_feature_string(
           features::kAudioServiceLaunchOnStartup),
       ", Sandbox=",
       enabled_or_disabled_bool_string(IsAudioServiceSandboxEnabled())}));

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  if (media::IsChromeWideEchoCancellationEnabled()) {
    LogToCircularBuffer(base::StrCat(
        {"ChromeWideEchoCancellation : Enabled", ", minimize_resampling = ",
         media::kChromeWideEchoCancellationMinimizeResampling.Get() ? "true"
                                                                    : "false",
         ", allow_all_sample_rates = ",
         media::kChromeWideEchoCancellationAllowAllSampleRates.Get()
             ? "true"
             : "false"}));
  } else {
    LogToCircularBuffer("ChromeWideEchoCancellation : Disabled");
  }

  if (base::FeatureList::IsEnabled(media::kDecreaseProcessingAudioFifoSize)) {
    LogToCircularBuffer(base::StrCat(
        {"DecreaseProcessingAudioFifoSize : Enabled", ", fifo_size = ",
         base::NumberToString(media::GetProcessingAudioFifoSize())}));
  } else {
    LogToCircularBuffer("DecreaseProcessingAudioFifoSize : Disabled");
  }
#endif

  // Audio manager
  // On some platforms, this can vary depending on build flags and failure
  // fallbacks. On Linux for example, we fallback on ALSA if PulseAudio fails to
  // initialize. TODO(http://crbug/843202): access AudioManager name via Audio
  // service interface.
  media::AudioManager* audio_manager = media::AudioManager::Get();
  LogToCircularBuffer(base::StringPrintf(
      "Audio manager: %s",
      audio_manager ? audio_manager->GetName() : "Out of process"));

  // Network interfaces
  const net::NetworkInterfaceList empty_network_list;
  const net::NetworkInterfaceList& network_list =
      networks.has_value() ? *networks : empty_network_list;
  LogToCircularBuffer("Discovered " +
                      base::NumberToString(network_list.size()) +
                      " network interfaces:");
  for (const auto& network : network_list) {
    LogToCircularBuffer(
        "Name: " + network.friendly_name + ", Address: " +
        IPAddressToSensitiveString(network.address) + ", Type: " +
        net::NetworkChangeNotifier::ConnectionTypeToString(network.type));
  }

  StartDone(std::move(callback));

  // After the above data has been written, tell the browser to enable logging.
  // TODO(terelius): Once we have moved over to Mojo, we could tell the
  // renderer to start logging here, but for the time being
  // WebRtcLoggingHandlerHost::StartLogging will be responsible for sending
  // that IPC message.

  // TODO(darin): Change SetLogMessageCallback to run on the UI thread.

  auto log_message_callback =
      base::BindRepeating(&ForwardMessageViaTaskRunner,
                          base::SequencedTaskRunner::GetCurrentDefault(),
                          base::BindRepeating(&WebRtcTextLogHandler::LogMessage,
                                              weak_factory_.GetWeakPtr()));
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&content::WebRtcLog::SetLogMessageCallback,
                     render_process_id_, std::move(log_message_callback)));
}
