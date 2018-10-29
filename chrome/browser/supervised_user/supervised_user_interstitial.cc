// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_interstitial.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/supervised_user/child_accounts/child_account_feedback_reporter_android.h"
#else
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

using content::BrowserThread;
using content::WebContents;

namespace {

// For use in histograms.
enum Commands { PREVIEW, BACK, NTP, ACCESS_REQUEST, HISTOGRAM_BOUNDING_VALUE };

class TabCloser : public content::WebContentsUserData<TabCloser> {
 public:
  ~TabCloser() override {}

  static void MaybeClose(WebContents* web_contents) {
    DCHECK(web_contents);

    // Close the tab only if there is a browser for it (which is not the case
    // for example in a <webview>).
#if !defined(OS_ANDROID)
    if (!chrome::FindBrowserWithWebContents(web_contents))
      return;
#endif
    TabCloser::CreateForWebContents(web_contents);
  }

 private:
  friend class content::WebContentsUserData<TabCloser>;

  explicit TabCloser(WebContents* web_contents)
      : web_contents_(web_contents), weak_ptr_factory_(this) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::Bind(&TabCloser::CloseTabImpl, weak_ptr_factory_.GetWeakPtr()));
  }

  void CloseTabImpl() {
    // On Android, FindBrowserWithWebContents and TabStripModel don't exist.
#if !defined(OS_ANDROID)
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
    DCHECK(browser);
    TabStripModel* tab_strip = browser->tab_strip_model();
    DCHECK_NE(TabStripModel::kNoTab,
              tab_strip->GetIndexOfWebContents(web_contents_));
    if (tab_strip->count() <= 1) {
      // Don't close the last tab in the window.
      web_contents_->RemoveUserData(UserDataKey());
      return;
    }
#endif
    web_contents_->Close();
  }

  WebContents* web_contents_;
  base::WeakPtrFactory<TabCloser> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabCloser);
};

}  // namespace

const content::InterstitialPageDelegate::TypeID
    SupervisedUserInterstitial::kTypeForTesting =
        &SupervisedUserInterstitial::kTypeForTesting;

// TODO(carlosil): Remove Show function and the rest of non-committed
// interstitials code once committed interstitials are the only code path.
// static
void SupervisedUserInterstitial::Show(
    WebContents* web_contents,
    const GURL& url,
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool initial_page_load,
    const base::Callback<void(bool)>& callback) {
  DCHECK(!base::FeatureList::IsEnabled(
      features::kSupervisedUserCommittedInterstitials));
  // |interstitial_page_| is responsible for deleting the interstitial.
  SupervisedUserInterstitial* interstitial = new SupervisedUserInterstitial(
      web_contents, url, reason, initial_page_load, callback);

  interstitial->Init();
}

// static
std::unique_ptr<SupervisedUserInterstitial> SupervisedUserInterstitial::Create(
    WebContents* web_contents,
    const GURL& url,
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool initial_page_load,
    const base::Callback<void(bool)>& callback) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kSupervisedUserCommittedInterstitials));
  std::unique_ptr<SupervisedUserInterstitial> interstitial(
      new SupervisedUserInterstitial(web_contents, url, reason,
                                     initial_page_load, callback));

  // Caller is responsible for deleting the interstitial.
  interstitial->Init();

  return interstitial;
}

