// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/artemis/data_aggregator_service.h"

#include <array>
#include <map>
#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/syslog_logging.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/chromebox_for_meetings/artemis/artemis_features.h"
#include "chrome/browser/ash/chromebox_for_meetings/artemis/log_source.h"
#include "chrome/browser/ash/chromebox_for_meetings/artemis/persistent_db.h"
#include "chrome/browser/ash/chromebox_for_meetings/artemis/specialized_log_sources.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace ash::cfm {

namespace {

// Local convenience aliases
using mojom::DataFilter::FilterType::CHANGE;
using mojom::DataFilter::FilterType::REGEX;

static DataAggregatorService* g_data_aggregator_service = nullptr;

constexpr base::TimeDelta kServiceAdaptorRetryDelay = base::Seconds(1);
constexpr size_t kServiceAdaptorRetryMaxTries = 5;

// Log data labels (processed on the Ratchet side)
constexpr char kRatchetChromeVersionLabel[] = "chrome";
constexpr char kRatchetDeviceIdLabel[] = "device_id";
constexpr char kRatchetEmailLabel[] = "name";
constexpr char kRatchetHwidLabel[] = "hwid";
constexpr char kRatchetOsChannelLabel[] = "os_release_track";
constexpr char kRatchetOsVersionLabel[] = "os";

constexpr net::BackoffEntry::Policy kEnqueueRetryBackoffPolicy = {
    0,              // Number of initial errors to ignore.
    1000,           // Initial delay in ms.
    2.0,            // Factor by which the waiting time will be multiplied.
    0.2,            // Fuzzing percentage.
    60 * 1000 * 5,  // Maximum delay in ms.
    -1,             // Never discard the entry.
    true,           // Use initial delay.
};

// Create aliases for readability. C++ has ugly syntax for global maps.
using CommandSourceMap =
    std::map<features::TelemetryVerbosity, std::vector<const char*>>;

using PollFrequencyMap =
    std::map<features::TelemetryVerbosity, base::TimeDelta>;

/*
 * IMPORTANT: When adding new commands to the below lists, please take care
 * to choose commands with a relatively small amount of output. Rule of thumb:
 * avoid commands that output more than (payload_max_size_bytes_ / 2) bytes of
 * data. We don't want to overwhelm missived with large payloads.
 *
 * To check size output, pipe the command to `wc`. The byte count will be the
 * last number.
 */
const CommandSourceMap& GetLocalCommandSourceMap() {
  static const base::NoDestructor<CommandSourceMap> map({
      // Note: these are cumulative.
      {features::TelemetryVerbosity::kWatchdog,
       {
           "lsusb -t",
       }},
      {features::TelemetryVerbosity::kInfo,
       {
           "df -h /var",
           "du -sh /var/log /var/spool/crash",
           "free -m",
           "ip -br addr",
           "v4l2-ctl --list-devices",
       }},
      {features::TelemetryVerbosity::kVerbose,
       {
           "iostat -o JSON",
           "top -b -n 1 -o %MEM",
       }},
  });
  return *map;
}

const PollFrequencyMap& GetLocalCommandPollFrequencyMap() {
  // Poll less frequently at higher verbosities to keep the payloads small.
  static const base::NoDestructor<PollFrequencyMap> map({
      {features::TelemetryVerbosity::kWatchdog, base::Seconds(5)},
      {features::TelemetryVerbosity::kInfo, base::Minutes(10)},
      {features::TelemetryVerbosity::kVerbose, base::Minutes(30)},
  });
  return *map;
}

constexpr const char* kLocalLogSources[] = {
    kCfmAuditLogFile,      kCfmBiosInfoLogFile,     kCfmChromeLogFile,
    kCfmChromeUserLogFile, kCfmCrosEcLogFile,       kCfmEventlogLogFile,
    kCfmFwupdLogFile,      kCfmPowerdLogFile,       kCfmSyslogLogFile,
    kCfmUiLogFile,         kCfmUpdateEngineLogFile, kCfmVariationsListLogFile,
};

// Define sane minimums and maximums for our Finch-controlled configs.
constexpr base::TimeDelta kMinimumFetchFrequency = base::Minutes(1);
constexpr base::TimeDelta kMaximumFetchFrequency = base::Hours(1);

constexpr base::TimeDelta kMinimumLogPollFrequency = base::Seconds(10);
constexpr base::TimeDelta kMaximumLogPollFrequency = base::Hours(1);

constexpr size_t kMinimumLogBatchSize = 10;
constexpr size_t kMaximumLogBatchSize = 10 * 1000;

constexpr size_t kMinimumPayloadMaxSizeBytes = 1000;
constexpr size_t kMaximumPayloadMaxSizeBytes = 1000 * 1000;

constexpr size_t kMinimumPayloadQueueMaxSize = 1;
constexpr size_t kMaximumPayloadQueueMaxSize = 10;

}  // namespace

