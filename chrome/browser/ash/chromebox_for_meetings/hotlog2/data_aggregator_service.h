// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_DATA_AGGREGATOR_SERVICE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_DATA_AGGREGATOR_SERVICE_H_

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/command_source.h"
#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/log_source.h"
#include "chrome/browser/ash/chromebox_for_meetings/service_adaptor.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_observer.h"
#include "chromeos/ash/services/chromebox_for_meetings/public/mojom/meet_devices_data_aggregator.mojom-shared.h"
#include "chromeos/ash/services/chromebox_for_meetings/public/mojom/meet_devices_data_aggregator.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::cfm {

// This service manages the aggregation of data from one or more
// DataSources, as well as "processing" the data, which includes
// uploading the data to an external server (eg for cloud logging),
// and adding watchdogs to any data source for on-demand monitoring.
// This is also the class that exposes its API over hotline for
// external clients to communicate with.
class DataAggregatorService : public CfmObserver,
                              public ServiceAdaptor::Delegate,
                              public mojom::DataAggregator {
 public:
  DataAggregatorService(const DataAggregatorService&) = delete;
  DataAggregatorService& operator=(const DataAggregatorService&) = delete;

  // Manage singleton instance.
  static void Initialize();
  static void Shutdown();
  static DataAggregatorService* Get();
  static bool IsInitialized();

 protected:
  // CfmObserver:
  bool ServiceRequestReceived(const std::string& interface_name) override;

  // ServiceAdaptorDelegate:
  void OnAdaptorDisconnect() override;
  void OnBindService(mojo::ScopedMessagePipeHandle receiver_pipe) override;

  // mojom::DataAggregator implementation
  void GetDataSourceNames(GetDataSourceNamesCallback callback) override;
  void AddDataSource(const std::string& source_name,
                     mojo::PendingRemote<mojom::DataSource> new_data_source,
                     AddDataSourceCallback callback) override;
  void AddWatchDog(const std::string& source_name,
                   mojom::DataFilterPtr filter,
                   mojo::PendingRemote<mojom::DataWatchDog> watch_dog,
                   AddWatchDogCallback callback) override;

  // Disconnect handler for |mojom::DataAggregator|
  virtual void OnMojoDisconnect();

 private:
  DataAggregatorService();
  ~DataAggregatorService() override;

  void AddLocalCommandSource(const std::string& command);
  void OnLocalCommandDisconnect(const std::string& command);
  void StartFetchTimer();
  void FetchFromAllSourcesAndEnqueue();
  void EnqueueData(const std::string& source_name,
                   const std::vector<std::string>& serialized_records);
  void HandleEnqueueResponse(const std::string& source_name, bool success);

  ServiceAdaptor service_adaptor_;
  mojo::ReceiverSet<mojom::DataAggregator> receivers_;

  base::RepeatingTimer fetch_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Worker thread for locally created DataSources
  scoped_refptr<base::SequencedTaskRunner> local_task_runner_;

  // Maps DataSource names to their remotes, for access convenience
  std::map<std::string, mojo::Remote<mojom::DataSource>> data_source_map_;

  // Must be the last class member.
  base::WeakPtrFactory<DataAggregatorService> weak_ptr_factory_{this};
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_HOTLOG2_DATA_AGGREGATOR_SERVICE_H_
