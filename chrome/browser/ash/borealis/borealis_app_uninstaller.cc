// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_app_uninstaller.h"

#include "base/logging.h"
#include "chrome/browser/ash/borealis/borealis_installer.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"

namespace borealis {

BorealisAppUninstaller::BorealisAppUninstaller(Profile* profile)
    : profile_(profile) {}

void BorealisAppUninstaller::Uninstall(std::string app_id,
                                       OnUninstalledCallback callback) {
  // TODO(b/171353248): Allow uninstalling other apps
  DCHECK(app_id == kBorealisAppId);

  BorealisService::GetForProfile(profile_)->Installer().Uninstall(
      base::DoNothing());
}

}  // namespace borealis
