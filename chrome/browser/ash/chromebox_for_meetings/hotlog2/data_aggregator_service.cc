// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/data_aggregator_service.h"

#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/log_source.h"
#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/persistent_db.h"
#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/specialized_log_sources.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace ash::cfm {

namespace {

// Local convenience aliases
using mojom::DataFilter::FilterType::CHANGE;
using mojom::DataFilter::FilterType::REGEX;

static DataAggregatorService* g_data_aggregator_service = nullptr;

constexpr base::TimeDelta kFetchFrequency = base::Minutes(1);
constexpr size_t kDefaultLogBatchSize = 500;  // lines

constexpr base::TimeDelta kServiceAdaptorRetryDelay = base::Seconds(1);
constexpr size_t kServiceAdaptorRetryMaxTries = 5;

// List of commands that should be polled frequently. Any commands
// being watched by watchdogs should be here.
constexpr base::TimeDelta kDefaultCommandPollFrequency = base::Seconds(5);
const char* kLocalCommandSourcesFastPoll[] = {
    "ip -brief address",
    "lspci",
    "lsusb -t",
};

// List of commands that should be polled at a much slower frequency
// than the default. These are strictly for telemetry purposes in
// cloud logging and should be reserved for commands that don't need
// constant monitoring. Commands that are watched by a watchdog should
// NOT be in this list.
constexpr base::TimeDelta kExtendedCommandPollFrequency = base::Minutes(1);
const char* kLocalCommandSourcesSlowPoll[] = {
    "df -h",
    "free -m",
    // Hide kernelspace processes and show limited columns.
    "ps -o pid,user,group,args --ppid 2 -p 2 -N --sort=pid",
};

constexpr base::TimeDelta kDefaultLogPollFrequency = base::Seconds(10);
const char* kLocalLogSources[] = {
    kCfmAuditLogFile,  kCfmBiosInfoLogFile,     kCfmChromeLogFile,
    kCfmCrosEcLogFile, kCfmEventlogLogFile,     kCfmFwupdLogFile,
    kCfmLacrosLogFile, kCfmPowerdLogFile,       kCfmSyslogLogFile,
    kCfmUiLogFile,     kCfmUpdateEngineLogFile, kCfmVariationsListLogFile,
};

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
             const std::string& command, const base::TimeDelta& poll_freq) {
            auto source = std::make_unique<CommandSource>(command, poll_freq);
            source->StartCollectingData();

            mojo::MakeSelfOwnedReceiver(std::move(source),
                                        std::move(pending_receiver));
          },
          remote.BindNewPipeAndPassReceiver(), command, poll_freq));

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
             const std::string& filepath) {
            auto source = LogSource::Create(filepath, kDefaultLogPollFrequency,
                                            kDefaultLogBatchSize);
            source->StartCollectingData();

            mojo::MakeSelfOwnedReceiver(std::move(source),
                                        std::move(pending_receiver));
          },
          remote.BindNewPipeAndPassReceiver(), filepath));

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
  VLOG(3) << "mojom::DataAggregator disconnected";
}

void DataAggregatorService::InitializeLocalSources() {
  // Add local command sources
  for (auto* const cmd : kLocalCommandSourcesFastPoll) {
    AddLocalCommandSource(cmd, kDefaultCommandPollFrequency);
  }

  for (auto* const cmd : kLocalCommandSourcesSlowPoll) {
    AddLocalCommandSource(cmd, kExtendedCommandPollFrequency);
  }

  // Add local log file sources
  for (auto* const logfile : kLocalLogSources) {
    AddLocalLogSource(logfile);
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
  VLOG(3) << "Uploader RequestBindService result: " << success
          << " for interface: " << interface_name;

  if (success) {
    InitializeDeviceInfoEndpoint(/*num_tries=*/0);
    return;
  }

  if (num_tries >= kServiceAdaptorRetryMaxTries) {
    LOG(ERROR) << "Retry limit reached for connecting to " << interface_name
               << ". Remote calls will fail.";
    return;
  }

  VLOG(3) << "Retrying service adaptor connection in "
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
  VLOG(3) << "DeviceInfo RequestBindService result: " << success
          << " for interface: " << interface_name;

  if (success) {
    RequestDeviceId();
    return;
  }

  if (num_tries >= kServiceAdaptorRetryMaxTries) {
    LOG(ERROR) << "Retry limit reached for connecting to " << interface_name
               << ". Remote calls will fail.";
    return;
  }

  VLOG(3) << "Retrying service adaptor connection in "
          << kServiceAdaptorRetryDelay;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DataAggregatorService::InitializeDeviceInfoEndpoint,
                     weak_ptr_factory_.GetWeakPtr(), num_tries + 1),
      kServiceAdaptorRetryDelay);
}

void DataAggregatorService::RequestDeviceId() {
  device_info_remote_->GetPolicyInfo(base::BindOnce(
      &DataAggregatorService::StoreDeviceId, weak_ptr_factory_.GetWeakPtr()));
}

