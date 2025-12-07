// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_tiles/chrome_enterprise_shortcuts_manager_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/ntp_tiles/enterprise/enterprise_shortcuts_manager_impl.h"

std::unique_ptr<ntp_tiles::EnterpriseShortcutsManager>
ChromeEnterpriseShortcutsManagerFactory::NewForProfile(Profile* profile) {
  return std::make_unique<ntp_tiles::EnterpriseShortcutsManagerImpl>(
      profile->GetPrefs());
}