// static
void DataAggregatorService::Initialize() {
  CHECK(!g_data_aggregator_service);
  g_data_aggregator_service = new DataAggregatorService();
}

// static
void DataAggregatorService::InitializeForTesting(
    DataAggregatorService* data_aggregator_service) {
  CHECK(!g_data_aggregator_service);
  g_data_aggregator_service = data_aggregator_service;
}

// static
void DataAggregatorService::Shutdown() {
  CHECK(g_data_aggregator_service);
  delete g_data_aggregator_service;
  g_data_aggregator_service = nullptr;
}

// static
DataAggregatorService* DataAggregatorService::Get() {
  CHECK(g_data_aggregator_service)
      << "DataAggregatorService::Get() called before Initialize()";
  return g_data_aggregator_service;
}

// static
bool DataAggregatorService::IsInitialized() {
  return g_data_aggregator_service;
}

bool DataAggregatorService::ServiceRequestReceived(
    const std::string& interface_name) {
  if (interface_name != mojom::DataAggregator::Name_) {
    return false;
  }
  service_adaptor_.BindServiceAdaptor();
  return true;
}

void DataAggregatorService::OnAdaptorDisconnect() {
  LOG(ERROR) << "mojom::DataAggregator Service Adaptor has been disconnected";
  // CleanUp to follow the lifecycle of the primary CfmServiceContext
  receivers_.Clear();
}

void DataAggregatorService::OnBindService(
    mojo::ScopedMessagePipeHandle receiver_pipe) {
  receivers_.Add(this, mojo::PendingReceiver<mojom::DataAggregator>(
                           std::move(receiver_pipe)));
}

void DataAggregatorService::GetDataSourceNames(
    GetDataSourceNamesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::string> source_names;
  for (auto const& data_source : data_source_map_) {
    source_names.push_back(data_source.first);
  }

  std::move(callback).Run(std::move(source_names));
}

void DataAggregatorService::AddDataSource(
    const std::string& source_name,
    mojo::PendingRemote<mojom::DataSource> new_data_source,
    AddDataSourceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (data_source_map_.count(source_name) != 0) {
    LOG(ERROR) << "Attempted to add source name " << source_name
               << " more than once. Disregarding this one.";
    std::move(callback).Run(false /* success */);
    return;
  }

  mojo::Remote<mojom::DataSource> data_source(std::move(new_data_source));
  data_source_map_[source_name] = std::move(data_source);
  std::move(callback).Run(true /* success */);
}

void DataAggregatorService::AddWatchDog(
    const std::string& source_name,
    mojom::DataFilterPtr filter,
    mojo::PendingRemote<mojom::DataWatchDog> watch_dog,
    AddWatchDogCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(b/326440932): add an enum for "watchable" data sources
  // and deny requests that are outside of this list.
  if (data_source_map_.count(source_name) != 0) {
    LOG(WARNING) << "Attempted to add a watchdog to a non-existent source: "
                 << source_name;
    std::move(callback).Run(false /* success */);
    return;
  }

  // Pass the callback through to the data source and run it there.
  data_source_map_[source_name]->AddWatchDog(
      std::move(filter), std::move(watch_dog), std::move(callback));
}

