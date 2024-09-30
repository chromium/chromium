// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_ui_controller.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/bubble/download_bubble_utils.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/common/pref_names.h"
#include "components/download/public/common/download_item.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/strings/string_util.h"
#include "chrome/browser/download/android/download_controller.h"
#include "chrome/browser/download/android/download_controller_base.h"
#include "components/pdf/common/constants.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/common/content_features.h"
#else
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/bubble/download_bubble_update_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/download/notification/download_notification_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

// DownloadShelfUIControllerDelegate{Android,} is used when a
// DownloadUIController is
// constructed without specifying an explicit Delegate.
#if BUILDFLAG(IS_ANDROID)

class AndroidUIControllerDelegate : public DownloadUIController::Delegate {
 public:
  AndroidUIControllerDelegate() {}
  ~AndroidUIControllerDelegate() override {}

 private:
  // DownloadUIController::Delegate
  void OnNewDownloadReady(download::DownloadItem* item) override;
};

void AndroidUIControllerDelegate::OnNewDownloadReady(
    download::DownloadItem* item) {
  DownloadControllerBase::Get()->OnDownloadStarted(item);
}

#else  // BUILDFLAG(IS_ANDROID)

void InitializeDownloadBubbleUpdateService(Profile* profile,
                                           content::DownloadManager* manager) {
  DownloadBubbleUpdateService* download_bubble_update_service =
      DownloadBubbleUpdateServiceFactory::GetForProfile(profile);
  if (!download_bubble_update_service) {
    return;
  }
  download_bubble_update_service->Initialize(manager);
}

class DownloadShelfUIControllerDelegate
    : public DownloadUIController::Delegate {
 public:
  // |profile| is required to outlive DownloadShelfUIControllerDelegate.
  explicit DownloadShelfUIControllerDelegate(Profile* profile)
      : profile_(profile) {}
  ~DownloadShelfUIControllerDelegate() override {}

 private:
  // DownloadUIController::Delegate
  void OnNewDownloadReady(download::DownloadItem* item) override;

  raw_ptr<Profile> profile_;
};

void DownloadShelfUIControllerDelegate::OnNewDownloadReady(
    download::DownloadItem* item) {
  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(item);
  // For the case of DevTools web contents, we'd like to use target browser
  // shelf although saving from the DevTools web contents.
  if (web_contents && DevToolsWindow::IsDevToolsWindow(web_contents)) {
    DevToolsWindow* devtools_window =
        DevToolsWindow::AsDevToolsWindow(web_contents);
    content::WebContents* inspected =
        devtools_window->GetInspectedWebContents();
    // Do not overwrite web contents for the case of remote debugging.
    if (inspected)
      web_contents = inspected;
  }
  Browser* browser =
      web_contents ? chrome::FindBrowserWithTab(web_contents) : nullptr;

  // As a last resort, use the last active browser for this profile. Not ideal,
  // but better than not showing the download at all.
  if (browser == nullptr)
    browser = chrome::FindLastActiveWithProfile(profile_);

  if (browser && browser->window() && browser->window()->GetDownloadShelf() &&
      DownloadItemModel(item).ShouldShowInShelf()) {
    DownloadUIModel::DownloadUIModelPtr model = DownloadItemModel::Wrap(item);

    // GetDownloadShelf creates the download shelf if it was not yet created.
    browser->window()->GetDownloadShelf()->AddDownload(std::move(model));
  }
}

class DownloadBubbleUIControllerDelegate
    : public DownloadUIController::Delegate {
 public:
  // |profile| is required to outlive DownloadBubbleUIControllerDelegate.
  explicit DownloadBubbleUIControllerDelegate(Profile* profile)
      : profile_(profile) {
    if (profile_->IsOffTheRecord()) {
      profile_->GetPrefs()->SetBoolean(prefs::kPromptForDownload, true);
    }
  }
  ~DownloadBubbleUIControllerDelegate() override = default;

 private:
  // DownloadUIController::Delegate
  void OnNewDownloadReady(download::DownloadItem* item) override;
  void OnButtonClicked() override;

  raw_ptr<Profile> profile_;
};

