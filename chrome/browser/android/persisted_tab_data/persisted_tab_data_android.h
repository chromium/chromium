// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_PERSISTED_TAB_DATA_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_PERSISTED_TAB_DATA_ANDROID_H_

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/tab_android_user_data.h"
#include "chrome/browser/profiles/profile.h"

class PersistedTabDataAndroidHelper;
class PersistedTabDataStorageAndroid;
class TabAndroid;

struct PersistedTabDataAndroidDeferredRequest;

// Helper base class for native PersistedTabData. This class lives on, and must
// only be used on, the UI thread.
class PersistedTabDataAndroid
    : public TabAndroidUserData<PersistedTabDataAndroid> {
 public:
  PersistedTabDataAndroid(TabAndroid* tab_android, const void* user_data_key);
  ~PersistedTabDataAndroid() override;

  using FromCallback = base::OnceCallback<void(PersistedTabDataAndroid*)>;
  using SupplierCallback =
      base::OnceCallback<std::unique_ptr<PersistedTabDataAndroid>(TabAndroid*)>;

 protected:
  // Handles PersistedTabData acquisition by:
  // - Acquire PersistedTabData associated with a Tab via UserData. If not there
  // ...
  // - Restore PersistedTabData from disk (if possible). If not there ...
  // - Re-acquire PersistedTabData using the supplier
  //
  // `supplier_callback` and `from_callback` will never be synchronously
  // invoked, i.e. they will not be invoked while the call to `From()` is still
  // on the stack.
  static void From(base::WeakPtr<TabAndroid> tab_android,
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

  // Remove all PersistedTabDataAndroid entries stored for |tab_id| related
  // to a given |profile|.
  static void RemoveAll(int tab_id, Profile* profile);

  // Determines if PersistedTabDataAndroid exists for the corresponding
  // |tab_android| and |user_data_key|. Returns true/false in
  // |exists_callback|.
  static void ExistsForTesting(TabAndroid* tab_android,
                               const void* user_data_key,
                               base::OnceCallback<void(bool)> exists_callback);

 private:
  friend class AuxiliarySearchProviderBrowserTest;
  friend class SyncedTabDelegateAndroidTest;
  friend class PersistedTabDataAndroidBrowserTest;
  friend class PersistedTabDataAndroidHelper;
  friend class SensitivityPersistedTabDataAndroid;
  friend class SensitivityPersistedTabDataAndroidBrowserTest;
  friend class TabAndroidUserData<PersistedTabDataAndroid>;

  // Storage implementation for PersistedTabData (currently only LevelDB is
  // supported) However, support may be added for other storage modes (e.g.
  // file, SQLite) in the future.
  raw_ptr<PersistedTabDataStorageAndroid> persisted_tab_data_storage_android_;

  // Identifier for the PersistedTabData which all keys are prepended with
  raw_ptr<const char> data_id_;

  int tab_id_;

  // Called when a Tab is closed.
  static void OnTabClose(TabAndroid* tab_android);

  // Called when deferred startup occurs.
  static void OnDeferredStartup();

  static std::map<std::string,
                  std::vector<PersistedTabDataAndroid::FromCallback>>&
  GetCachedCallbackMap();

  void RunCallbackOnUIThread(const TabAndroid* tab_android,
                             const void* user_data_key);

  // PersistedTabData::From requests are delayed until deferred
  // startup occurs to mitigate the risk of jank.
  static base::circular_deque<PersistedTabDataAndroidDeferredRequest>&
  GetDeferredRequests();
  static bool deferred_startup_complete_;

  TAB_ANDROID_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_ANDROID_PERSISTED_TAB_DATA_PERSISTED_TAB_DATA_ANDROID_H_