void DataAggregatorService::AddLocalCommandSource(
    const std::string& command,
    const base::TimeDelta& poll_freq) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(data_source_map_.count(command) == 0)
      << "Local command '" << command << "' was added twice.";

  mojo::Remote<mojom::DataSource> remote;
  local_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](mojo::PendingReceiver<mojom::DataSource> pending_receiver,
             const std::string& device_id, const std::string& command,
             const size_t payload_max_size_bytes,
             const base::TimeDelta& poll_freq) {
            auto source = std::make_unique<CommandSource>(
                command, payload_max_size_bytes, poll_freq);
            source->AssignDeviceID(device_id);
            source->StartCollectingData();

            mojo::MakeSelfOwnedReceiver(std::move(source),
                                        std::move(pending_receiver));
          },
          remote.BindNewPipeAndPassReceiver(),
          active_transport_payload_.permanent_id(), command,
          payload_max_size_bytes_, poll_freq));

  remote.set_disconnect_handler(
      base::BindOnce(&DataAggregatorService::OnLocalCommandDisconnect,
                     base::Unretained(this), command, poll_freq));

  data_source_map_[command] = std::move(remote);
}

void DataAggregatorService::OnLocalCommandDisconnect(
    const std::string& command,
    const base::TimeDelta& poll_freq) {
  // This is unlikely, but if one of our local remotes disconnects,
  // just request to re-add it. The pointers in our local maps will
  // be overridden, and the old objects will be destroyed.
  LOG(WARNING) << "Local DataSource for '" << command << "' has disconnected; "
               << "attempting to reconnect.";
  data_source_map_.erase(command);
  AddLocalCommandSource(command, poll_freq);
}

void DataAggregatorService::AddLocalLogSource(const std::string& filepath) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(data_source_map_.count(filepath) == 0)
      << "Local log file '" << filepath << "' was added twice.";

  mojo::Remote<mojom::DataSource> remote;
  local_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](mojo::PendingReceiver<mojom::DataSource> pending_receiver,
             const std::string& device_id, const std::string& filepath,
             const size_t payload_max_size_bytes,
             const base::TimeDelta log_poll_frequency,
             const size_t log_batch_size) {
            auto source = LogSource::Create(filepath, payload_max_size_bytes,
                                            log_poll_frequency, log_batch_size);
            source->AssignDeviceID(device_id);
            source->StartCollectingData();

            mojo::MakeSelfOwnedReceiver(std::move(source),
                                        std::move(pending_receiver));
          },
          remote.BindNewPipeAndPassReceiver(),
          active_transport_payload_.permanent_id(), filepath,
          payload_max_size_bytes_, log_poll_frequency_, log_batch_size_));

  remote.set_disconnect_handler(
      base::BindOnce(&DataAggregatorService::OnLocalLogDisconnect,
                     base::Unretained(this), filepath));

  data_source_map_[filepath] = std::move(remote);
}

void DataAggregatorService::OnLocalLogDisconnect(const std::string& filepath) {
  // This is unlikely, but if one of our local remotes disconnects,
  // just request to re-add it. The pointers in our local maps will
  // be overridden, and the old objects will be destroyed.
  LOG(WARNING) << "Local DataSource for '" << filepath << "' has disconnected; "
               << "attempting to reconnect.";
  data_source_map_.erase(filepath);
  AddLocalLogSource(filepath);
}

void DataAggregatorService::OnMojoDisconnect() {
  VLOG(2) << "mojom::DataAggregator disconnected";
}

void DataAggregatorService::InitializeCommandSources(
    enum features::TelemetryVerbosity verbosity) {
  const auto& command_map = GetLocalCommandSourceMap();
  const auto& freq_map = GetLocalCommandPollFrequencyMap();

  auto cmd_list = command_map.at(verbosity);
  auto frequency = freq_map.at(verbosity);

  for (const char* cmd : cmd_list) {
    VLOG(1) << "Adding command '" << cmd << "' to sources.";
    AddLocalCommandSource(cmd, frequency);
  }
}

