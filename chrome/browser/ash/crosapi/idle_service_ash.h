// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_IDLE_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_IDLE_SERVICE_ASH_H_

#include "base/time/time.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/crosapi/mojom/idle_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace ui {
class Event;
}  // namespace ui

namespace crosapi {

// The ash-chrome implementation of the IdleService crosapi interface.
// This class must only be used from the main thread.
class IdleServiceAsh : public mojom::IdleService {
 public:
  // Helper to observe changes in relevant quantities, read these values, and
  // manage / dispatch to observers for IdleServiceAsh.
  class Dispatcher : public ui::UserActivityObserver,
                     public ash::SessionManagerClient::Observer {
   public:
    Dispatcher();
    Dispatcher(const Dispatcher&) = delete;
    Dispatcher& operator=(const Dispatcher&) = delete;
    ~Dispatcher() override;

    // ui::UserActivityObserver:
    void OnUserActivity(const ui::Event* event) override;

    // SessionManagerClient::Observer:
    void ScreenLockedStateUpdated() override;

   private:
    friend IdleServiceAsh;

    // Reads IdleInfo and sends copies to |observers_|.
    void DispatchChange();

    // Whether the class is disabled for testing.
    static bool is_disabled_for_testing_;

    // The time stamp of the |event| when OnUserActivity() is last called.
    base::TimeTicks last_user_activity_time_stamp_;

    // Support any number of observers.
    mojo::RemoteSet<mojom::IdleInfoObserver> observers_;
  };

  // Returns a new mojom::IdleInfoPtr populated with freshly read ChromeOS data.
  static mojom::IdleInfoPtr ReadIdleInfoFromSystem();

  // IdleServiceAsh::Dispatcher listens to objects that can be uninstantiated
  // in some tests. This function
  static void DisableForTesting();

  IdleServiceAsh();
  IdleServiceAsh(const IdleServiceAsh&) = delete;
  IdleServiceAsh& operator=(const IdleServiceAsh&) = delete;
  ~IdleServiceAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::IdleService> receiver);

  // crosapi::mojom::IdleService:
  void AddIdleInfoObserver(
      mojo::PendingRemote<mojom::IdleInfoObserver> observer) override;

 private:
  // Support any number of connections.
  mojo::ReceiverSet<mojom::IdleService> receivers_;

  Dispatcher dispatcher_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_IDLE_SERVICE_ASH_H_