void DataAggregatorService::StoreDeviceId(
    chromeos::cfm::mojom::PolicyInfoPtr policy_info) {
  // Only start collecting data if we have a device_id. Without a proper
  // ID, we can't upload logs to cloud logging, so the data is useless.
  if (policy_info->device_id.has_value()) {
    device_id_ = policy_info->device_id.value();
    VLOG(4) << "Assigning device ID " << device_id_;
    StartFetchTimer();
  }
}

void DataAggregatorService::StartFetchTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fetch_timer_.Start(
      FROM_HERE, kFetchFrequency,
      base::BindRepeating(&DataAggregatorService::FetchFromAllSourcesAndEnqueue,
                          weak_ptr_factory_.GetWeakPtr()));
}

void DataAggregatorService::FetchFromAllSourcesAndEnqueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& data_source : data_source_map_) {
    std::string source_name = data_source.first;
    const auto& source_remote = data_source.second;

    auto enqueue_callback =
        base::BindOnce(&DataAggregatorService::EnqueueData,
                       weak_ptr_factory_.GetWeakPtr(), std::move(source_name));

    source_remote->Fetch(std::move(enqueue_callback));
  }
}

void DataAggregatorService::EnqueueData(
    const std::string& source_name,
    const std::vector<std::string>& serialized_entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (serialized_entries.empty()) {
    return;
  }

  if (VLOG_IS_ON(4)) {
    VLOG(4) << "Enqueuing the following entries: ";
    for (auto& entry : serialized_entries) {
      VLOG(4) << entry;
    }
  }

  // TODO(b/340913913): each data source will produce one TransportPayload
  // per call to Fetch(). We should instead combine the logs of multiple
  // sources into a single payload to reduce QPS.
  proto::TransportPayload transport_payload;
  WrapEntriesInTransportPayload(source_name, serialized_entries,
                                &transport_payload);

  auto enqueue_success_callback =
      base::BindOnce(&DataAggregatorService::HandleEnqueueResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(source_name));

  // TODO(b/339455254): have each data source specify a priority instead
  // of assuming kLow for every enqueue.
  uploader_remote_->Enqueue(transport_payload.SerializeAsString(),
                            chromeos::cfm::mojom::EnqueuePriority::kLow,
                            std::move(enqueue_success_callback));
}

void DataAggregatorService::WrapEntriesInTransportPayload(
    const std::string& source_name,
    const std::vector<std::string>& serialized_entries,
    proto::TransportPayload* transport_payload) {
  // TODO(b/336777241): use different payloads for different source types.
  // Using LogPayload for everything at this time.
  proto::LogPayload* log_payload = transport_payload->mutable_log_payload();
  proto::LogSet* log_set = log_payload->add_log_sets();
  google::protobuf::RepeatedPtrField<proto::LogEntry>* entries =
      log_set->mutable_entries();

  log_set->set_log_source(source_name);

  // Deserialize the entries back into protos and append them to the payload.
  for (const auto& entry_str : serialized_entries) {
    proto::LogEntry entry;
    if (!entry.ParseFromString(entry_str)) {
      LOG(WARNING) << "Unable to parse entry. Dropping '" << entry_str << "'";
    } else {
      entries->Add(std::move(entry));
    }
  }

  auto timestamp =
      (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds();

  transport_payload->set_collection_timestamp_ms(timestamp);
  transport_payload->set_permanent_id(device_id_);
}

void DataAggregatorService::HandleEnqueueResponse(
    const std::string& source_name,
    chromeos::cfm::mojom::LoggerStatusPtr status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status->code != chromeos::cfm::mojom::LoggerErrorCode::kOk) {
    LOG(ERROR) << "Recent enqueue for source '" << source_name
               << "' failed with error code: " << status->code
               << ". Trying again in " << kFetchFrequency;
    return;
  }

  CHECK(data_source_map_.count(source_name) != 0)
      << "Enqueued records for data source " << source_name
      << ", but it no longer exists?";

  // If the enqueue succeeded, tell the data source so it can
  // update its internal pointers. Note that for non-incremental
  // sources this will likely just be a no-op.
  data_source_map_[source_name]->Flush();
}

DataAggregatorService::DataAggregatorService()
    : service_adaptor_(mojom::DataAggregator::Name_, this) {
  CfmHotlineClient::Get()->AddObserver(this);

  DETACH_FROM_SEQUENCE(sequence_checker_);

  receivers_.set_disconnect_handler(base::BindRepeating(
      &DataAggregatorService::OnMojoDisconnect, base::Unretained(this)));

  local_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});

  local_task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(&PersistentDb::Initialize));

  InitializeUploadEndpoint(/*num_tries=*/0);
  InitializeLocalSources();
}

DataAggregatorService::~DataAggregatorService() {
  local_task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(&PersistentDb::Shutdown));
  CfmHotlineClient::Get()->RemoveObserver(this);
}

}  // namespace ash::cfm
