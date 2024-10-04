// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"

#include <string>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/supervised_user/core/browser/child_account_service.h"
#include "components/supervised_user/core/browser/supervised_user_capabilities.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/navigation_handle.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_urls.h"
#include "google_apis/gaia/core_account_id.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "chrome/browser/supervised_user/supervised_user_verification_controller_client.h"
#include "chrome/browser/supervised_user/supervised_user_verification_page_blocked_sites.h"
#include "chrome/browser/supervised_user/supervised_user_verification_page_youtube.h"
#endif

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

bool SupervisedUserCanSkipExtensionParentApprovals(const Profile* profile) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return profile->IsChild() &&
         IsSupervisedUserSkipParentApprovalToInstallExtensionsEnabled() &&
         profile->GetPrefs()->GetBoolean(
             prefs::kSkipParentApprovalToInstallExtensions);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

bool AreExtensionsPermissionsEnabled(Profile* profile) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(IS_CHROMEOS)
  return profile->IsChild();
#else
  return profile->IsChild() &&
         base::FeatureList::IsEnabled(
             kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
#endif  // BUILDFLAG(IS_CHROMEOS)
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
      // TODO(crbug.com/41488885): Check if this is needed for Ash Internals.
      .WithAshInternals(ProfileSelection::kOriginalOnly)
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

void AssertChildStatusOfTheUser(Profile* profile, bool is_child) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (user && is_child != (user->GetType() == user_manager::UserType::kChild)) {
    LOG(FATAL) << "User child flag has changed: " << is_child;
  }
  if (!user && ash::ProfileHelper::IsUserProfile(profile)) {
    LOG(FATAL) << "User instance not found while setting child account flag.";
  }
#endif
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
std::string CreateReauthenticationInterstitialForYouTube(
    content::NavigationHandle& navigation_handle) {
  content::WebContents* web_contents = navigation_handle.GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  GURL request_url = navigation_handle.GetURL();
  bool is_main_frame = navigation_handle.GetNavigatingFrameType() ==
                       content::FrameType::kPrimaryMainFrame;

  std::unique_ptr<SupervisedUserVerificationPageForYouTube> blocking_page =
      std::make_unique<SupervisedUserVerificationPageForYouTube>(
          web_contents, profile->GetProfileUserName(), request_url,
          ChildAccountServiceFactory::GetForProfile(profile),
          navigation_handle.GetNextPageUkmSourceId(),
          std::make_unique<SupervisedUserVerificationControllerClient>(
              web_contents, profile->GetPrefs(),
              g_browser_process->GetApplicationLocale(),
              GURL(chrome::kChromeUINewTabURL), request_url),
          is_main_frame);

  std::string interstitial_html = blocking_page->GetHTMLContents();
  security_interstitials::SecurityInterstitialTabHelper::AssociateBlockingPage(
      &navigation_handle, std::move(blocking_page));
  return interstitial_html;
}

std::string CreateReauthenticationInterstitialForBlockedSites(
    content::NavigationHandle& navigation_handle,
    FilteringBehaviorReason block_reason) {
  content::WebContents* web_contents = navigation_handle.GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);
  bool has_second_custodian =
      !supervised_user_service->GetSecondCustodianName().empty();
  GURL request_url = navigation_handle.GetURL();
  bool is_main_frame = navigation_handle.GetNavigatingFrameType() ==
                       content::FrameType::kPrimaryMainFrame;

  std::unique_ptr<SupervisedUserVerificationPageForBlockedSites> blocking_page =
      std::make_unique<SupervisedUserVerificationPageForBlockedSites>(
          web_contents, profile->GetProfileUserName(), request_url,
          ChildAccountServiceFactory::GetForProfile(profile),
          std::make_unique<SupervisedUserVerificationControllerClient>(
              web_contents, profile->GetPrefs(),
              g_browser_process->GetApplicationLocale(),
              GURL(chrome::kChromeUINewTabURL), request_url),
          block_reason, is_main_frame, has_second_custodian);

  std::string interstitial_html = blocking_page->GetHTMLContents();
  security_interstitials::SecurityInterstitialTabHelper::AssociateBlockingPage(
      &navigation_handle, std::move(blocking_page));
  return interstitial_html;
}
#endif

}  // namespace supervised_user
