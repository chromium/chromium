// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_FAKE_CROSTINI_FEATURES_H_
#define CHROME_BROWSER_ASH_CROSTINI_FAKE_CROSTINI_FEATURES_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/crostini/crostini_features.h"

class Profile;

namespace crostini {

// FakeCrostiniFeatures implements a fake version of CrostiniFeatures which can
// be used for testing.  It captures the current global CrostiniFeatures object
// and replaces it for the scope of this object.  It overrides only the
// features that you set and uses the previous object for other features.
class FakeCrostiniFeatures : public CrostiniFeatures {
 public:
  FakeCrostiniFeatures();
  ~FakeCrostiniFeatures() override;

  // CrostiniFeatures:
  bool CouldBeAllowed(Profile* profile, std::string* reason) override;
  bool IsAllowedNow(Profile* profile, std::string* reason) override;
  bool IsEnabled(Profile* profile) override;
  bool IsExportImportUIAllowed(Profile* profile) override;
  bool IsRootAccessAllowed(Profile* profile) override;
  bool IsContainerUpgradeUIAllowed(Profile* profile) override;
  void CanChangeAdbSideloading(
      Profile* profile,
      CanChangeAdbSideloadingCallback callback) override;
  bool IsPortForwardingAllowed(Profile* profile) override;
  bool IsMultiContainerAllowed(Profile* profile) override;

  void SetAll(bool flag);
  void ClearAll();

  void set_could_be_allowed(bool allowed) { could_be_allowed_ = allowed; }
  void set_is_allowed_now(bool allowed) { allowed_now_ = allowed; }
  void set_enabled(bool enabled) { enabled_ = enabled; }
  void set_export_import_ui_allowed(bool allowed) {
    export_import_ui_allowed_ = allowed;
  }
  void set_root_access_allowed(bool allowed) { root_access_allowed_ = allowed; }
  void set_container_upgrade_ui_allowed(bool allowed) {
    container_upgrade_ui_allowed_ = allowed;
  }

  void set_can_change_adb_sideloading(bool can_change) {
    can_change_adb_sideloading_ = can_change;
  }

  void set_port_forwarding_allowed(bool allowed) {
    port_forwarding_allowed_ = allowed;
  }

  void set_multi_container_allowed(bool allowed) {
    multi_container_allowed_ = allowed;
  }

 private:
  // Original global static when this instance is created. It is captured when
  // FakeCrostiniFeatures is created and replaced at destruction.
  raw_ptr<CrostiniFeatures> original_features_;

  std::optional<bool> could_be_allowed_;
  std::optional<bool> allowed_now_;
  std::optional<bool> enabled_;
  std::optional<bool> export_import_ui_allowed_;
  std::optional<bool> root_access_allowed_;
  std::optional<bool> container_upgrade_ui_allowed_;
  std::optional<bool> can_change_adb_sideloading_;
  std::optional<bool> port_forwarding_allowed_;
  std::optional<bool> multi_container_allowed_;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_FAKE_CROSTINI_FEATURES_H_
