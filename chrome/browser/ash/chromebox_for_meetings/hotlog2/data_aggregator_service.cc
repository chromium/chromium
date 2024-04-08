// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/data_aggregator_service.h"

#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace ash::cfm {

static DataAggregatorService* g_data_aggregator_service = nullptr;

constexpr base::TimeDelta kFetchFrequency = base::Seconds(30);
constexpr base::TimeDelta kDefaultCommandPollFrequency = base::Seconds(5);

// static
void DataAggregatorService::Initialize() {
  CHECK(!g_data_aggregator_service);
  g_data_aggregator_service = new DataAggregatorService();
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

void DataAggregatorService::AddLocalCommandSource(const std::string& command) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(data_source_map_.count(command) == 0)
      << "Local command '" << command << "' was added twice.";

  mojo::Remote<mojom::DataSource> remote;
  local_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](mojo::PendingReceiver<mojom::DataSource> pending_receiver,
             const std::string& command) {
            mojo::MakeSelfOwnedReceiver(
                std::make_unique<CommandSource>(command,
                                                kDefaultCommandPollFrequency),
                std::move(pending_receiver));
          },
          remote.BindNewPipeAndPassReceiver(), command));

  remote.set_disconnect_handler(
      base::BindOnce(&DataAggregatorService::OnLocalCommandDisconnect,
                     base::Unretained(this), command));

  data_source_map_[command] = std::move(remote);
}

void DataAggregatorService::OnLocalCommandDisconnect(
    const std::string& command) {
  // This is unlikely, but if one of our local remotes disconnects,
  // just request to re-add it. The pointers in our local maps will
  // be overridden, and the old objects will be destroyed.
  LOG(WARNING) << "Local DataSource for '" << command << "' has disconnected; "
               << "attempting to reconnect.";
  data_source_map_.erase(command);
  AddLocalCommandSource(command);
}

void DataAggregatorService::OnMojoDisconnect() {
  VLOG(3) << "mojom::DataAggregator disconnected";
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
    const std::vector<std::string>& serialized_records) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(b/326441003): this function will need to be replaced
  // with an async call to the uploader. For now, assume success
  // and just print the data.
  bool success = true;

  VLOG(4) << "Enqueuing the following records: ";
  for (auto& record : serialized_records) {
    VLOG(4) << record;
  }

  // TODO(b/326441003): this will eventually be a callback function
  // for the async upload transaction.
  HandleEnqueueResponse(std::move(source_name), success);
}

void DataAggregatorService::HandleEnqueueResponse(
    const std::string& source_name,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
    LOG(ERROR) << "Recent enqueue for source '" << source_name
               << "' failed. Trying again in " << kFetchFrequency;
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

  // Add local command sources
  std::vector<std::string> cmd_sources = {"ip -brief address"};
  for (const auto& cmd : cmd_sources) {
    AddLocalCommandSource(cmd);
  }

  StartFetchTimer();
}

DataAggregatorService::~DataAggregatorService() {
  CfmHotlineClient::Get()->RemoveObserver(this);
}

}  // namespace ash::cfm
