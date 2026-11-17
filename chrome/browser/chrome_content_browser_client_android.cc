// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client_android.h"

#include <vector>

#include "base/android/content_uri_utils.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "chrome/common/chrome_descriptors_android.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/posix_file_descriptor_info.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "url/gurl.h"
#include "url/url_constants.h"

// Helper function for GetAdditionalMappedFilesForChildProcess().
void GetMappedLocalePacksForChildProcess(
    content::PosixFileDescriptorInfo* mappings) {
  const std::vector<ui::ResourceBundle::FdAndRegion>& locale_paks =
      ui::GetLocalePaks();
  // We can have up to 2 locale paks here: the main (potentially gendered) pak,
  // and optionally a default-gender fallback pak if a string doesn't exist in
  // the gendered pak.
  CHECK_GE(locale_paks.size(), 1u);
  CHECK_LE(locale_paks.size(), 2u);

  CHECK_GE(locale_paks[0].fd, 0);
  mappings->ShareWithRegion(kAndroidMainLocalePakDescriptor, locale_paks[0].fd,
                            locale_paks[0].region);
  if (locale_paks.size() == 2u) {
    CHECK_GE(locale_paks[1].fd, 0);
    mappings->ShareWithRegion(kAndroidFallbackLocalePakDescriptor,
                              locale_paks[1].fd, locale_paks[1].region);
  }
}

bool IsContentSchemeURLNavigationAllowed(const GURL& url) {
  CHECK(url.SchemeIs(url::kContentScheme));
  if (!base::FeatureList::IsEnabled(
          features::kBlockNonExportedContentProviderNavigation)) {
    return true;
  }
  if (!url.is_valid() || url.host().empty()) {
    return false;
  }
  return base::ResolveContentUriOwner(base::FilePath(url.spec())) !=
         base::ContentUriOwner::kNonExportedFromCurrentApp;
}
