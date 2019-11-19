// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_EXTENSION_EVENT_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_EXTENSION_EVENT_OBSERVER_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "extensions/browser/extension_host_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"

class Profile;

namespace extensions {
class ExtensionHost;
}

namespace chromeos {

// This class listens for extension events that should potentially keep the
// system awake while they are being processed.  Examples include push messages
// that arrive from Google's GCM servers and network requests initiated by
// extensions while processing the push messages.  This class is owned by
// WakeOnWifiManager.
class ExtensionEventObserver : public ProfileManagerObserver,
                               public extensions::ProcessManagerObserver,
                               public extensions::ExtensionHostObserver,
                               public PowerManagerClient::Observer {
 public:
  class TestApi {
   public:
    ~TestApi();

    // Runs |suspend_readiness_callback_| if it is non-null and then resets it.
    // Returns true iff it actually ran the callback.
    bool MaybeRunSuspendReadinessCallback();

    // Returns true if the ExtensionEventObserver will delay suspend attempts
    // for |host| if host has pending push messages or network requests.
    bool WillDelaySuspendForExtensionHost(extensions::ExtensionHost* host);

   private:
    friend class ExtensionEventObserver;

    explicit TestApi(base::WeakPtr<ExtensionEventObserver> parent);

    base::WeakPtr<ExtensionEventObserver> parent_;

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  ExtensionEventObserver();
  ~ExtensionEventObserver() override;

  std::unique_ptr<TestApi> CreateTestApi();

  // Called by the WakeOnWifiManager to control whether the
  // ExtensionEventObserver should or should not delay the system suspend.
  void SetShouldDelaySuspend(bool should_delay);

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // extensions::ProcessManagerObserver:
  void OnBackgroundHostCreated(extensions::ExtensionHost* host) override;
  void OnProcessManagerShutdown(extensions::ProcessManager* manager) override;

  // extensions::ExtensionHostObserver:
  void OnExtensionHostDestroyed(const extensions::ExtensionHost* host) override;
  void OnBackgroundEventDispatched(const extensions::ExtensionHost* host,
                                   const std::string& event_name,
                                   int event_id) override;
  void OnBackgroundEventAcked(const extensions::ExtensionHost* host,
                              int event_id) override;
  void OnNetworkRequestStarted(const extensions::ExtensionHost* host,
                               uint64_t request_id) override;
  void OnNetworkRequestDone(const extensions::ExtensionHost* host,
                            uint64_t request_id) override;

  // PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void DarkSuspendImminent() override;
  void SuspendDone(const base::TimeDelta& duration) override;

 private:
  friend class TestApi;

  // Called when the system is about to perform a regular suspend or a dark
  // suspend.
  void OnSuspendImminent(bool dark_suspend);

  // Reports readiness to suspend to the PowerManagerClient if a suspend is
  // pending and there are no outstanding events keeping the system awake.
  void MaybeReportSuspendReadiness();

  struct KeepaliveSources;
  std::unordered_map<const extensions::ExtensionHost*,
                     std::unique_ptr<KeepaliveSources>>
      keepalive_sources_;

  ScopedObserver<extensions::ProcessManager, extensions::ProcessManagerObserver>
      process_manager_observers_{this};

  bool should_delay_suspend_ = true;
  int suspend_keepalive_count_ = 0;

  // |this| blocks Power Manager suspend with this token. When the token is
  // empty, |this| isn't blocking suspend.
  base::UnguessableToken block_suspend_token_;

  base::CancelableClosure suspend_readiness_callback_;

  base::WeakPtrFactory<ExtensionEventObserver> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionEventObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_EXTENSION_EVENT_OBSERVER_H_