void DataAggregatorService::InitializeLocalSources() {
  // Add local command sources. Watchdog commands are always enabled.
  InitializeCommandSources(features::TelemetryVerbosity::kWatchdog);

  // Add verbosity-dependent telemetry logs. Higher verbosity can be set
  // via ArtemisDynamicCloudLogging experiment.
  auto verbosity = features::kTelemetryVerbosity.Get();

  if (verbosity == features::TelemetryVerbosity::kVerbose) {
    LOG(WARNING) << "Enabling VERBOSE level telemetry.";
    InitializeCommandSources(features::TelemetryVerbosity::kVerbose);
    InitializeCommandSources(features::TelemetryVerbosity::kInfo);
  } else if (verbosity == features::TelemetryVerbosity::kInfo) {
    LOG(WARNING) << "Enabling INFO level telemetry.";
    InitializeCommandSources(features::TelemetryVerbosity::kInfo);
  }

  // Add local log file sources
  for (auto* const logfile : kLocalLogSources) {
    VLOG(1) << "Adding log file '" << logfile << "' to local sources.";
    AddLocalLogSource(logfile);
  }

  // Add any additional log files activated by experiment
  std::string param = features::kSupplementaryLogs.Get();
  if (!param.empty()) {
    std::vector<std::string> items = base::SplitString(
        param, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    for (const auto& file : items) {
      // NB: using LOG(WARNING) here so we can track this in prod.
      LOG(WARNING) << "[Dynamic] Adding log file '" << file
                   << "' to local sources.";
      AddLocalLogSource(file);
    }
  }
}

void DataAggregatorService::InitializeUploadEndpoint(size_t num_tries) {
  // Hook into the existing CfmLoggerService.
  const std::string kMeetDevicesLoggerInterfaceName =
      chromeos::cfm::mojom::MeetDevicesLogger::Name_;

  // We'll only be bound if we tried to initialize the endpoint
  // already and failed. Just reset and try again.
  if (uploader_remote_.is_bound()) {
    uploader_remote_.reset();
  }

  service_adaptor_.GetService(
      kMeetDevicesLoggerInterfaceName,
      uploader_remote_.BindNewPipeAndPassReceiver().PassPipe(),
      base::BindOnce(&DataAggregatorService::OnRequestBindUploadService,
                     weak_ptr_factory_.GetWeakPtr(),
                     kMeetDevicesLoggerInterfaceName, num_tries));
}

void DataAggregatorService::OnRequestBindUploadService(
    const std::string& interface_name,
    size_t num_tries,
    bool success) {
  VLOG(2) << "Uploader RequestBindService result: " << success
          << " for interface: " << interface_name;

  if (success) {
    last_upload_time_ = base::TimeTicks::Now();
    InitializeDeviceInfoEndpoint(/*num_tries=*/0);
    return;
  }

  if (num_tries >= kServiceAdaptorRetryMaxTries) {
    LOG(ERROR) << "Retry limit reached for connecting to " << interface_name
               << ". Remote calls will fail.";
    base::UmaHistogramEnumeration(kSetupStatusMetricName,
                                  SetupStatus::kLoggerServiceBindFailure);
    return;
  }

  VLOG(2) << "Retrying service adaptor connection in "
          << kServiceAdaptorRetryDelay;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DataAggregatorService::InitializeUploadEndpoint,
                     weak_ptr_factory_.GetWeakPtr(), num_tries + 1),
      kServiceAdaptorRetryDelay);
}

void DataAggregatorService::InitializeDeviceInfoEndpoint(size_t num_tries) {
  // Hook into the existing CfmDeviceInfoService.
  const std::string kMeetDevicesInfoInterfaceName =
      chromeos::cfm::mojom::MeetDevicesInfo::Name_;

  // We'll only be bound if we tried to initialize the endpoint
  // already and failed. Just reset and try again.
  if (device_info_remote_.is_bound()) {
    device_info_remote_.reset();
  }

  service_adaptor_.GetService(
      kMeetDevicesInfoInterfaceName,
      device_info_remote_.BindNewPipeAndPassReceiver().PassPipe(),
      base::BindOnce(&DataAggregatorService::OnRequestBindDeviceInfoService,
                     weak_ptr_factory_.GetWeakPtr(),
                     kMeetDevicesInfoInterfaceName, num_tries));
}

