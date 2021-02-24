// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_SUPERVISED_USER_CROS_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_SUPERVISED_USER_CROS_SETTINGS_PROVIDER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "chromeos/settings/cros_settings_provider.h"

namespace base {
class Value;
}

namespace chromeos {

class SupervisedUserCrosSettingsProvider : public CrosSettingsProvider {
 public:
  explicit SupervisedUserCrosSettingsProvider(
      const CrosSettingsProvider::NotifyObserversCallback& notify_cb);
  ~SupervisedUserCrosSettingsProvider() override;

  // CrosSettingsProvider:
  const base::Value* Get(const std::string& path) const override;
  TrustedStatus PrepareTrustedValues(base::OnceClosure* callback) override;
  bool HandlesSetting(const std::string& path) const override;

 private:
  // Cros pref name to pref value.
  std::map<std::string, base::Value> child_user_restrictions_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserCrosSettingsProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_SUPERVISED_USER_CROS_SETTINGS_PROVIDER_H_
