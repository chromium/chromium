// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_text_log_handler.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/cpu.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc/webrtc_log_uploader.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/media/webrtc_logging_messages.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/webrtc_log.h"
#include "content/public/common/content_features.h"
#include "gpu/config/gpu_info.h"
#include "media/audio/audio_manager.h"
#include "media/webrtc/webrtc_switches.h"
#include "net/base/ip_address.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "services/service_manager/sandbox/features.h"

#if defined(OS_LINUX)
#include "base/linux_util.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/system/statistics_provider.h"
#endif

using base::IntToString;
using content::BrowserThread;

namespace {

std::string FormatMetaDataAsLogMessage(const MetaDataMap& meta_data) {
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
    default: { break; }
  }
  return sensitive_address;
#else
  return address.ToString();
#endif
}

net::NetworkInterfaceList GetNetworkInterfaceList() {
  net::NetworkInterfaceList network_list;
  net::GetNetworkList(&network_list,
                      net::EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES);
  return network_list;
}

}  // namespace

WebRtcLogBuffer::WebRtcLogBuffer()
    : buffer_(),
      circular_(&buffer_[0], sizeof(buffer_), sizeof(buffer_) / 2, false),
      read_only_(false) {}

WebRtcLogBuffer::~WebRtcLogBuffer() {
  DCHECK(read_only_ || thread_checker_.CalledOnValidThread());
}

void WebRtcLogBuffer::Log(const std::string& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!read_only_);
  circular_.Write(message.c_str(), message.length());
  const char eol = '\n';
  circular_.Write(&eol, 1);
}

webrtc_logging::PartialCircularBuffer WebRtcLogBuffer::Read() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(read_only_);
  return webrtc_logging::PartialCircularBuffer(&buffer_[0], sizeof(buffer_));
}

void WebRtcLogBuffer::SetComplete() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!read_only_) << "Already set? (programmer error)";
  read_only_ = true;
  // Detach from the current thread so that we can check reads on a different
  // thread.  This is to make sure that Read()s still happen on one thread only.
  thread_checker_.DetachFromThread();
}

WebRtcTextLogHandler::WebRtcTextLogHandler(int render_process_id)
    : render_process_id_(render_process_id), logging_state_(CLOSED) {}

WebRtcTextLogHandler::~WebRtcTextLogHandler() {
  // If the log isn't closed that means we haven't decremented the log count
  // in the LogUploader.
  DCHECK(logging_state_ == CLOSED || channel_is_closing_);
  DCHECK(!log_buffer_);
}

WebRtcTextLogHandler::LoggingState WebRtcTextLogHandler::GetState() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return logging_state_;
}

bool WebRtcTextLogHandler::GetChannelIsClosing() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return channel_is_closing_;
}

void WebRtcTextLogHandler::SetMetaData(std::unique_ptr<MetaDataMap> meta_data,
                                       const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  if (channel_is_closing_) {
    FireGenericDoneCallback(callback, false, "The renderer is closing.");
    return;
  }

  if (logging_state_ != CLOSED && logging_state_ != STARTED) {
    FireGenericDoneCallback(callback, false,
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

  FireGenericDoneCallback(callback, true, "");
}

bool WebRtcTextLogHandler::StartLogging(WebRtcLogUploader* log_uploader,
                                        const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  if (channel_is_closing_) {
    FireGenericDoneCallback(callback, false, "The renderer is closing.");
    return false;
  }

  if (logging_state_ != CLOSED) {
    FireGenericDoneCallback(callback, false, "A log is already open.");
    return false;
  }

  if (!log_uploader->ApplyForStartLogging()) {
    FireGenericDoneCallback(callback, false,
                            "Cannot start, maybe the maximum number of "
                            "simultaneuos logs has been reached.");
    return false;
  }

  logging_state_ = STARTING;

  DCHECK(!log_buffer_);
  log_buffer_.reset(new WebRtcLogBuffer());
  if (!meta_data_)
    meta_data_.reset(new MetaDataMap());

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetNetworkInterfaceList),
      base::BindOnce(&WebRtcTextLogHandler::LogInitialInfoOnIOThread, this,
                     callback));
  return true;
}

