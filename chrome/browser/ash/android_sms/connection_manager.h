// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ANDROID_SMS_CONNECTION_MANAGER_H_
#define CHROME_BROWSER_ASH_ANDROID_SMS_CONNECTION_MANAGER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/android_sms/android_sms_app_manager.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "content/public/browser/service_worker_context_observer.h"

class Profile;

namespace content {
class ServiceWorkerContext;
}  // namespace content

namespace ash {
namespace android_sms {

class ConnectionEstablisher;

// ConnectionManager checks to see when the user has no Android Messages for Web
// pages open (in a web page or PWA) and notifies the corresponding
// service worker to start a background connection.
//
// This class is an observer for Android Messages for Web service worker
// lifecycle events and uses ConnectionEstablisher to initiate a background
// connection with Tachyon server when appropriate. Specifically, in the
// following cases
// 1. Startup.
// 2. Activation: This occurs on service worker update, when a new updated
//    version of the service worker replaces the current active version or on
//    service worker cold start.
// 3. NoControllees: When all pages controlled by the service worker are closed.
// The connection establishment may be attempted in more cases than actually
// required. It's left to the service worker to ignore or establish a background
// connection as required. E.g., The service worker will not establish a
// connection if it's is already connected to the Android Messages for Web page
// or if a connection already exists.
class ConnectionManager
    : public content::ServiceWorkerContextObserver,
      public AndroidSmsAppManager::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  ConnectionManager(
      std::unique_ptr<ConnectionEstablisher> connection_establisher,
      Profile* profile,
      AndroidSmsAppManager* android_sms_app_manager,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);

  ConnectionManager(const ConnectionManager&) = delete;
  ConnectionManager& operator=(const ConnectionManager&) = delete;

  ~ConnectionManager() override;

  // Sends a start connection message to the service worker.
  void StartConnection();

 private:
  friend class ConnectionManagerTest;

  // Thin wrapper for fetching ServiceWorkerContexts; overridden in tests.
  class ServiceWorkerProvider {
   public:
    ServiceWorkerProvider();

    ServiceWorkerProvider(const ServiceWorkerProvider&) = delete;
    ServiceWorkerProvider& operator=(const ServiceWorkerProvider&) = delete;

    virtual ~ServiceWorkerProvider();

    virtual content::ServiceWorkerContext* Get(const GURL& url,
                                               Profile* profile);
  };

  ConnectionManager(
      std::unique_ptr<ConnectionEstablisher> connection_establisher,
      Profile* profile,
      AndroidSmsAppManager* android_sms_app_manager,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      std::unique_ptr<ServiceWorkerProvider> service_worker_provider);

  // content::ServiceWorkerContextObserver:
  void OnVersionActivated(int64_t version_id, const GURL& scope) override;
  void OnVersionRedundant(int64_t version_id, const GURL& scope) override;
  void OnNoControllees(int64_t version_id, const GURL& scope) override;

  // AndroidSmsAppManager::Observer:
  void OnInstalledAppUrlChanged() override;

  // multidevice_setup::MultideviceSetupClient::Observer:
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_state_map) override;

  void UpdateConnectionStatus();
  std::optional<GURL> GenerateEnabledPwaUrl();
  content::ServiceWorkerContext* GetCurrentServiceWorkerContext();

  void SetServiceWorkerProviderForTesting(
      std::unique_ptr<ServiceWorkerProvider> service_worker_provider);

  std::unique_ptr<ConnectionEstablisher> connection_establisher_;
  raw_ptr<Profile> profile_;
  raw_ptr<AndroidSmsAppManager> android_sms_app_manager_;
  raw_ptr<multidevice_setup::MultiDeviceSetupClient> multidevice_setup_client_;

  std::unique_ptr<ServiceWorkerProvider> service_worker_provider_;

  // Version ID of the Android Messages for Web service worker that's currently
  // active i.e., capable of handling messages and controlling pages.
  std::optional<int64_t> active_version_id_;

  // Version ID of the previously active Android Messages for Web
  // service worker.
  std::optional<int64_t> prev_active_version_id_;

  // The URL of the Android Messages PWA, if it is currently enabled. If the
  // feature is not currently enabled, this field is null.
  std::optional<GURL> enabled_pwa_url_;
};

}  // namespace android_sms
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ANDROID_SMS_CONNECTION_MANAGER_H_
