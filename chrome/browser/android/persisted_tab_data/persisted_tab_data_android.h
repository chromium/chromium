// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_PERSISTED_TAB_DATA_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_PERSISTED_TAB_DATA_ANDROID_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/tab_android_user_data.h"

class PersistedTabDataStorageAndroid;
class TabAndroid;

// Base class for native PersistedTabData. Clients extend off this class.
class PersistedTabDataAndroid
    : public TabAndroidUserData<PersistedTabDataAndroid> {
 public:
  PersistedTabDataAndroid(TabAndroid* tab_android, const void* user_data_key);
  ~PersistedTabDataAndroid() override;

  using FromCallback = base::OnceCallback<void(PersistedTabDataAndroid*)>;
  using SupplierCallback =
      base::OnceCallback<std::unique_ptr<PersistedTabDataAndroid>()>;

 protected:
  // Handles PersistedTabData acquisition by:
  // - Acquire PersistedTabData associated with a Tab via UserData. If not there
  // ...
  // - Restore PersistedTabData from disk (if possible). If not there ...
  // - Re-acquire PersistedTabData using the supplier
  static void From(TabAndroid* tab_android,
                   const void* user_data_key,
                   SupplierCallback supplier_callback,
                   FromCallback from_callback);

  // Serialize PersistedTabData for storage
  virtual std::unique_ptr<const std::vector<uint8_t>> Serialize() = 0;

  // Deserialize PersistedTabData from storage
  virtual void Deserialize(const std::vector<uint8_t>& data) = 0;

  // Save PersistedTabData (usually following an update to a persisted
  // attribute)
  void Save();

  // Remove PersistedTabData from storage (e.g. following the Tab being
  // destroyed)
  void Remove();

 private:
  friend class TabAndroidUserData<PersistedTabDataAndroid>;
  friend class SensitivityPersistedTabDataAndroidBrowserTest;

  // Storage implementation for PersistedTabData (currently only LevelDB is
  // supported) However, support may be added for other storage modes (e.g.
  // file, SQLite) in the future.
  raw_ptr<PersistedTabDataStorageAndroid> persisted_tab_data_storage_android_;

  // Identifier for the PersistedTabData which all keys are prepended with
  raw_ptr<const char> data_id_;

  int tab_id_;

  static Profile* GetProfile(TabAndroid* tab_android);

  TAB_ANDROID_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_PERSISTED_TAB_DATA_ANDROID_H_
