// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_SYNC_SYNC_CROSAPI_MANAGER_LACROS_H_
#define CHROME_BROWSER_LACROS_SYNC_SYNC_CROSAPI_MANAGER_LACROS_H_

#include <memory>

class Profile;
class SyncExplicitPassphraseClientLacros;
class SyncUserSettingsClientLacros;

// Controls lifetime of sync-related Crosapi clients.
class SyncCrosapiManagerLacros {
 public:
  SyncCrosapiManagerLacros();
  ~SyncCrosapiManagerLacros();

  void PostProfileInit(Profile* profile);

 private:
  // TODO(crbug.com/1327602): Destroy `sync_explicit_passphrase_client_` upon
  // main profile SyncService shutdown and remove handling of SyncService
  // from the client code.
  std::unique_ptr<SyncExplicitPassphraseClientLacros>
      sync_explicit_passphrase_client_;
  std::unique_ptr<SyncUserSettingsClientLacros> sync_user_settings_client_;
};

#endif  // CHROME_BROWSER_LACROS_SYNC_SYNC_CROSAPI_MANAGER_LACROS_H_