void DownloadBubbleUIControllerDelegate::OnNewDownloadReady(
    download::DownloadItem* item) {
  if (!DownloadItemModel(item).ShouldShowInBubble())
    return;
  // crx downloads are handled by the DownloadBubbleUpdateService.
  // TODO(chlily): Consolidate these code paths.
  if (download_crx_util::IsExtensionDownload(*item)) {
    return;
  }

  DownloadBubbleUpdateService* download_bubble_update_service =
      DownloadBubbleUpdateServiceFactory::GetForProfile(profile_);
  if (!download_bubble_update_service) {
    return;
  }
  download_bubble_update_service->NotifyWindowsOfDownloadItemAdded(item);
}

void DownloadBubbleUIControllerDelegate::OnButtonClicked() {
  BrowserList* browser_list = BrowserList::GetInstance();
  if (!browser_list)
    return;

  for (Browser* browser : *browser_list) {
    if (browser && browser->window() &&
        browser->window()->GetDownloadBubbleUIController()) {
      browser->window()->GetDownloadBubbleUIController()->HandleButtonPressed();
    }
  }
}

#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)

// A composite `DownloadUIController::Delegate` for use exclusively on ChromeOS.
// TODO(http://b/279791981): Remove after enabling the new downloads integration
// with System UI surfaces and deprecating `DownloadNotificationManager`.
class CrOSUIControllerDelegate : public DownloadUIController::Delegate {
 public:
  explicit CrOSUIControllerDelegate(content::DownloadManager* manager) {
    // Conditionally add the `DownloadBubbleUIControllerDelegate`.
    auto* profile = Profile::FromBrowserContext(manager->GetBrowserContext());
    if (download::IsDownloadBubbleEnabled()) {
      delegates_.emplace_back(
          std::make_unique<DownloadBubbleUIControllerDelegate>(profile));
      InitializeDownloadBubbleUpdateService(profile, manager);
    }

    // Generally the `DownloadNotificationManager` should always be added as it
    // provides System UI notifications on ChromeOS.
    bool add_download_notification_manager = true;

    // In Lacros, the `DownloadNotificationManager` should be added if and only
    // if the new downloads integration with System UI surfaces is disabled.
    // This ensures that exactly one System UI notification provider exists.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    if (auto* proxy = chromeos::BrowserParamsProxy::Get();
        proxy && proxy->IsSysUiDownloadsIntegrationV2Enabled()) {
      add_download_notification_manager = false;
    }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    if (add_download_notification_manager) {
      delegates_.emplace_back(
          std::make_unique<DownloadNotificationManager>(profile));
    }
  }

  CrOSUIControllerDelegate(const CrOSUIControllerDelegate&) = delete;
  CrOSUIControllerDelegate& operator=(const CrOSUIControllerDelegate&) = delete;
  ~CrOSUIControllerDelegate() override = default;

 private:
  // DownloadUIController::Delegate:
  void OnNewDownloadReady(download::DownloadItem* item) override {
    for (auto& delegate : delegates_) {
      delegate->OnNewDownloadReady(item);
    }
  }

  void OnButtonClicked() override {
    for (auto& delegate : delegates_) {
      delegate->OnButtonClicked();
    }
  }

  // The collection of delegates contained by this composite.
  std::vector<std::unique_ptr<DownloadUIController::Delegate>> delegates_;
};

#endif  // BUILDFLAG(IS_CHROMEOS)

} // namespace

DownloadUIController::Delegate::~Delegate() {
}

void DownloadUIController::Delegate::OnButtonClicked() {}

