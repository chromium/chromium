// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_features.h"

namespace infobars {

BASE_FEATURE(kCentralizedInfoBarFramework, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kEnableAll,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedAutomation,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedBadFlags,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedCollectedCookies,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedDefaultBrowser,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedDevToolsConfirm,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedDevToolsSharedProcess,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedGoogleApiKeys,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedInstallerDownloader,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedKnownInterceptionDisclosure,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedLinkCapturing,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedPageInfo,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool, kMigratedPdf, &kCentralizedInfoBarFramework, false);
BASE_FEATURE_PARAM(bool,
                   kMigratedObsoleteSystem,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedOSCryptAsyncAvailability,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedChromeForTesting,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedPinInfoBar,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedLocalTestPolicies,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedThemeInstalled,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedExtensionDevTools,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedSessionRestore,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedInstallationError,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedWebAuthFlow,
                   &kCentralizedInfoBarFramework,
                   false);

const base::FeatureParam<bool>* GetInfoBarMigrationParam(
    InfoBarDelegate::InfoBarIdentifier infobar_id) {
  switch (infobar_id) {
    case InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE:
      return &kMigratedAutomation;
    case InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE:
      return &kMigratedBadFlags;
    case InfoBarDelegate::COLLECTED_COOKIES_INFOBAR_DELEGATE:
      return &kMigratedCollectedCookies;
    case InfoBarDelegate::DEFAULT_BROWSER_INFOBAR_DELEGATE:
      return &kMigratedDefaultBrowser;
    case InfoBarDelegate::DEV_TOOLS_INFOBAR_DELEGATE:
      return &kMigratedDevToolsConfirm;
    case InfoBarDelegate::DEV_TOOLS_SHARED_PROCESS_DELEGATE:
      return &kMigratedDevToolsSharedProcess;
    case InfoBarDelegate::GOOGLE_API_KEYS_INFOBAR_DELEGATE:
      return &kMigratedGoogleApiKeys;
    case InfoBarDelegate::INSTALLER_DOWNLOADER_INFOBAR_DELEGATE:
      return &kMigratedInstallerDownloader;
    case InfoBarDelegate::KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_DELEGATE:
      return &kMigratedKnownInterceptionDisclosure;
    case InfoBarDelegate::ENABLE_LINK_CAPTURING_INFOBAR_DELEGATE:
      return &kMigratedLinkCapturing;
    case InfoBarDelegate::PAGE_INFO_INFOBAR_DELEGATE:
      return &kMigratedPageInfo;
    case InfoBarDelegate::PDF_INFOBAR_DELEGATE:
      return &kMigratedPdf;
    case InfoBarDelegate::CHROME_FOR_TESTING_INFOBAR_DELEGATE:
      return &kMigratedChromeForTesting;
    case InfoBarDelegate::OBSOLETE_SYSTEM_INFOBAR_DELEGATE:
      return &kMigratedObsoleteSystem;
    case InfoBarDelegate::OSCRYPTASYNC_AVAILABILITY_INFOBAR_DELEGATE:
      return &kMigratedOSCryptAsyncAvailability;
    case InfoBarDelegate::PIN_INFOBAR_DELEGATE:
      return &kMigratedPinInfoBar;
    case InfoBarDelegate::LOCAL_TEST_POLICIES_APPLIED_INFOBAR:
      return &kMigratedLocalTestPolicies;
    case InfoBarDelegate::THEME_INSTALLED_INFOBAR_DELEGATE:
      return &kMigratedThemeInstalled;
    case InfoBarDelegate::EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE:
      return &kMigratedExtensionDevTools;
    case InfoBarDelegate::SESSION_RESTORE_INFOBAR_DELEGATE:
      return &kMigratedSessionRestore;
    case InfoBarDelegate::INSTALLATION_ERROR_INFOBAR_DELEGATE:
      return &kMigratedInstallationError;
    case InfoBarDelegate::EXTENSIONS_WEB_AUTH_FLOW_INFOBAR_DELEGATE:
      return &kMigratedWebAuthFlow;
    default:
      return nullptr;
  }
}

bool IsInfoBarMigrated(InfoBarDelegate::InfoBarIdentifier infobar_id) {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  if (!base::FeatureList::IsEnabled(kCentralizedInfoBarFramework)) {
    return false;
  }

  const auto* param = GetInfoBarMigrationParam(infobar_id);
  if (param == nullptr) {
    return false;
  }

  if (kEnableAll.Get()) {
    return true;
  }

  return param->Get();
#endif
}

}  // namespace infobars
