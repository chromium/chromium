// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_ui_controller.h"

#include <algorithm>

#include "base/feature_list.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_observer.h"
#include "chrome/browser/enterprise/watermark/settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/enterprise/watermarking/content/watermark_text_container.h"
#include "components/enterprise/watermarking/watermark.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace enterprise_data_protection {

DataProtectionUIController::ContentsViewDataProtectionController::
    ContentsViewDataProtectionController(
        DataProtectionUIController* owner,
        ContentsContainerView* contents_container_view,
        PrefService* prefs)
    : owner_(owner),
      contents_container_view_(contents_container_view),
      prefs_(prefs) {
  ContentsWebView* contents_web_view = contents_container_view->contents_view();
  web_contents_attached_subscription_ =
      contents_web_view->AddWebContentsAttachedCallback(base::BindRepeating(
          &ContentsViewDataProtectionController::OnWebContentsAttached,
          base::Unretained(this)));
  web_contents_detached_subscription_ =
      contents_web_view->AddWebContentsDetachedCallback(base::BindRepeating(
          &ContentsViewDataProtectionController::OnWebContentsDetached,
          base::Unretained(this)));
  OnWebContentsAttached(contents_web_view);
}

DataProtectionUIController::ContentsViewDataProtectionController::
    ~ContentsViewDataProtectionController() = default;

void DataProtectionUIController::ContentsViewDataProtectionController::
    OnWebContentsAttached(views::WebView* web_view) {
  if (!web_view->web_contents()) {
    return;
  }

  tabs::TabInterface* tab =
      tabs::TabInterface::GetFromContents(web_view->web_contents());

  enterprise_data_protection::DataProtectionNavigationController* controller =
      tab->GetTabFeatures()->data_protection_controller();

  contents_container_view_->ApplyWatermarkSettings(
      controller->watermark_text(), enterprise_watermark::GetFillColor(prefs_),
      enterprise_watermark::GetOutlineColor(prefs_),
      enterprise_watermark::GetFontSize(prefs_));

  watermark_subscription_ =
      controller->RegisterWatermarkStringUpdatedCallback(base::BindRepeating(
          &ContentsViewDataProtectionController::OnWatermarkStringUpdated,
          base::Unretained(this)));
#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
  owner_->MaybeUpdateScreenshotSettings();

  screenshot_subscription_ =
      controller->RegisterScreenshotAllowedUpdatedCallback(base::BindRepeating(
          &ContentsViewDataProtectionController::OnScreenshotAllowedUpdated,
          base::Unretained(this)));
#endif
}

void DataProtectionUIController::ContentsViewDataProtectionController::
    OnWebContentsDetached(views::WebView* web_view) {
  watermark_subscription_ = base::CallbackListSubscription();
#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
  screenshot_subscription_ = base::CallbackListSubscription();
#endif
}

void DataProtectionUIController::ContentsViewDataProtectionController::
    OnWatermarkStringUpdated(const std::string& watermark_string) {
  contents_container_view_->ApplyWatermarkSettings(
      watermark_string, enterprise_watermark::GetFillColor(prefs_),
      enterprise_watermark::GetOutlineColor(prefs_),
      enterprise_watermark::GetFontSize(prefs_));
}

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
void DataProtectionUIController::ContentsViewDataProtectionController::
    OnScreenshotAllowedUpdated(bool allowed) {
  owner_->MaybeUpdateScreenshotSettings();
}
#endif

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
bool DataProtectionUIController::ContentsViewDataProtectionController::
    GetScreenshotAllowed() {
  // It is possible with splits in an intermediate state that there is no
  // attached webcontents to the `contents_container_view_` This can happen
  // during reversing of splits since the webcontents could be detached from one
  // of the views.
  if (!contents_container_view_->GetVisible() ||
      !contents_container_view_->contents_view()->web_contents()) {
    return true;
  }

  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(
      contents_container_view_->contents_view()->web_contents());

  enterprise_data_protection::DataProtectionNavigationController* controller =
      tab->GetTabFeatures()->data_protection_controller();

  return controller->screenshot_allowed();
}
#endif

DEFINE_USER_DATA(DataProtectionUIController);

DataProtectionUIController::DataProtectionUIController(
    BrowserView* browser_view)
    : browser_view_(browser_view),
      scoped_data_holder_(browser_view->browser()->GetUnownedUserDataHost(),
                          *this) {
  for (ContentsContainerView* contents_container_view :
       browser_view->GetContentsContainerViews()) {
    contents_view_data_protection_controllers_.push_back(
        std::make_unique<ContentsViewDataProtectionController>(
            this, contents_container_view,
            browser_view->GetProfile()->GetPrefs()));
  }
}

// static
DataProtectionUIController* DataProtectionUIController::From(
    BrowserWindowInterface* browser_window_interface) {
  return ui::ScopedUnownedUserData<DataProtectionUIController>::Get(
      browser_window_interface->GetUnownedUserDataHost());
}

void DataProtectionUIController::ApplyWatermarkSettings(
    const std::string& watermark_text,
    SkColor fill_color,
    SkColor outline_color,
    int font_size) {
  for (ContentsContainerView* contents_container_view :
       browser_view_->GetContentsContainerViews()) {
    contents_container_view->ApplyWatermarkSettings(watermark_text, fill_color,
                                                    outline_color, font_size);
  }
}

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
void DataProtectionUIController::MaybeUpdateScreenshotSettings() {
  bool disallow_screenshot =
      std::any_of(contents_view_data_protection_controllers_.begin(),
                  contents_view_data_protection_controllers_.end(),
                  [](const std::unique_ptr<
                      enterprise_data_protection::DataProtectionUIController::
                          ContentsViewDataProtectionController>& controller) {
                    return !controller->GetScreenshotAllowed();
                  });

  browser_view_->ApplyScreenshotSettings(!disallow_screenshot);
}
#endif

DataProtectionUIController::~DataProtectionUIController() = default;

}  // namespace enterprise_data_protection
