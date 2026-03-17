// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNGRADE_DOWNGRADE_MANAGER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_DOWNGRADE_DOWNGRADE_MANAGER_DELEGATE_IMPL_H_

#include "chrome/browser/downgrade/downgrade_manager_delegate.h"

namespace downgrade {

class DowngradeManagerDelegateImpl : public DowngradeManagerDelegate {
 public:
  DowngradeManagerDelegateImpl() = default;
  ~DowngradeManagerDelegateImpl() override = default;

  int GetMaxNumberOfSnapshots() const override;
  bool UserDataSnapshotEnabled() const override;
  base::FilePath GetDiskCacheDir() const override;
  std::vector<SnapshotItemDetails> GetUserDataSnapshotItems() const override;
  std::vector<SnapshotItemDetails> GetProfileSnapshotItems() const override;
};

}  // namespace downgrade

#endif  // CHROME_BROWSER_DOWNGRADE_DOWNGRADE_MANAGER_DELEGATE_IMPL_H_
