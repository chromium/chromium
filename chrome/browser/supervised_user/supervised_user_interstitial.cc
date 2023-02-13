// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_interstitial.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/web_approvals_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/supervised_user/child_accounts/child_account_feedback_reporter_android.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/supervised_user/chromeos/supervised_user_favicon_request_handler.h"
#endif

using content::WebContents;

namespace {

class TabCloser : public content::WebContentsUserData<TabCloser> {
 public:
  TabCloser(const TabCloser&) = delete;
  TabCloser& operator=(const TabCloser&) = delete;

  ~TabCloser() override {}

  static void MaybeClose(WebContents* web_contents) {
    DCHECK(web_contents);

    // Close the tab only if there is a browser for it (which is not the case
    // for example in a <webview>).
#if !BUILDFLAG(IS_ANDROID)
    if (!chrome::FindBrowserWithWebContents(web_contents))
      return;
#endif
    TabCloser::CreateForWebContents(web_contents);
  }

 private:
  friend class content::WebContentsUserData<TabCloser>;

  explicit TabCloser(WebContents* web_contents)
      : content::WebContentsUserData<TabCloser>(*web_contents) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&TabCloser::CloseTabImpl,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  void CloseTabImpl() {
    // On Android, FindBrowserWithWebContents and TabStripModel don't exist.
#if !BUILDFLAG(IS_ANDROID)
    Browser* browser = chrome::FindBrowserWithWebContents(&GetWebContents());
    DCHECK(browser);
    TabStripModel* tab_strip = browser->tab_strip_model();
    DCHECK_NE(TabStripModel::kNoTab,
              tab_strip->GetIndexOfWebContents(&GetWebContents()));
    if (tab_strip->count() <= 1) {
      // Don't close the last tab in the window.
      GetWebContents().RemoveUserData(UserDataKey());
      return;
    }
#endif
    GetWebContents().Close();
  }

  base::WeakPtrFactory<TabCloser> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabCloser);

// Removes all the infobars which are attached to |web_contents| and for
// which ShouldExpire() returns true.
void CleanUpInfoBar(content::WebContents* web_contents) {
  infobars::ContentInfoBarManager* manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  if (manager) {
    content::LoadCommittedDetails details;
    // |details.is_same_document| is default false, and |details.is_main_frame|
    // is default true. This results in is_navigation_to_different_page()
    // returning true.
    DCHECK(details.is_navigation_to_different_page());
    content::NavigationController& controller = web_contents->GetController();
    details.entry = controller.GetVisibleEntry();
    if (controller.GetLastCommittedEntry()) {
      details.previous_entry_index = controller.GetLastCommittedEntryIndex();
      details.previous_main_frame_url =
          controller.GetLastCommittedEntry()->GetURL();
    }
    for (int i = manager->infobar_count() - 1; i >= 0; --i) {
      infobars::InfoBar* infobar = manager->infobar_at(i);
      if (infobar->delegate()->ShouldExpire(
              infobars::ContentInfoBarManager::
                  NavigationDetailsFromLoadCommittedDetails(details)))
        manager->RemoveInfoBar(infobar);
    }
  }
}

// TODO(b/250924204): Implement shared logic to get the user's given name.
std::u16string GetActiveUserFirstName() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return user_manager::UserManager::Get()->GetActiveUser()->GetGivenName();
#else
  // TODO(b/243656773): Implement for LaCrOS.
  return std::u16string();
#endif
}
}  // namespace

// static
std::unique_ptr<SupervisedUserInterstitial> SupervisedUserInterstitial::Create(
    WebContents* web_contents,
    const GURL& url,
    supervised_user::FilteringBehaviorReason reason,
    int frame_id,
    int64_t interstitial_navigation_id) {
  std::unique_ptr<SupervisedUserInterstitial> interstitial =
      base::WrapUnique(new SupervisedUserInterstitial(
          web_contents, url, reason, frame_id, interstitial_navigation_id));

  if (web_contents->GetPrimaryMainFrame()->GetFrameTreeNodeId() == frame_id)
    CleanUpInfoBar(web_contents);

  // Caller is responsible for deleting the interstitial.
  return interstitial;
}

