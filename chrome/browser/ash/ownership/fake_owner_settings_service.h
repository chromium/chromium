// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_OWNERSHIP_FAKE_OWNER_SETTINGS_SERVICE_H_
#define CHROME_BROWSER_ASH_OWNERSHIP_FAKE_OWNER_SETTINGS_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"

class Profile;

namespace ownership {
class OwnerKeyUtil;
}  // namespace ownership

namespace ash {

class StubCrosSettingsProvider;

class FakeOwnerSettingsService : public OwnerSettingsServiceAsh {
 public:
  FakeOwnerSettingsService(StubCrosSettingsProvider* provider,
                           Profile* profile);
  FakeOwnerSettingsService(
      StubCrosSettingsProvider* provider,
      Profile* profile,
      const scoped_refptr<ownership::OwnerKeyUtil>& owner_key_util);

  FakeOwnerSettingsService(const FakeOwnerSettingsService&) = delete;
  FakeOwnerSettingsService& operator=(const FakeOwnerSettingsService&) = delete;

  ~FakeOwnerSettingsService() override;

  void set_set_management_settings_result(bool success) {
    set_management_settings_result_ = success;
  }

  const ManagementSettings& last_settings() const {
    return last_settings_;
  }

  // OwnerSettingsServiceAsh:
  bool IsOwner() override;
  bool Set(const std::string& setting, const base::Value& value) override;

 private:
  bool set_management_settings_result_ = true;
  ManagementSettings last_settings_;
  raw_ptr<StubCrosSettingsProvider, DanglingUntriaged> settings_provider_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_OWNERSHIP_FAKE_OWNER_SETTINGS_SERVICE_H_
