// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/full_restore_service.h"

#include "chrome/browser/chromeos/full_restore/full_restore_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace chromeos {
namespace full_restore {

FullRestoreService::FullRestoreService(Profile* profile) {
  // TODO(crbug.com/909794):If the system crashed before reboot, show the
  // notification notification. Otherwise, read |kRestoreAppsAndPagesPrefName|
  // from the user pref.
}

FullRestoreService::~FullRestoreService() = default;

}  // namespace full_restore
}  // namespace chromeos
