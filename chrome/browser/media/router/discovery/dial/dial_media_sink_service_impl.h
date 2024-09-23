// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_IMPL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_IMPL_H_

#include <memory>
#include <set>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media/router/discovery/dial/device_description_service.h"
#include "chrome/browser/media/router/discovery/dial/dial_app_discovery_service.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service.h"
#include "chrome/browser/media/router/discovery/dial/dial_registry.h"
#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"
#include "components/media_router/common/discovery/media_sink_service_base.h"
#include "components/media_router/common/discovery/media_sink_service_util.h"

namespace media_router {

class DeviceDescriptionService;
class DialRegistry;

// A service which can be used to start background discovery and resolution of
// DIAL devices (Smart TVs, Game Consoles, etc.). It is indirectly owned by a
// singleton that is never freed. It may be created on any thread. All methods,
// unless otherwise noted, must be invoked on the SequencedTaskRunner given by
// |task_runner()|.
class DialMediaSinkServiceImpl : public MediaSinkServiceBase,
                                 public DialRegistry::Client {
 public:
  // Callbacks invoked when the list of available sinks for |app_name| changes.
  // The client can call |GetAvailableSinks()| to obtain the latest sink list.
  // |app_name|: app name, e.g. YouTube.
  // TODO(imcheng): Move sink query logic into DialAppDiscoveryService and
  // have it use MediaSinkServiceBase::Observer to observe sinks.
  using SinkQueryByAppCallbackList =
      base::RepeatingCallbackList<void(const std::string&)>;
  using SinkQueryByAppCallback = SinkQueryByAppCallbackList::CallbackType;

  // Represents DIAL app status on receiver device.
  enum SinkAppStatus { kUnknown = 0, kAvailable, kUnavailable };

  // |on_sinks_discovered_cb|: Callback for MediaSinkServiceBase.
  // Note that both callbacks are invoked on |task_runner|.
  // |task_runner|: The SequencedTaskRunner this class runs in.
  DialMediaSinkServiceImpl(
      const OnSinksDiscoveredCallback& on_sinks_discovered_cb,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~DialMediaSinkServiceImpl() override;

  virtual void Initialize();

  void StartDiscovery();

  // MediaSinkServiceBase implementation.
  void DiscoverSinksNow() override;

  // Returns the SequencedTaskRunner that should be used to invoke methods on
  // this instance. Can be invoked on any thread.
  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

  virtual DialAppDiscoveryService* app_discovery_service();

  // Registers |callback| to callback list entry in |sink_queries_|, with the
  // key |app_name|. Caller owns the returned subscription and is responsible
  // for destroying when it wants to unregister |callback|. Marked virtual for
  // tests.
  virtual base::CallbackListSubscription StartMonitoringAvailableSinksForApp(
      const std::string& app_name,
      const SinkQueryByAppCallback& callback);

  // Returns the current list of sinks compatible with |app_name|. The caller
  // can call this method after calling |StartMonitoringAvailableSinksForApp()|
  // to obtain the initial list, or when the callback fires to get the updated
  // list.
  // Marked virtual for tests.
  virtual std::vector<MediaSinkInternal> GetAvailableSinks(
      const std::string& app_name) const;

 protected:
  void SetDescriptionServiceForTest(
      std::unique_ptr<DeviceDescriptionService> description_service);
  void SetAppDiscoveryServiceForTest(
      std::unique_ptr<DialAppDiscoveryService> app_discovery_service);

 private:
  friend class DialMediaSinkServiceImplTest;
  friend class MockDialMediaSinkServiceImpl;
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           OnDeviceDescriptionRestartsTimer);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           OnDialDeviceListRestartsTimer);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           OnDeviceDescriptionAvailable);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           OnDeviceDescriptionAvailableIPAddressChanged);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           StartStopMonitoringAvailableSinksForApp);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           OnDialAppInfoAvailableNoStartMonitoring);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           OnDialAppInfoAvailableNoSink);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           OnDialAppInfoAvailableSinksAdded);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           OnDialAppInfoAvailableSinksRemoved);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           OnDialAppInfoAvailableWithAlreadyAvailableSinks);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           StartAfterStopMonitoringForApp);
  FRIEND_TEST_ALL_PREFIXES(DialMediaSinkServiceImplTest,
                           FetchDialAppInfoWithDiscoveryOnlySink);

  // DialRegistry::Client implementation
  void OnDialDeviceList(const DialRegistry::DeviceList& devices) override;
  void OnDialError(DialRegistry::DialErrorCode type) override;

  // Called when description service successfully fetches and parses device
  // description XML. Restart |finish_timer_| if it is not running.
  void OnDeviceDescriptionAvailable(
      const DialDeviceData& device_data,
      const ParsedDialDeviceDescription& description_data);

  // Called when fails to fetch or parse device description XML.
  void OnDeviceDescriptionError(const DialDeviceData& device,
                                const std::string& error_message);

  // Called when app discovery service finishes fetching and parsing app info
  // XML.
  void OnAppInfoParseCompleted(const std::string& sink_id,
                               const std::string& app_name,
                               DialAppInfoResult result);

  // Queries app status of |app_name| on |dial_sink|.
  void FetchAppInfoForSink(const MediaSinkInternal& dial_sink,
                           const std::string& app_name);

  // Issues HTTP request to get status of all registered apps on current sinks.
  void RescanAppInfo();

  // Helper function to get app status from |app_statuses_|.
  SinkAppStatus GetAppStatus(const std::string& sink_id,
                             const std::string& app_name) const;

  // Helper function to set app status in |app_statuses_|.
  void SetAppStatus(const std::string& sink_id,
                    const std::string& app_name,
                    SinkAppStatus app_status);

  void MaybeRemoveSinkQueryCallbackList(
      const std::string& app_name,
      SinkQueryByAppCallbackList* callback_list);

  // MediaSinkServiceBase implementation.
  void OnDiscoveryComplete() override;
  void RecordDeviceCounts() override;

  // Initialized in |Start()|.
  std::unique_ptr<DialRegistry> dial_registry_;

  // Initialized in |Start()|.
  std::unique_ptr<DeviceDescriptionService> description_service_;

  // Initialized in |Start()|.
  std::unique_ptr<DialAppDiscoveryService> app_discovery_service_;

  // Device data list from current round of discovery.
  DialRegistry::DeviceList current_devices_;

  // Sinks that are added during the latest round of discovery. In
  // |OnDiscoveryCompleted()| this will be merged into
  // |MediaSinkServiceBase::sinks_| and then cleared.
  base::flat_map<MediaSink::Id, MediaSinkInternal> latest_sinks_;

  // Map of app status, keyed by <sink id:app name>.
  base::flat_map<std::string, SinkAppStatus> app_statuses_;

  // Set of sink queries keyed by app name.
  base::flat_map<std::string, std::unique_ptr<SinkQueryByAppCallbackList>>
      sink_queries_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DialDeviceCountMetrics metrics_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_MEDIA_SINK_SERVICE_IMPL_H_