void DataAggregatorService::OnRequestBindDeviceInfoService(
    const std::string& interface_name,
    size_t num_tries,
    bool success) {
  VLOG(2) << "DeviceInfo RequestBindService result: " << success
          << " for interface: " << interface_name;

  if (success) {
    RequestDeviceInfo();
    return;
  }

  if (num_tries >= kServiceAdaptorRetryMaxTries) {
    LOG(ERROR) << "Retry limit reached for connecting to " << interface_name
               << ". Remote calls will fail.";
    base::UmaHistogramEnumeration(kSetupStatusMetricName,
                                  SetupStatus::kDeviceInfoServiceBindFailure);
    return;
  }

  VLOG(2) << "Retrying service adaptor connection in "
          << kServiceAdaptorRetryDelay;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DataAggregatorService::InitializeDeviceInfoEndpoint,
                     weak_ptr_factory_.GetWeakPtr(), num_tries + 1),
      kServiceAdaptorRetryDelay);
}

void DataAggregatorService::RequestDeviceInfo() {
  device_info_remote_->GetPolicyInfo(base::BindOnce(
      &DataAggregatorService::StorePolicyInfo, weak_ptr_factory_.GetWeakPtr()));
  // NB: we make 3 async calls here, but only the PolicyInfo data is
  // required for transporting payloads. The two calls below will fill
  // up a shared labels field that we'll use later, but we can start
  // collecting data without that info. If any payloads are sent to the
  // ERP endpoint without the labels populated, they will be populated
  // manually on the server side. This is merely a convenience for Fleet
  // to avoid performing this computation for every log.
  device_info_remote_->GetSysInfo(base::BindOnce(
      &DataAggregatorService::StoreSysInfo, weak_ptr_factory_.GetWeakPtr()));
  device_info_remote_->GetMachineStatisticsInfo(
      base::BindOnce(&DataAggregatorService::StoreMachineStatisticsInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DataAggregatorService::StorePolicyInfo(
    chromeos::cfm::mojom::PolicyInfoPtr policy_info) {
  // Only start collecting data if we have a device_id. Without a proper
  // ID, we can't upload logs to cloud logging, so the data is useless.
  if (!policy_info->device_id.has_value()) {
    LOG(ERROR)
        << "Unable to determine device ID! Cloud logging will be disabled.";
    return;
  }

  // Same with the robot email.
  if (!policy_info->service_account_email_address.has_value()) {
    LOG(ERROR)
        << "Unable to determine robot email! Cloud logging will be disabled.";
    base::UmaHistogramEnumeration(kSetupStatusMetricName,
                                  SetupStatus::kNoRobotEmailFound);
    return;
  }

  active_transport_payload_.set_permanent_id(policy_info->device_id.value());
  active_transport_payload_.set_robot_email(
      policy_info->service_account_email_address.value());
  shared_labels_[kRatchetDeviceIdLabel] = policy_info->device_id.value();
  shared_labels_[kRatchetEmailLabel] =
      policy_info->service_account_email_address.value();

  VLOG(1) << "Assigning device ID " << policy_info->device_id.value()
          << " and email "
          << policy_info->service_account_email_address.value();

  base::UmaHistogramEnumeration(kSetupStatusMetricName,
                                SetupStatus::kSetupSucceeded);

  InitializeLocalSources();
  StartFetchTimer();
}

void DataAggregatorService::StoreSysInfo(
    chromeos::cfm::mojom::SysInfoPtr sys_info) {
  if (sys_info->release_track.has_value()) {
    shared_labels_[kRatchetOsChannelLabel] = sys_info->release_track.value();
  }

  if (sys_info->release_version.has_value()) {
    shared_labels_[kRatchetOsVersionLabel] = sys_info->release_version.value();
  }

  if (sys_info->browser_version.has_value()) {
    shared_labels_[kRatchetChromeVersionLabel] =
        sys_info->browser_version.value();
  }
}

void DataAggregatorService::StoreMachineStatisticsInfo(
    chromeos::cfm::mojom::MachineStatisticsInfoPtr stat_info) {
  if (stat_info->hwid.has_value()) {
    shared_labels_[kRatchetHwidLabel] = stat_info->hwid.value();
  }
}

void DataAggregatorService::StartFetchTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Artemis started. Listening for data.";
  fetch_timer_.Start(
      FROM_HERE, fetch_frequency_,
      base::BindRepeating(&DataAggregatorService::FetchFromAllSourcesAndEnqueue,
                          weak_ptr_factory_.GetWeakPtr()));
}

/*
 * The upload process is a bit involved, so let's summarize:
 *
 * Note that `FetchFromAllSourcesAndEnqueue()` will be referred to as FetchAll()
 * for brevity.
 *
 * - We call FetchAll() on a repeated timer. This will make the async Fetch()
 *   requests for every data source we track.
 * - As data comes in from the async calls, we add it to the "active" payload,
 *   which is a reused payload object that collects data until the payload
 *   is ready, most commonly when it reaches a max size, at which point the
 *   data is copied to a new payload and the "active" payload is zero'ed out.
 * - The new payload mentioned above is pushed to our upload queue. If there is
 *   no enqueue currently in progress, we will also enqueue it to our reporting
 *   pipeline.
 * - Once an enqueue is initiated, we set an enqueue_in_progress_ bool and
 *   enter our enqueue routine. During this time, we will return early from all
 *   FetchAll() attempts until the enqueue succeeds.
 *        NOTE: despite cancelling future FetchAll requests, we may still get
 *        rolling responses from the async Fetch() calls that we already called.
 *        These will just be appended into the now-empty active payload.
 * - If the initial enqueue attempt fails, we will try again after N seconds,
 *   determined by a backoff timer. Fetches will continue to be halted during
 *   all retry attempts.
 * - Once the enqueue attempt succeeds, we pop the payload off our queue and
 *   check the queue for more data. If more is available, we'll immediately
 *   schedule another enqueue.
 * - FetchAll() calls will also be paused if the queue ever reaches the max
 *   size set by `payload_queue_max_size_`. This prevents us from needing to
 *   drop data to keep the memory footprint down.
 */
void DataAggregatorService::FetchFromAllSourcesAndEnqueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Wait for enqueue callback to fire before fetching more data.
  if (enqueue_in_progress_) {
    return;
  }

  // If the queue is full, halt fetches until we can catch up. Note that
  // a full queue implies that we've begun the enqueue process for the
  // first item, which will continue to attempt an enqueue until it
  // succeeds, at which point it will trigger the enqueue for the next
  // one. In other words, we should never reach a deadlocked state where
  // `Fetch()` calls are halted AND enqueues are halted.
  if (pending_transport_payloads_.size() >= payload_queue_max_size_) {
    LOG(WARNING) << "Payload queue is at capacity. Forgoing next fetch.";
    return;
  }

  VLOG(1) << "Fetching data from " << data_source_map_.size() << " sources.";

  for (const auto& data_source : data_source_map_) {
    std::string source_name = data_source.first;
    const auto& source_remote = data_source.second;

    auto append_callback =
        base::BindOnce(&DataAggregatorService::AppendEntriesToActivePayload,
                       weak_ptr_factory_.GetWeakPtr(), std::move(source_name));

    source_remote->Fetch(std::move(append_callback));
  }
}

void DataAggregatorService::AppendEntriesToActivePayload(
    const std::string& source_name,
    const std::vector<std::string>& serialized_entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (serialized_entries.empty()) {
    return;
  }

  if (VLOG_IS_ON(4)) {
    VLOG(4) << "Appending the following entries: ";
    for (auto& entry : serialized_entries) {
      VLOG(4) << entry;
    }
  }

  // TODO(b/336777241): use different payloads for different source types.
  // Using LogPayload for everything at this time.
  proto::LogPayload* log_payload =
      active_transport_payload_.mutable_log_payload();
  proto::LogSet* log_set = nullptr;

  // First check if we already have a LogSet for this data source.
  for (auto& set : *(log_payload->mutable_log_sets())) {
    if (set.log_source() == source_name) {
      log_set = &set;
      break;
    }
  }

  // If the current payload doesn't contain a LogSet for this data
  // source yet, create a new one.
  if (log_set == nullptr) {
    log_set = log_payload->add_log_sets();
  }

  google::protobuf::RepeatedPtrField<proto::LogEntry>* entries =
      log_set->mutable_entries();

  log_set->set_log_source(source_name);

  auto* labels = log_set->mutable_labels();
  labels->insert(shared_labels_.begin(), shared_labels_.end());

  // Deserialize the entries back into protos and append them to the payload.
  for (const auto& entry_str : serialized_entries) {
    proto::LogEntry entry;
    if (!entry.ParseFromString(entry_str)) {
      LOG(WARNING) << "Unable to parse entry. Dropping '" << entry_str << "'";
    } else {
      entries->Add(std::move(entry));
    }
  }

  if (DidActivePayloadReachMaxSize()) {
    VLOG(1) << "Payload is ready to be enqueued. Pushing to pending queue.";
    AddActivePayloadToPendingQueue();

    // Additionally, push the next payload to the wire if we aren't currently
    // enqueuing anything else.
    if (!enqueue_in_progress_) {
      EnqueueNextPendingTransportPayload();
    }
  }
}

bool DataAggregatorService::DidActivePayloadReachMaxSize() const {
  return active_transport_payload_.ByteSizeLong() >= payload_max_size_bytes_;
}

void DataAggregatorService::AddActivePayloadToPendingQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto timestamp =
      (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds();

  // We want to take the necessary data from the active payload and
  // create a new (equivalent) payload that we can push to our queue.
  // To avoid a large deep copy, steal the log pointer from the active
  // payload and reassign it to the new one.
  proto::TransportPayload pending_payload;
  pending_payload.set_permanent_id(active_transport_payload_.permanent_id());
  pending_payload.set_collection_timestamp_ms(timestamp);

  proto::LogPayload* curr_active_payload =
      active_transport_payload_.release_log_payload();
  pending_payload.set_allocated_log_payload(curr_active_payload);

  pending_transport_payloads_.push(std::move(pending_payload));

  base::UmaHistogramCounts100(kPayloadQueueSizeMetricName,
                              pending_transport_payloads_.size());

  VLOG(2) << "Pushed payload into pending queue. New size: "
          << pending_transport_payloads_.size();
}

void DataAggregatorService::EnqueueNextPendingTransportPayload() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Don't attempt to enqueue anything if we're still waiting for
  // a response from the last enqueue. We'll call this function
  // again immediately after getting a response (when we check
  // the pending queue size), so this will just delay this
  // enqueue attempt until then.
  if (enqueue_in_progress_) {
    return;
  }

  InitiateEnqueueRequest();
}

