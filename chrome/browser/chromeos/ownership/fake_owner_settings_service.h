// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OWNERSHIP_FAKE_OWNER_SETTINGS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_OWNERSHIP_FAKE_OWNER_SETTINGS_SERVICE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"

class Profile;

namespace ownership {
class OwnerKeyUtil;
}

namespace chromeos {

class StubCrosSettingsProvider;

class FakeOwnerSettingsService : public OwnerSettingsServiceChromeOS {
 public:
  FakeOwnerSettingsService(StubCrosSettingsProvider* provider,
                           Profile* profile);
  FakeOwnerSettingsService(
      StubCrosSettingsProvider* provider,
      Profile* profile,
      const scoped_refptr<ownership::OwnerKeyUtil>& owner_key_util);

  ~FakeOwnerSettingsService() override;

  void set_set_management_settings_result(bool success) {
    set_management_settings_result_ = success;
  }

  const ManagementSettings& last_settings() const {
    return last_settings_;
  }

  // OwnerSettingsServiceChromeOS:
  bool IsOwner() override;
  bool Set(const std::string& setting, const base::Value& value) override;

 private:
  bool set_management_settings_result_ = true;
  ManagementSettings last_settings_;
  StubCrosSettingsProvider* settings_provider_;

  DISALLOW_COPY_AND_ASSIGN(FakeOwnerSettingsService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OWNERSHIP_FAKE_OWNER_SETTINGS_SERVICE_H_
