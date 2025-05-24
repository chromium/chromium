// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SETTINGS_STUB_CROS_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_ASH_SETTINGS_STUB_CROS_SETTINGS_PROVIDER_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "chromeos/ash/components/settings/fake_cros_settings_provider.h"
#include "components/prefs/pref_value_map.h"

namespace ash {

// CrosSettingsProvider implementation that stores settings in memory unsigned.
class StubCrosSettingsProvider : public CrosSettingsProvider {
 public:
  explicit StubCrosSettingsProvider(
      const NotifyObserversCallback& notify_cb = {});

  StubCrosSettingsProvider(const StubCrosSettingsProvider&) = delete;
  StubCrosSettingsProvider& operator=(const StubCrosSettingsProvider&) = delete;

  ~StubCrosSettingsProvider() override;

  // CrosSettingsProvider implementation.
  const base::Value* Get(std::string_view path) const override;
  TrustedStatus PrepareTrustedValues(base::OnceClosure* callback) override;
  bool HandlesSetting(std::string_view path) const override;

  void SetTrustedStatus(TrustedStatus status);
  void SetCurrentUserIsOwner(bool owner);

  bool current_user_is_owner() const { return current_user_is_owner_; }

  // Sets in-memory setting at |path| to value |in_value|.
  void Set(const std::string& path, const base::Value& in_value);

  // Convenience forms of Set(). These methods will replace any existing value
  // at that |path|, even if it has a different type.
  void SetBoolean(const std::string& path, bool in_value);
  void SetInteger(const std::string& path, int in_value);
  void SetDouble(const std::string& path, double in_value);
  void SetString(const std::string& path, const std::string& in_value);

 private:
  // Initializes settings to their defaults.
  void SetDefaults();

  // Called when the value in `fake_provider_` is updated.
  void OnValueChanged(const std::string& path);

  // Some tests imply that calling Set() as non-owner doesn't change the actual
  // value but still trigger a notification. For such cases, it is possible to
  // emulate this behavior by changing the ownership status to non-owner with
  // |SetCurrentUserIsOwner(false)|.
  bool current_user_is_owner_ = true;

  // TODO(crbug.com/402266244): Migrated StubCrosSettingsProvider into
  // FakeCrosSettingsProvider.
  FakeCrosSettingsProvider fake_provider_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SETTINGS_STUB_CROS_SETTINGS_PROVIDER_H_