void DataAggregatorService::InitiateEnqueueRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pending_transport_payloads_.empty()) {
    LOG(WARNING) << "Requested payload enqueue, but payload queue is empty.";
    return;
  }

  base::UmaHistogramCounts1M(
      kEnqueuedPayloadSizeMetricName,
      pending_transport_payloads_.front().ByteSizeLong());

  auto enqueue_success_callback =
      base::BindOnce(&DataAggregatorService::HandleEnqueueResponse,
                     weak_ptr_factory_.GetWeakPtr());

  enqueue_in_progress_ = true;

  uploader_remote_->Enqueue(
      pending_transport_payloads_.front().SerializeAsString(),
      chromeos::cfm::mojom::EnqueuePriority::kLow,
      std::move(enqueue_success_callback));
}

void DataAggregatorService::HandleEnqueueResponse(
    chromeos::cfm::mojom::LoggerStatusPtr status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  enum LoggerResponse response;

  if (status->code == chromeos::cfm::mojom::LoggerErrorCode::kOk) {
    response = LoggerResponse::kOk;
  } else if (status->code ==
             chromeos::cfm::mojom::LoggerErrorCode::kOutOfRange) {
    response = LoggerResponse::kDeniedDueToThrottling;
  } else if (status->code ==
             chromeos::cfm::mojom::LoggerErrorCode::kUnauthenticated) {
    response = LoggerResponse::kUnauthenticated;
  } else if (status->code ==
             chromeos::cfm::mojom::LoggerErrorCode::kUnavailable) {
    response = LoggerResponse::kUnavailable;
  } else {
    response = LoggerResponse::kOther;
  }

  base::UmaHistogramEnumeration(kLoggerServiceResponseMetricName, response);

  if (status->code != chromeos::cfm::mojom::LoggerErrorCode::kOk) {
    enqueue_retry_backoff_.InformOfRequest(/*succeeded=*/false);
    auto retry_delay = enqueue_retry_backoff_.GetTimeUntilRelease();

    LOG(ERROR) << "Recent enqueue failed with error code: " << status->code
               << ". Trying again in " << retry_delay;

    // TODO(crbug.com/475558926): Remove when fixed.
    SYSLOG(ERROR) << "Recent enqueue failed with error code: " << status->code
                  << ". Trying again in " << retry_delay;

    current_enqueue_retries_++;
    base::UmaHistogramTimes(kTimeWaitedBeforeEnqueueRetryMetricName,
                            retry_delay);

    // Note: we call the helper directly here to force the attempt to go
    // through, despite `enqueue_in_progress_` being set. We can't unset
    // this var as we may get additional enqueue requests while we wait
    // to retry, and we want to decline these. Otherwise, we break the
    // backoff timer functionality of the retry.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DataAggregatorService::InitiateEnqueueRequest,
                       weak_ptr_factory_.GetWeakPtr()),
        retry_delay);
    return;
  }

  VLOG(1) << "Recent enqueue succeeded.";
  enqueue_retry_backoff_.Reset();

  // If the enqueue succeeded, Flush() all of the affected data sources so they
  // can update their internal pointers. Note that for non-incremental sources
  // this will likely just be a no-op.
  proto::LogPayload* log_payload =
      pending_transport_payloads_.front().mutable_log_payload();
  google::protobuf::RepeatedPtrField<proto::LogSet>* log_sets =
      log_payload->mutable_log_sets();

  for (const auto& log_set : *log_sets) {
    const auto& data_source = log_set.log_source();
    data_source_map_[data_source]->Flush();
  }

  base::UmaHistogramCounts1000(kNumberOfRetriesBeforeSuccessfulEnqueueMetricName,
                               current_enqueue_retries_);

  base::UmaHistogramTimes(kTimeSinceLastSuccessfulEnqueueMetricName,
                          base::TimeTicks::Now() - last_upload_time_);

  // Clean up.
  enqueue_in_progress_ = false;
  last_upload_time_ = base::TimeTicks::Now();
  current_enqueue_retries_ = 0;
  pending_transport_payloads_.pop();

  // Try another transfer if the queue is still populated.
  if (!pending_transport_payloads_.empty()) {
    VLOG(2) << "More payloads in queue; enqueueing.";
    EnqueueNextPendingTransportPayload();
  }
}