void WebRtcTextLogHandler::StartDone(const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  if (channel_is_closing_) {
    FireGenericDoneCallback(callback, false,
                            "Failed to start log. Renderer is closing.");
    return;
  }

  DCHECK_EQ(STARTING, logging_state_);

  logging_started_time_ = base::Time::Now();
  logging_state_ = STARTED;
  FireGenericDoneCallback(callback, true, "");
}

bool WebRtcTextLogHandler::StopLogging(const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  if (channel_is_closing_) {
    FireGenericDoneCallback(callback, false,
                            "Can't stop log. Renderer is closing.");
    return false;
  }

  if (logging_state_ != STARTED) {
    FireGenericDoneCallback(callback, false, "Logging not started.");
    return false;
  }

  stop_callback_ = callback;
  logging_state_ = STOPPING;

  content::WebRtcLog::ClearLogMessageCallback(render_process_id_);
  return true;
}

void WebRtcTextLogHandler::StopDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(stop_callback_);

  if (channel_is_closing_) {
    FireGenericDoneCallback(stop_callback_, false,
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
    FireGenericDoneCallback(stop_callback_, true, "");
    stop_callback_.Reset();
  }
}

void WebRtcTextLogHandler::ChannelClosing() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (logging_state_ == STARTING || logging_state_ == STARTED)
    content::WebRtcLog::ClearLogMessageCallback(render_process_id_);
  channel_is_closing_ = true;
}

void WebRtcTextLogHandler::DiscardLog() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(logging_state_ == STOPPED ||
         (channel_is_closing_ && logging_state_ != CLOSED));

  log_buffer_.reset();
  meta_data_.reset();
  logging_state_ = LoggingState::CLOSED;
}

void WebRtcTextLogHandler::ReleaseLog(
    std::unique_ptr<WebRtcLogBuffer>* log_buffer,
    std::unique_ptr<MetaDataMap>* meta_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(logging_state_ == STOPPED ||
         (channel_is_closing_ && logging_state_ != CLOSED));
  DCHECK(log_buffer_);
  DCHECK(meta_data_);

  // Checking log_buffer_ here due to seeing some crashes out in the wild.
  // See crbug/699960 for more details.
  // TODO(crbug/807547): Remove if condition.
  if (log_buffer_) {
    log_buffer_->SetComplete();
    *log_buffer = std::move(log_buffer_);
  }

  if (meta_data_)
    *meta_data = std::move(meta_data_);

  logging_state_ = LoggingState::CLOSED;
}

void WebRtcTextLogHandler::LogToCircularBuffer(const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(logging_state_, CLOSED);
  if (log_buffer_) {
    log_buffer_->Log(message);
  }
}

void WebRtcTextLogHandler::LogMessage(const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (logging_state_ == STARTED && !channel_is_closing_) {
    LogToCircularBuffer(WebRtcLoggingMessageData::Format(
        message, base::Time::Now(), logging_started_time_));
  }
}

void WebRtcTextLogHandler::LogWebRtcLoggingMessageData(
    const WebRtcLoggingMessageData& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LogToCircularBuffer(message.Format(logging_started_time_));
}

bool WebRtcTextLogHandler::ExpectLoggingStateStopped(
    const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (logging_state_ != STOPPED) {
    FireGenericDoneCallback(callback, false,
                            "Logging not stopped or no log open.");
    return false;
  }
  return true;
}

void WebRtcTextLogHandler::FireGenericDoneCallback(
    const GenericDoneCallback& callback,
    bool success,
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  if (error_message.empty()) {
    DCHECK(success);
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             base::BindOnce(callback, success, error_message));
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
    return "";
  };

  std::string error_message_with_state =
      base::StrCat({error_message, ". State=", state_string(), ". Channel is ",
                    channel_is_closing_ ? "" : "not ", "closing."});

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(callback, success, error_message_with_state));
}

