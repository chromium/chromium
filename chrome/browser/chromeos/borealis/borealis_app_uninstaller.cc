// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_app_uninstaller.h"

#include "base/logging.h"

namespace borealis {

BorealisAppUninstaller::BorealisAppUninstaller(Profile* profile)
    : profile_(profile) {}

void BorealisAppUninstaller::Uninstall(std::string app_id,
                                       OnUninstalledCallback callback) {
  // TODO(b/170264723): Implement this.
  (void)profile_;
  LOG(WARNING) << "Uninstallation is not implemented.";
}

}  // namespace borealis
