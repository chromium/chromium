// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_H_

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/dns_sd_delegate.h"
#include "chrome/browser/media/router/discovery/mdns/dns_sd_registry.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/discovery/media_sink_service_util.h"
#include "components/prefs/pref_change_registrar.h"

namespace media_router {

// A service which can be used to start background discovery and resolution of
// Cast devices. It is owned by a singleton that is never freed.
// This class is not thread safe. All methods must be invoked on the UI thread.
// TODO(imcheng): Consider removing this class and moving the logic into a part
// of CastMediaSinkServiceImpl that runs on the UI thread, and renaming
// CastMediaSinkServiceImpl to CastMediaSinkService. Longer term, we
// should look into eliminating dependencies on the UI thread.
class CastMediaSinkService : public DnsSdRegistry::DnsSdObserver {
 public:
  CastMediaSinkService();

  CastMediaSinkService(const CastMediaSinkService&) = delete;
  CastMediaSinkService& operator=(const CastMediaSinkService&) = delete;

  ~CastMediaSinkService() override;

  // Starts Cast sink discovery. No-ops if already started.
  // `sink_discovery_cb`: Callback to invoke when the list of discovered sinks
  // has been updated.
  // `discovery_permission_rejected_cb`: Callback to invoke when the DnsSd
  // discovery fails due to permission rejected.
  // `dial_media_sink_service`: Optional pointer to DIAL MediaSinkService for
  // dual discovery. Marked virtual for tests.
  virtual void Initialize(
      const OnSinksDiscoveredCallback& sinks_discovered_cb,
      base::RepeatingClosure discovery_permission_rejected_cb,
      MediaSinkServiceBase* dial_media_sink_service);

  virtual void DiscoverSinksNow();

  // Resets `local_state_change_registrar_` and thus stops propagating changes
  // to the allow all IPS pref.
  void StopObservingPrefChanges();

  // Marked virtual for tests.
  virtual std::unique_ptr<CastMediaSinkServiceImpl, base::OnTaskRunnerDeleter>
  CreateImpl(const OnSinksDiscoveredCallback& sinks_discovered_cb,
             MediaSinkServiceBase* dial_media_sink_service);

  CastMediaSinkServiceImpl* impl() { return impl_.get(); }

  // Registers with DnsSdRegistry to listen for Cast devices. Called when users
  // make an explicit interaction with Cast (e.g. opening the Media Router
  // dialog). On windows, this method is also called when the browser is
  // informed that users have granted mDNS permission.
  // Subsequent invocations of this method are no-op.
  // Marked virtual for tests.
  virtual void StartMdnsDiscovery();

  bool MdnsDiscoveryStarted() const;

  void SetDnsSdRegistryForTest(DnsSdRegistry* registry);

 private:
  friend class CastMediaSinkServiceTest;

  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceTest, TestRestartAfterStop);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceTest, TestMultipleStartAndStop);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceTest, TestOnDnsSdEvent);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceTest, TestTimer);

  void RunSinksDiscoveredCallback(
      const OnSinksDiscoveredCallback& sinks_discovered_cb,
      std::vector<MediaSinkInternal> sinks);

  // DnsSdRegistry::DnsSdObserver implementation
  void OnDnsSdEvent(const std::string& service_type,
                    const DnsSdRegistry::DnsSdServiceList& services) override;
  void OnDnsSdPermissionRejected() override;

  // Sets the current value of |CastAllowAllIPs()| on |impl_|.
  void SetCastAllowAllIPs();

  // Raw pointer to DnsSdRegistry instance, which is a global leaky singleton
  // and lives as long as the browser process.
  raw_ptr<DnsSdRegistry> dns_sd_registry_ = nullptr;

  // Created on the UI thread, used and destroyed on its SequencedTaskRunner.
  std::unique_ptr<CastMediaSinkServiceImpl, base::OnTaskRunnerDeleter> impl_;

  // Listens for local state pref changes for kMediaRouterCastAllowAllIPs.
  PrefChangeRegistrar local_state_change_registrar_;

  // List of cast sinks found in current round of mDNS discovery.
  std::vector<MediaSinkInternal> cast_sinks_;

  // Invoked when `OnDnsSdPermissionRejected()` is called.
  base::RepeatingClosure discovery_permission_rejected_cb_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CastMediaSinkService> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_H_
