// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_PREF_STORE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_PREF_STORE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/observer_list.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "components/prefs/pref_store.h"
#include "components/supervised_user/core/common/supervised_users.h"

namespace base {
class Value;
}

class PrefValueMap;

namespace supervised_user {
class SupervisedUserSettingsService;
}  // namespace supervised_user

// A PrefStore that gets its values from supervised user settings via the
// SupervisedUserSettingsService passed in at construction.
class SupervisedUserPrefStore : public PrefStore {
 public:
  explicit SupervisedUserPrefStore(
      supervised_user::SupervisedUserSettingsService*
          supervised_user_settings_service);

  // PrefStore overrides:
  bool GetValue(base::StringPiece key,
                const base::Value** value) const override;
  base::Value::Dict GetValues() const override;
  void AddObserver(PrefStore::Observer* observer) override;
  void RemoveObserver(PrefStore::Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;

 private:
  ~SupervisedUserPrefStore() override;

  void OnNewSettingsAvailable(const base::Value::Dict& settings);

  void OnSettingsServiceShutdown();

  base::CallbackListSubscription user_settings_subscription_;

  base::CallbackListSubscription shutdown_subscription_;

  std::unique_ptr<PrefValueMap> prefs_;

  base::ObserverList<PrefStore::Observer, true>::Unchecked observers_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_PREF_STORE_H_
