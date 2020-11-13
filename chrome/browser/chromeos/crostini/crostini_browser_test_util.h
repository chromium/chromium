// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_BROWSER_TEST_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_BROWSER_TEST_UTIL_H_

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/crostini/fake_crostini_features.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "services/network/public/mojom/network_change_manager.mojom-forward.h"

class CrostiniBrowserTestChromeBrowserMainExtraParts;

// Common base for Crostini browser tests. Allows tests to set network
// connection type.
class CrostiniBrowserTestBase : public InProcessBrowserTest {
 public:
  explicit CrostiniBrowserTestBase(bool register_termina);

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
  CrostiniBrowserTestChromeBrowserMainExtraParts* extra_parts_ = nullptr;

 private:
  void DiskMountImpl(const std::string& source_path,
                     const std::string& source_format,
                     const std::string& mount_label,
                     const std::vector<std::string>& mount_options,
                     chromeos::MountType type,
                     chromeos::MountAccessMode access_mode);

  // Owned by chromeos::disks::DiskMountManager;
  chromeos::disks::MockDiskMountManager* dmgr_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniBrowserTestBase);
};

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_BROWSER_TEST_UTIL_H_
