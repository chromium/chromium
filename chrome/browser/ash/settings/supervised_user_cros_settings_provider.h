// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SETTINGS_SUPERVISED_USER_CROS_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_ASH_SETTINGS_SUPERVISED_USER_CROS_SETTINGS_PROVIDER_H_

#include <map>
#include <string>

#include "chromeos/ash/components/settings/cros_settings_provider.h"

namespace base {
class Value;
}

namespace ash {

class SupervisedUserCrosSettingsProvider : public CrosSettingsProvider {
 public:
  explicit SupervisedUserCrosSettingsProvider(
      const CrosSettingsProvider::NotifyObserversCallback& notify_cb);

  SupervisedUserCrosSettingsProvider(
      const SupervisedUserCrosSettingsProvider&) = delete;
  SupervisedUserCrosSettingsProvider& operator=(
      const SupervisedUserCrosSettingsProvider&) = delete;

  ~SupervisedUserCrosSettingsProvider() override;

  // CrosSettingsProvider:
  const base::Value* Get(const std::string& path) const override;
  TrustedStatus PrepareTrustedValues(base::OnceClosure* callback) override;
  bool HandlesSetting(const std::string& path) const override;

 private:
  // Cros pref name to pref value.
  std::map<std::string, base::Value> child_user_restrictions_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SETTINGS_SUPERVISED_USER_CROS_SETTINGS_PROVIDER_H_
