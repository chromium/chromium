// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_BROWSER_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_BROWSER_TEST_UTIL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#include "services/network/public/mojom/network_change_manager.mojom-forward.h"

class CrostiniBrowserTestChromeBrowserMainExtraParts;

// Common base for Crostini browser tests. Allows tests to set network
// connection type.
class CrostiniBrowserTestBase : public InProcessBrowserTest {
 public:
  explicit CrostiniBrowserTestBase(bool register_termina);

  CrostiniBrowserTestBase(const CrostiniBrowserTestBase&) = delete;
  CrostiniBrowserTestBase& operator=(const CrostiniBrowserTestBase&) = delete;

  // BrowserTestBase:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override;
  void SetUpOnMainThread() override;

  void SetConnectionType(network::mojom::ConnectionType connection_type);

  void UnregisterTermina();

 protected:
  const bool register_termina_;

  base::test::ScopedFeatureList scoped_feature_list_;
  crostini::FakeCrostiniFeatures fake_crostini_features_;

  // Owned by content::Browser
  raw_ptr<CrostiniBrowserTestChromeBrowserMainExtraParts, DanglingUntriaged>
      extra_parts_ = nullptr;

 private:
  void DiskMountImpl(const std::string& source_path,
                     const std::string& source_format,
                     const std::string& mount_label,
                     const std::vector<std::string>& mount_options,
                     ash::MountType type,
                     ash::MountAccessMode access_mode,
                     ash::disks::DiskMountManager::MountPathCallback callback);

  // Owned by ash::disks::DiskMountManager;
  raw_ptr<ash::disks::MockDiskMountManager, DanglingUntriaged> dmgr_;
};

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_BROWSER_TEST_UTIL_H_
