// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_SYNC_SYNC_USER_SETTINGS_CLIENT_LACROS_H_
#define CHROME_BROWSER_LACROS_SYNC_SYNC_USER_SETTINGS_CLIENT_LACROS_H_

#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace syncer {
class SyncUserSettings;
}  // namespace syncer

// Once created, observes changes in Ash SyncUserSettings via Crosapi
// (currently, only apps toggle state) and populates them to Lacros
// SyncUserSettings.
class SyncUserSettingsClientLacros
    : public crosapi::mojom::SyncUserSettingsClientObserver {
 public:
  // |remote| must be bound. |sync_user_settings| must not be null and must
  // outlive |this| object.
  SyncUserSettingsClientLacros(
      mojo::Remote<crosapi::mojom::SyncUserSettingsClient> remote,
      syncer::SyncUserSettings* sync_user_settings);
  SyncUserSettingsClientLacros(const SyncUserSettingsClientLacros& other) =
      delete;
  SyncUserSettingsClientLacros& operator=(
      const SyncUserSettingsClientLacros& other) = delete;
  ~SyncUserSettingsClientLacros() override;

  // crosapi::mojom::SyncUserSettingsClientObserver overrides.
  void OnAppsSyncEnabledChanged(bool is_apps_sync_enabled) override;

 private:
  void OnIsAppsSyncEnabledFetched(bool is_apps_sync_enabled);

  raw_ptr<syncer::SyncUserSettings> sync_user_settings_;
  mojo::Receiver<crosapi::mojom::SyncUserSettingsClientObserver>
      observer_receiver_{this};
  mojo::Remote<crosapi::mojom::SyncUserSettingsClient> remote_;
};

#endif  // CHROME_BROWSER_LACROS_SYNC_SYNC_USER_SETTINGS_CLIENT_LACROS_H_