DownloadUIController::DownloadUIController(content::DownloadManager* manager,
                                           std::unique_ptr<Delegate> delegate)
    : download_notifier_(manager, this), delegate_(std::move(delegate)) {
#if BUILDFLAG(IS_ANDROID)
  if (!delegate_)
    delegate_ = std::make_unique<AndroidUIControllerDelegate>();
#elif BUILDFLAG(IS_CHROMEOS)
  if (!delegate_) {
    delegate_ = std::make_unique<CrOSUIControllerDelegate>(manager);
  }
#else   // BUILDFLAG(IS_CHROMEOS)
  if (!delegate_) {
    Profile* profile =
        Profile::FromBrowserContext(manager->GetBrowserContext());
    if (download::IsDownloadBubbleEnabled()) {
      delegate_ = std::make_unique<DownloadBubbleUIControllerDelegate>(profile);
      InitializeDownloadBubbleUpdateService(profile, manager);
    } else {
      delegate_ = std::make_unique<DownloadShelfUIControllerDelegate>(profile);
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

DownloadUIController::~DownloadUIController() = default;

void DownloadUIController::OnButtonClicked() {
  delegate_->OnButtonClicked();
}

void DownloadUIController::OnDownloadCreated(content::DownloadManager* manager,
                                             download::DownloadItem* item) {
  // Record the security level of the page triggering the download. Only record
  // when the download occurs in the WebContents that initiated the download
  // (e.g., not downloads in new tabs or windows, which have a different
  // WebContents).
  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(item);
  if (web_contents && (item->IsSavePackageDownload() ||
                       (web_contents->GetURL() != item->GetOriginalUrl() &&
                        web_contents->GetURL() != item->GetURL()))) {
    auto* security_state_tab_helper =
        SecurityStateTabHelper::FromWebContents(web_contents);
    if (security_state_tab_helper) {
      UMA_HISTOGRAM_ENUMERATION("Security.SecurityLevel.DownloadStarted",
                                security_state_tab_helper->GetSecurityLevel(),
                                security_state::SECURITY_LEVEL_COUNT);
    }
  }

  if (web_contents) {
    // TODO(crbug.com/40169435): Add test for this metric.
    RecordDownloadStartPerProfileType(
        Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  }

  // SavePackage downloads are created in a state where they can be shown in the
  // browser. Call OnDownloadUpdated() once to notify the UI immediately.
  OnDownloadUpdated(manager, item);
}

void DownloadUIController::OnDownloadUpdated(content::DownloadManager* manager,
                                             download::DownloadItem* item) {
  DownloadItemModel item_model(item);

  bool needs_to_render = false;
#if BUILDFLAG(IS_ANDROID)
  if (manager && manager->GetDelegate() &&
      manager->GetDelegate()->ShouldOpenPdfInline() &&
      !item->IsMustDownload() &&
      item->GetState() == download::DownloadItem::IN_PROGRESS &&
      base::EqualsCaseInsensitiveASCII(item->GetMimeType(),
                                       pdf::kPDFMimeType)) {
    needs_to_render = true;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // Ignore if we've already notified the UI about |item| or if it isn't a new
  // download.
  if (item_model.WasUINotified() ||
      (!item_model.ShouldNotifyUI() && !needs_to_render)) {
    return;
  }

  // Downloads blocked by local policies should be notified, otherwise users
  // won't get any feedback that the download has failed.
  bool should_notify =
      item->GetLastReason() ==
          download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED &&
      item->GetInsecureDownloadStatus() !=
          download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK;

  // Wait until the target path is determined or the download is canceled.
  if (item->GetTargetFilePath().empty() &&
      item->GetState() != download::DownloadItem::CANCELLED && !should_notify) {
    return;
  }

  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(item);
  if (web_contents) {
#if BUILDFLAG(IS_ANDROID)
    if (!needs_to_render) {
      DownloadController::CloseTabIfEmpty(web_contents, item);
    }
#else   // BUILDFLAG(IS_ANDROID)
    Browser* browser = chrome::FindBrowserWithTab(web_contents);
    // If the download occurs in a new tab, and it's not a save page
    // download (started before initial navigation completed) close it.
    // Avoid calling CloseContents if the tab is not in this browser's tab strip
    // model; this can happen if the download was initiated by something
    // internal to Chrome, such as by the app list.
    if (browser && web_contents->GetController().IsInitialNavigation() &&
        browser->tab_strip_model()->count() > 1 &&
        browser->tab_strip_model()->GetIndexOfWebContents(web_contents) !=
            TabStripModel::kNoTab &&
        !item->IsSavePackageDownload()) {
      web_contents->Close();
    }
#endif  // BUILDFLAG(IS_ANDROID)
  }

  if (item->GetState() == download::DownloadItem::CANCELLED)
    return;

  DownloadItemModel(item).SetWasUINotified(true);
  delegate_->OnNewDownloadReady(item);
}
