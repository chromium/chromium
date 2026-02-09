// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXTENSION_SETTINGS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXTENSION_SETTINGS_HELPER_H_

#include <ostream>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"

class Profile;

namespace syncer {
class SyncServiceImpl;
}  // namespace syncer

namespace extension_settings_helper {

// Calls Set() with |settings| for |profile| and the extension with ID |id|.
void SetExtensionSettings(Profile* profile,
                          const std::string& id,
                          const base::DictValue& settings);

// Calls Set() with |settings| for all profiles the extension with ID |id|.
void SetExtensionSettings(
    const std::vector<raw_ptr<Profile, VectorExperimental>>& profiles,
    const std::string& id,
    const base::DictValue& settings);

// A checker that waits for the extension settings to be the same across all
// profiles.
class AllExtensionSettingsSameChecker : public MultiClientStatusChangeChecker {
 public:
  // TODO(crbug.com/461744384): use ExtensionRegistry observer instead of
  // MultiClientStatusChangeChecker.
  AllExtensionSettingsSameChecker(
      const std::vector<raw_ptr<syncer::SyncServiceImpl, VectorExperimental>>&
          services,
      const std::vector<raw_ptr<Profile, VectorExperimental>>& profiles);
  ~AllExtensionSettingsSameChecker() override;

  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const std::vector<raw_ptr<Profile, VectorExperimental>> profiles_;
};

}  // namespace extension_settings_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXTENSION_SETTINGS_HELPER_H_