SupervisedUserInterstitial::SupervisedUserInterstitial(
    WebContents* web_contents,
    const GURL& url,
    supervised_user::FilteringBehaviorReason reason,
    int frame_id,
    int64_t interstitial_navigation_id)
    : web_contents_(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      url_(url),
      reason_(reason),
      frame_id_(frame_id),
      interstitial_navigation_id_(interstitial_navigation_id) {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  if (supervised_user::IsLocalWebApprovalsEnabled()) {
    favicon_handler_ = std::make_unique<SupervisedUserFaviconRequestHandler>(
        url_.GetWithEmptyPath(),
        LargeIconServiceFactory::GetForBrowserContext(profile_));
    // Prefetch the favicon which will be rendered as part of the web approvals
    // ParentAccessDialog. Pass in DoNothing() for the favicon fetched callback
    // because if the favicon is by the time the user triggers the opening of
    // the ParentAccessDialog, we show the default favicon.
    favicon_handler_->StartFaviconFetch(base::DoNothing());
  }
#endif
}

SupervisedUserInterstitial::~SupervisedUserInterstitial() {}

// static
std::string SupervisedUserInterstitial::GetHTMLContents(
    Profile* profile,
    supervised_user::FilteringBehaviorReason reason,
    bool already_sent_request,
    bool is_main_frame) {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);

  std::string custodian = supervised_user_service->GetCustodianName();
  std::string second_custodian =
      supervised_user_service->GetSecondCustodianName();
  std::string custodian_email =
      supervised_user_service->GetCustodianEmailAddress();
  std::string second_custodian_email =
      supervised_user_service->GetSecondCustodianEmailAddress();
  std::string profile_image_url = profile->GetPrefs()->GetString(
      prefs::kSupervisedUserCustodianProfileImageURL);
  std::string profile_image_url2 = profile->GetPrefs()->GetString(
      prefs::kSupervisedUserSecondCustodianProfileImageURL);

  bool allow_access_requests = supervised_user_service->web_approvals_manager()
                                   .AreRemoteApprovalRequestsEnabled();

  return supervised_user::BuildErrorPageHtml(
      allow_access_requests, profile_image_url, profile_image_url2, custodian,
      custodian_email, second_custodian, second_custodian_email, reason,
      g_browser_process->GetApplicationLocale(), already_sent_request,
      is_main_frame);
}

void SupervisedUserInterstitial::GoBack() {
  // GoBack only for main frame.
  DCHECK_EQ(web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId(),
            frame_id());

  UMA_HISTOGRAM_ENUMERATION(kInterstitialCommandHistogramName, Commands::BACK,
                            Commands::HISTOGRAM_BOUNDING_VALUE);
  AttemptMoveAwayFromCurrentFrameURL();
  OnInterstitialDone();
}

void SupervisedUserInterstitial::RequestUrlAccessRemote(
    base::OnceCallback<void(bool)> callback) {
  UMA_HISTOGRAM_ENUMERATION(kInterstitialCommandHistogramName,
                            Commands::REMOTE_ACCESS_REQUEST,
                            Commands::HISTOGRAM_BOUNDING_VALUE);
  OutputRequestPermissionSourceMetric();

  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  supervised_user_service->web_approvals_manager().RequestRemoteApproval(
      url_, std::move(callback));
}

void SupervisedUserInterstitial::RequestUrlAccessLocal(
    base::OnceCallback<void(bool)> callback) {
  UMA_HISTOGRAM_ENUMERATION(kInterstitialCommandHistogramName,
                            Commands::LOCAL_ACCESS_REQUEST,
                            Commands::HISTOGRAM_BOUNDING_VALUE);
  OutputRequestPermissionSourceMetric();

  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  gfx::ImageSkia favicon;
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  favicon = favicon_handler_->GetFaviconOrFallback();
#endif
  supervised_user_service->web_approvals_manager().RequestLocalApproval(
      web_contents(), url_, GetActiveUserFirstName(), favicon,
      std::move(callback));
}

void SupervisedUserInterstitial::ShowFeedback() {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  std::string second_custodian =
      supervised_user_service->GetSecondCustodianName();

  std::u16string reason = l10n_util::GetStringUTF16(
      supervised_user::GetBlockMessageID(reason_, second_custodian.empty()));
  std::string message = l10n_util::GetStringFUTF8(
      IDS_BLOCK_INTERSTITIAL_DEFAULT_FEEDBACK_TEXT, reason);
#if BUILDFLAG(IS_ANDROID)
  ReportChildAccountFeedback(web_contents_, message, url_);
#else
  chrome::ShowFeedbackPage(
      url_, profile_, chrome::kFeedbackSourceSupervisedUserInterstitial,
      message, std::string() /* description_placeholder_text */,
      std::string() /* category_tag */, std::string() /* extra_diagnostics */);
#endif
  return;
}

void SupervisedUserInterstitial::AttemptMoveAwayFromCurrentFrameURL() {
  // No need to do anything if the WebContents is in the process of being
  // destroyed anyway.
  if (web_contents_->IsBeingDestroyed())
    return;

  // If the interstitial was shown over an existing page, navigate back from
  // that page. If that is not possible, attempt to close the entire tab.
  if (web_contents_->GetController().CanGoBack()) {
    web_contents_->GetController().GoBack();
    return;
  }

  TabCloser::MaybeClose(web_contents_);
}

void SupervisedUserInterstitial::OnInterstitialDone() {
  auto* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(web_contents_);

  // After this, the WebContents may be destroyed. Make sure we don't try to use
  // it again.
  web_contents_ = nullptr;
  navigation_observer->OnInterstitialDone(frame_id_);
}

void SupervisedUserInterstitial::OutputRequestPermissionSourceMetric() {
  RequestPermissionSource source;
  if (web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId() == frame_id())
    source = RequestPermissionSource::MAIN_FRAME;
  else
    source = RequestPermissionSource::SUB_FRAME;

  UMA_HISTOGRAM_ENUMERATION(kInterstitialPermissionSourceHistogramName, source,
                            RequestPermissionSource::HISTOGRAM_BOUNDING_VALUE);
}
