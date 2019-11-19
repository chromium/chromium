// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_PREF_STORE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_PREF_STORE_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/observer_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "components/prefs/pref_store.h"

namespace base {
class DictionaryValue;
class Value;
}

class PrefValueMap;
class SupervisedUserSettingsService;

// A PrefStore that gets its values from supervised user settings via the
// SupervisedUserSettingsService passed in at construction.
class SupervisedUserPrefStore : public PrefStore {
 public:
  explicit SupervisedUserPrefStore(
      SupervisedUserSettingsService* supervised_user_settings_service);

  // PrefStore overrides:
  bool GetValue(const std::string& key,
                const base::Value** value) const override;
  std::unique_ptr<base::DictionaryValue> GetValues() const override;
  void AddObserver(PrefStore::Observer* observer) override;
  void RemoveObserver(PrefStore::Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;

 private:
  ~SupervisedUserPrefStore() override;

  void OnNewSettingsAvailable(const base::DictionaryValue* settings);

  void OnSettingsServiceShutdown();

  std::unique_ptr<
      base::CallbackList<void(const base::DictionaryValue*)>::Subscription>
      user_settings_subscription_;

  std::unique_ptr<base::CallbackList<void()>::Subscription>
      shutdown_subscription_;

  std::unique_ptr<PrefValueMap> prefs_;

  base::ObserverList<PrefStore::Observer, true>::Unchecked observers_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_PREF_STORE_H_