DataAggregatorService::DataAggregatorService()
    : service_adaptor_(mojom::DataAggregator::Name_, this),
      enqueue_retry_backoff_(&kEnqueueRetryBackoffPolicy) {
  CfmHotlineClient::Get()->AddObserver(this);

  DETACH_FROM_SEQUENCE(sequence_checker_);

  receivers_.set_disconnect_handler(base::BindRepeating(
      &DataAggregatorService::OnMojoDisconnect, base::Unretained(this)));

  local_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});

  local_task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(&PersistentDb::Initialize));

  // Cache configs.
  fetch_frequency_ = std::clamp(features::kFetchFrequency.Get(),
                                kMinimumFetchFrequency, kMaximumFetchFrequency);

  log_poll_frequency_ =
      std::clamp(features::kLogPollFrequency.Get(), kMinimumLogPollFrequency,
                 kMaximumLogPollFrequency);

  log_batch_size_ = std::clamp(features::kLogBatchSize.Get(),
                               kMinimumLogBatchSize, kMaximumLogBatchSize);

  payload_max_size_bytes_ =
      std::clamp(features::kPayloadMaxSizeBytes.Get(),
                 kMinimumPayloadMaxSizeBytes, kMaximumPayloadMaxSizeBytes);

  payload_queue_max_size_ =
      std::clamp(features::kPayloadQueueMaxSize.Get(),
                 kMinimumPayloadQueueMaxSize, kMaximumPayloadQueueMaxSize);

  VLOG(1) << "Starting Artemis with config: fetch_frequency = "
          << fetch_frequency_
          << ", log_poll_frequency = " << log_poll_frequency_
          << ", log_batch_size = " << log_batch_size_
          << ", payload_max_size_bytes = " << payload_max_size_bytes_
          << ", payload_queue_max_size = " << payload_queue_max_size_;

  InitializeUploadEndpoint(/*num_tries=*/0);
}

DataAggregatorService::~DataAggregatorService() {
  local_task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(&PersistentDb::Shutdown));
  CfmHotlineClient::Get()->RemoveObserver(this);
}

}  // namespace ash::cfm
