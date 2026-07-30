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
                   kMigratedCollectedCookies,
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
                   kMigratedPageInfo,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool, kMigratedPdf, &kCentralizedInfoBarFramework, false);
BASE_FEATURE_PARAM(bool,
                   kMigratedObsoleteSystem,
                   &kCentralizedInfoBarFramework,
                   false);

BASE_FEATURE_PARAM(bool,
                   kMigratedChromeForTesting,
                   &kCentralizedInfoBarFramework,
                   false);

const base::FeatureParam<bool>* GetInfoBarMigrationParam(
    InfoBarDelegate::InfoBarIdentifier infobar_id) {
  switch (infobar_id) {
    case InfoBarDelegate::COLLECTED_COOKIES_INFOBAR_DELEGATE:
      return &kMigratedCollectedCookies;
    case InfoBarDelegate::GOOGLE_API_KEYS_INFOBAR_DELEGATE:
      return &kMigratedGoogleApiKeys;
    case InfoBarDelegate::INSTALLER_DOWNLOADER_INFOBAR_DELEGATE:
      return &kMigratedInstallerDownloader;
    case InfoBarDelegate::KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_DELEGATE:
      return &kMigratedKnownInterceptionDisclosure;
    case InfoBarDelegate::PAGE_INFO_INFOBAR_DELEGATE:
      return &kMigratedPageInfo;
    case InfoBarDelegate::PDF_INFOBAR_DELEGATE:
      return &kMigratedPdf;
    case InfoBarDelegate::CHROME_FOR_TESTING_INFOBAR_DELEGATE:
      return &kMigratedChromeForTesting;
    case InfoBarDelegate::OBSOLETE_SYSTEM_INFOBAR_DELEGATE:
      return &kMigratedObsoleteSystem;
    default:
      return nullptr;
  }
}

bool IsInfoBarMigrated(InfoBarDelegate::InfoBarIdentifier infobar_id) {
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
}

}  // namespace infobars
