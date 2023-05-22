// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include <string>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/url_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/common/features.h"
#include "components/url_matcher/url_util.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_urls.h"
#include "google_apis/gaia/core_account_id.h"
#include "url/url_constants.h"

namespace supervised_user {

bool IsSupportedChromeExtensionURL(const GURL& effective_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  static const char* const kCrxDownloadUrls[] = {
      "https://clients2.googleusercontent.com/crx/blobs/",
      "https://chrome.google.com/webstore/download/"};

  // Chrome Webstore.
  if (extension_urls::IsWebstoreDomain(
          url_matcher::util::Normalize(effective_url))) {
    return true;
  }

  // Allow webstore crx downloads. This applies to both extension installation
  // and updates.
  if (extension_urls::GetWebstoreUpdateUrl() ==
      url_matcher::util::Normalize(effective_url)) {
    return true;
  }

  // The actual CRX files are downloaded from other URLs. Allow them too.
  // These URLs have https scheme.
  if (!effective_url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  for (const char* crx_download_url_str : kCrxDownloadUrls) {
    GURL crx_download_url(crx_download_url_str);
    if (crx_download_url.host_piece() == effective_url.host_piece() &&
        base::StartsWith(effective_url.path_piece(),
                         crx_download_url.path_piece(),
                         base::CompareCase::SENSITIVE)) {
      return true;
    }
  }
  return false;
#else
  return false;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

bool ShouldContentSkipParentAllowlistFiltering(content::WebContents* contents) {
  // Note that |contents| can be an inner WebContents. Get the outer most
  // WebContents and check if it belongs to the EDUCoexistence login flow.
  content::WebContents* outer_most_content =
      contents->GetOutermostWebContents();

  return outer_most_content->GetLastCommittedURL() ==
         GURL(chrome::kChromeUIEDUCoexistenceLoginURLV2);
}

ProfileSelections BuildProfileSelectionsForRegularAndGuest() {
  // Do not create for Incognito profile.
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOriginalOnly)
      .WithGuest(ProfileSelection::kRedirectedToOriginal)
      .Build();
}

ProfileSelections BuildProfileSelectionsLegacy() {
  CHECK(!base::FeatureList::IsEnabled(
      supervised_user::kUpdateSupervisedUserFactoryCreation));
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOriginalOnly)
      .WithGuest(ProfileSelection::kOriginalOnly)
      .Build();
}

std::string GetAccountGivenName(Profile& profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile);
  CHECK(identity_manager);

  const CoreAccountInfo core_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  const AccountInfo account_info =
      identity_manager->FindExtendedAccountInfo(core_info);
  return account_info.given_name;
}

}  // namespace supervised_user
