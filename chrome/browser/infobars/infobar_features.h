// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_FEATURES_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/infobars/core/infobar_delegate.h"

namespace infobars {

// Feature flag controlling the centralization of desktop infobars.
// TODO(https://crbug.com/512837934): Remove feature flag once fully launched
// and all feature-specific delegates are migrated.
BASE_DECLARE_FEATURE(kCentralizedInfoBarFramework);

BASE_DECLARE_FEATURE_PARAM(bool, kEnableAll);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedAutomation);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedBadFlags);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedCollectedCookies);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedDefaultBrowser);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedDevToolsConfirm);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedDevToolsSharedProcess);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedGoogleApiKeys);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedInstallerDownloader);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedKnownInterceptionDisclosure);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedLinkCapturing);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedPageInfo);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedPdf);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedChromeForTesting);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedObsoleteSystem);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedOSCryptAsyncAvailability);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedPinInfoBar);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedLocalTestPolicies);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedThemeInstalled);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedExtensionDevTools);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedSessionRestore);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedInstallationError);
BASE_DECLARE_FEATURE_PARAM(bool, kMigratedWebAuthFlow);

// Returns true if the centralization framework is enabled and the specified
// infobar is configured to be migrated.
bool IsInfoBarMigrated(InfoBarDelegate::InfoBarIdentifier infobar_id);

}  // namespace infobars

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_FEATURES_H_