SupervisedUserInterstitial::SupervisedUserInterstitial(
    WebContents* web_contents,
    const GURL& url,
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool initial_page_load,
    const base::Callback<void(bool)>& callback)
    : web_contents_(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      interstitial_page_(NULL),
      url_(url),
      reason_(reason),
      initial_page_load_(initial_page_load),
      proceeded_(false),
      callback_(callback),
      scoped_observer_(this),
      weak_ptr_factory_(this) {}

SupervisedUserInterstitial::~SupervisedUserInterstitial() {}

void SupervisedUserInterstitial::Init() {
  DCHECK(!ShouldProceed());

  InfoBarService* service = InfoBarService::FromWebContents(web_contents_);
  if (service) {
    // Remove all the infobars which are attached to |web_contents_| and for
    // which ShouldExpire() returns true.
    content::LoadCommittedDetails details;
    // |details.is_same_document| is default false, and |details.is_main_frame|
    // is default true. This results in is_navigation_to_different_page()
    // returning true.
    DCHECK(details.is_navigation_to_different_page());
    const content::NavigationController& controller =
        web_contents_->GetController();
    details.entry = controller.GetVisibleEntry();
    if (controller.GetLastCommittedEntry()) {
      details.previous_entry_index = controller.GetLastCommittedEntryIndex();
      details.previous_url = controller.GetLastCommittedEntry()->GetURL();
    }
    details.type = content::NAVIGATION_TYPE_NEW_PAGE;
    for (int i = service->infobar_count() - 1; i >= 0; --i) {
      infobars::InfoBar* infobar = service->infobar_at(i);
      if (infobar->delegate()->ShouldExpire(
              InfoBarService::NavigationDetailsFromLoadCommittedDetails(
                  details)))
        service->RemoveInfoBar(infobar);
    }
  }

  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  scoped_observer_.Add(supervised_user_service);

  if (!base::FeatureList::IsEnabled(
          features::kSupervisedUserCommittedInterstitials)) {
    // If committed interstitials are enabled we do not create an
    // interstitial_page
    interstitial_page_ = content::InterstitialPage::Create(
        web_contents_, initial_page_load_, url_, this);
    interstitial_page_->Show();
  }
}

// static
std::string SupervisedUserInterstitial::GetHTMLContents(
    Profile* profile,
    supervised_user_error_page::FilteringBehaviorReason reason) {
  bool is_child_account = profile->IsChild();

  bool is_deprecated = !is_child_account;

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

  bool allow_access_requests = supervised_user_service->AccessRequestsEnabled();

  return supervised_user_error_page::BuildHtml(
      allow_access_requests, profile_image_url, profile_image_url2, custodian,
      custodian_email, second_custodian, second_custodian_email,
      is_child_account, is_deprecated, reason,
      g_browser_process->GetApplicationLocale());
}

std::string SupervisedUserInterstitial::GetHTMLContents() {
  return GetHTMLContents(profile_, reason_);
}

void SupervisedUserInterstitial::CommandReceived(const std::string& command) {
  if (command == "\"back\"") {
    UMA_HISTOGRAM_ENUMERATION("ManagedMode.BlockingInterstitialCommand",
                              BACK,
                              HISTOGRAM_BOUNDING_VALUE);
    if (base::FeatureList::IsEnabled(
            features::kSupervisedUserCommittedInterstitials)) {
      DontProceedInternal();
    } else {
      interstitial_page_->DontProceed();
    }
    return;
  }

  if (command == "\"request\"") {
    UMA_HISTOGRAM_ENUMERATION("ManagedMode.BlockingInterstitialCommand",
                              ACCESS_REQUEST,
                              HISTOGRAM_BOUNDING_VALUE);

    SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(profile_);
    supervised_user_service->AddURLAccessRequest(
        url_, base::Bind(&SupervisedUserInterstitial::OnAccessRequestAdded,
                         weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  base::string16 second_custodian =
      base::UTF8ToUTF16(supervised_user_service->GetSecondCustodianName());

  if (command == "\"feedback\"") {
    bool is_child_account = profile_->IsChild();
    base::string16 reason =
        l10n_util::GetStringUTF16(supervised_user_error_page::GetBlockMessageID(
            reason_, is_child_account, second_custodian.empty()));
    std::string message = l10n_util::GetStringFUTF8(
        IDS_BLOCK_INTERSTITIAL_DEFAULT_FEEDBACK_TEXT, reason);
#if defined(OS_ANDROID)
    DCHECK(is_child_account);
    ReportChildAccountFeedback(web_contents_, message, url_);
#else
    chrome::ShowFeedbackPage(chrome::FindBrowserWithWebContents(web_contents_),
                             chrome::kFeedbackSourceSupervisedUserInterstitial,
                             message,
                             std::string() /* description_placeholder_text */,
                             std::string() /* category_tag */,
                             std::string() /* extra_diagnostics */);
#endif
    return;
  }

  NOTREACHED();
}

void SupervisedUserInterstitial::RequestPermission(
    base::OnceCallback<void(bool)> RequestCallback) {
  UMA_HISTOGRAM_ENUMERATION("ManagedMode.BlockingInterstitialCommand",
                            ACCESS_REQUEST, HISTOGRAM_BOUNDING_VALUE);
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  supervised_user_service->AddURLAccessRequest(url_,
                                               std::move(RequestCallback));
}

void SupervisedUserInterstitial::OnProceed() {
  ProceedInternal();
}

void SupervisedUserInterstitial::OnDontProceed() {
  DontProceedInternal();
}

content::InterstitialPageDelegate::TypeID
SupervisedUserInterstitial::GetTypeForTesting() const {
  return SupervisedUserInterstitial::kTypeForTesting;
}

void SupervisedUserInterstitial::OnURLFilterChanged() {
  if (ShouldProceed()) {
    if (base::FeatureList::IsEnabled(
            features::kSupervisedUserCommittedInterstitials)) {
      ProceedInternal();
    } else {
      // Interstitial page deletes the interstitial when proceeding but not
      // synchronously, so a check is required to avoid calling proceed twice.
      if (!proceeded_)
        interstitial_page_->Proceed();
      proceeded_ = true;
    }
  }
}

void SupervisedUserInterstitial::OnAccessRequestAdded(bool success) {
  DCHECK(!base::FeatureList::IsEnabled(
      features::kSupervisedUserCommittedInterstitials));
  VLOG(1) << "Sent access request for " << url_.spec()
          << (success ? " successfully" : " unsuccessfully");
  std::string jsFunc =
      base::StringPrintf("setRequestStatus(%s);", success ? "true" : "false");
  if (interstitial_page_->GetMainFrame()) {
    interstitial_page_->GetMainFrame()->ExecuteJavaScript(
        base::ASCIIToUTF16(jsFunc));
  }
}

bool SupervisedUserInterstitial::ShouldProceed() {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  const SupervisedUserURLFilter* url_filter =
      supervised_user_service->GetURLFilter();
  SupervisedUserURLFilter::FilteringBehavior behavior;
  if (url_filter->HasAsyncURLChecker()) {
    if (!url_filter->GetManualFilteringBehaviorForURL(url_, &behavior))
      return false;
  } else {
    behavior = url_filter->GetFilteringBehaviorForURL(url_);
  }
  return behavior != SupervisedUserURLFilter::BLOCK;
}

void SupervisedUserInterstitial::MoveAwayFromCurrentPage() {
  // No need to do anything if the WebContents is in the process of being
  // destroyed anyway.
  if (web_contents_->IsBeingDestroyed())
    return;

  // If the interstitial was shown during a page load and there is no history
  // entry to go back to, attempt to close the tab.
  // This check is skipped when committed interstitials are on, because all
  // interstitials are treated as initial page loads in this case, the case
  // where there is nothing to go back to will be handled by the default case at
  // the end.
  if (!base::FeatureList::IsEnabled(
          features::kSupervisedUserCommittedInterstitials) &&
      initial_page_load_) {
    if (web_contents_->GetController().IsInitialBlankNavigation())
      TabCloser::MaybeClose(web_contents_);
    return;
  }

  // If the interstitial was shown over an existing page, navigate back from
  // that page. If that is not possible, attempt to close the entire tab.
  if (web_contents_->GetController().CanGoBack()) {
    web_contents_->GetController().GoBack();
    return;
  }

  TabCloser::MaybeClose(web_contents_);
}

void SupervisedUserInterstitial::DispatchContinueRequest(
    bool continue_request) {
  callback_.Run(continue_request);

  // After this, the WebContents may be destroyed. Make sure we don't try to use
  // it again.
  web_contents_ = nullptr;
}

void SupervisedUserInterstitial::ProceedInternal() {
  if (base::FeatureList::IsEnabled(
          features::kSupervisedUserCommittedInterstitials) &&
      web_contents_) {
    // In the committed interstitials case, there will be nothing to resume, so
    // refresh instead.
    web_contents_->GetController().Reload(content::ReloadType::NORMAL, true);
  }
  DispatchContinueRequest(true);
}

void SupervisedUserInterstitial::DontProceedInternal() {
  MoveAwayFromCurrentPage();
  DispatchContinueRequest(false);
}