void WebRtcTextLogHandler::LogInitialInfoOnIOThread(
    const GenericDoneCallback& callback,
    const net::NetworkInterfaceList& network_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (logging_state_ != STARTING || channel_is_closing_) {
    FireGenericDoneCallback(callback, false, "Logging cancelled.");
    return;
  }

  // Log start time (current time). We don't use base/i18n/time_formatting.h
  // here because we don't want the format of the current locale.
  base::Time::Exploded now = {0};
  base::Time::Now().LocalExplode(&now);
  LogToCircularBuffer(base::StringPrintf("Start %d-%02d-%02d %02d:%02d:%02d",
                                         now.year, now.month, now.day_of_month,
                                         now.hour, now.minute, now.second));

  // Write metadata if received before logging started.
  if (meta_data_ && !meta_data_->empty()) {
    std::string info = FormatMetaDataAsLogMessage(*meta_data_);
    LogToCircularBuffer(info);
  }

  // Chrome version
  LogToCircularBuffer("Chrome version: " + version_info::GetVersionNumber() +
                      " " + chrome::GetChannelName());

  // OS
  LogToCircularBuffer(base::SysInfo::OperatingSystemName() + " " +
                      base::SysInfo::OperatingSystemVersion() + " " +
                      base::SysInfo::OperatingSystemArchitecture());
#if defined(OS_LINUX)
  LogToCircularBuffer("Linux distribution: " + base::GetLinuxDistro());
#endif

  // CPU
  base::CPU cpu;
  LogToCircularBuffer(
      "Cpu: " + IntToString(cpu.family()) + "." + IntToString(cpu.model()) +
      "." + IntToString(cpu.stepping()) + ", x" +
      IntToString(base::SysInfo::NumberOfProcessors()) + ", " +
      IntToString(base::SysInfo::AmountOfPhysicalMemoryMB()) + "MB");
  LogToCircularBuffer("Cpu brand: " + cpu.cpu_brand());

  // Computer model
  std::string computer_model = "Not available";
#if defined(OS_MACOSX)
  computer_model = base::mac::GetModelIdentifier();
#elif defined(OS_CHROMEOS)
  chromeos::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
      chromeos::system::kHardwareClassKey, &computer_model);
#endif
  LogToCircularBuffer("Computer model: " + computer_model);

  // GPU
  gpu::GPUInfo gpu_info = content::GpuDataManager::GetInstance()->GetGPUInfo();
  const gpu::GPUInfo::GPUDevice& active_gpu = gpu_info.active_gpu();
  LogToCircularBuffer(
      "Gpu: machine-model-name=" + gpu_info.machine_model_name +
      ", machine-model-version=" + gpu_info.machine_model_version +
      ", vendor-id=" + base::UintToString(active_gpu.vendor_id) +
      ", device-id=" + base::UintToString(active_gpu.device_id) +
      ", driver-vendor=" + active_gpu.driver_vendor +
      ", driver-version=" + active_gpu.driver_version);
  LogToCircularBuffer("OpenGL: gl-vendor=" + gpu_info.gl_vendor +
                      ", gl-renderer=" + gpu_info.gl_renderer +
                      ", gl-version=" + gpu_info.gl_version);

  // AudioService features
  auto enabled_or_disabled_string = [](auto& feature) {
    return base::FeatureList::IsEnabled(feature) ? "enabled" : "disabled";
  };
  LogToCircularBuffer(base::StrCat(
      {"AudioService: AudioStreams=",
       enabled_or_disabled_string(features::kAudioServiceAudioStreams),
       ", OutOfProcess=",
       enabled_or_disabled_string(features::kAudioServiceOutOfProcess),
       ", LaunchOnStartup=",
       enabled_or_disabled_string(features::kAudioServiceLaunchOnStartup),
       ", Sandbox=",
       enabled_or_disabled_string(
           service_manager::features::kAudioServiceSandbox),
       ", ApmInAudioService=",
       enabled_or_disabled_string(features::kWebRtcApmInAudioService)}));

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
  LogToCircularBuffer("Discovered " +
                      base::NumberToString(network_list.size()) +
                      " network interfaces:");
  for (const auto& network : network_list) {
    LogToCircularBuffer(
        "Name: " + network.friendly_name + ", Address: " +
        IPAddressToSensitiveString(network.address) + ", Type: " +
        net::NetworkChangeNotifier::ConnectionTypeToString(network.type));
  }

  StartDone(callback);

  // After the above data has been written, tell the browser to enable logging.
  // TODO(terelius): Once we have moved over to Mojo, we could tell the
  // renderer to start logging here, but for the time being
  // WebRtcLoggingHandlerHost::StartLogging will be responsible for sending
  // that IPC message.
  content::WebRtcLog::SetLogMessageCallback(
      render_process_id_, base::Bind(&WebRtcTextLogHandler::LogMessage, this));
}
