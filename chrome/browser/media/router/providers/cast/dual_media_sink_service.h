// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_DUAL_MEDIA_SINK_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_DUAL_MEDIA_SINK_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/common/media_router/discovery/media_sink_internal.h"
#include "chrome/common/media_router/media_source.h"
#include "url/origin.h"

namespace media_router {

class CastAppDiscoveryService;
class CastMediaSinkService;
class DialMediaSinkService;
class DialMediaSinkServiceImpl;
class MediaSinkServiceBase;

// This class uses DialMediaSinkService and CastMediaSinkService to discover
// sinks used by the Cast MediaRouteProvider. It also encapsulates the setup
// necessary to enable dual discovery on Dial/CastMediaSinkService.
// All methods must be called on the UI thread.
class DualMediaSinkService {
 public:
  // Arg 0: Provider name ("dial" or "cast").
  // Arg 1: List of sinks for the provider.
  using OnSinksDiscoveredProviderCallback =
      base::RepeatingCallback<void(const std::string&,
                                   const std::vector<MediaSinkInternal>&)>;
  using OnSinksDiscoveredProviderCallbackList =
      base::CallbackList<void(const std::string&,
                              const std::vector<MediaSinkInternal>&)>;
  using Subscription =
      std::unique_ptr<OnSinksDiscoveredProviderCallbackList::Subscription>;

  // Returns the lazily-created leaky singleton instance.
  static DualMediaSinkService* GetInstance();
  static void SetInstanceForTest(DualMediaSinkService* instance_for_test);

  // Returns the current list of sinks, keyed by provider name.
  const base::flat_map<std::string, std::vector<MediaSinkInternal>>&
  current_sinks() {
    return current_sinks_;
  }

  // Used by DialMediaRouteProvider only.
  DialMediaSinkServiceImpl* GetDialMediaSinkServiceImpl();

  // Used by CastMediaRouteProvider only.
  MediaSinkServiceBase* GetCastMediaSinkServiceImpl();

  CastAppDiscoveryService* cast_app_discovery_service() {
    return cast_app_discovery_service_.get();
  }

  // Adds |callback| to be notified when the list of discovered sinks changes.
  // The caller is responsible for destroying the returned Subscription when it
  // no longer wishes to receive updates.
  Subscription AddSinksDiscoveredCallback(
      const OnSinksDiscoveredProviderCallback& callback);

  virtual void OnUserGesture();

  // Starts mDNS discovery on |cast_media_sink_service_| if it is not already
  // started.
  virtual void StartMdnsDiscovery();

 protected:
  // Used by tests.
  DualMediaSinkService(
      std::unique_ptr<CastMediaSinkService> cast_media_sink_service,
      std::unique_ptr<DialMediaSinkService> dial_media_sink_service);
  virtual ~DualMediaSinkService();

 private:
  friend class DualMediaSinkServiceTest;
  FRIEND_TEST_ALL_PREFIXES(DualMediaSinkServiceTest,
                           AddSinksDiscoveredCallback);
  template <bool>
  friend class MediaRouterDesktopTestBase;
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest, ProvideSinks);

  static DualMediaSinkService* instance_for_test_;

  friend struct std::default_delete<DualMediaSinkService>;

  DualMediaSinkService();

  void OnSinksDiscovered(const std::string& provider_name,
                         std::vector<MediaSinkInternal> sinks);

  // Note: Dual discovery logic assumes |dial_media_sink_service_| outlives
  // |cast_media_sink_service_|.
  std::unique_ptr<DialMediaSinkService> dial_media_sink_service_;
  std::unique_ptr<CastMediaSinkService> cast_media_sink_service_;
  std::unique_ptr<CastAppDiscoveryService> cast_app_discovery_service_;

  OnSinksDiscoveredProviderCallbackList sinks_discovered_callbacks_;
  base::flat_map<std::string, std::vector<MediaSinkInternal>> current_sinks_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(DualMediaSinkService);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_DUAL_MEDIA_SINK_SERVICE_H_
