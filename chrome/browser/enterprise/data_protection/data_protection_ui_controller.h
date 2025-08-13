// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_UI_CONTROLLER_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_UI_CONTROLLER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace views {
class WebView;
}

class ContentsContainerView;
class BrowserView;
class BrowserWindowInterface;
class PrefService;

namespace enterprise_data_protection {

// Browser level controller that owns per contents view controllers to
// show watermarks and apply screenshot policy to the browser.
class DataProtectionUIController {
 public:
  explicit DataProtectionUIController(BrowserView* browser_view);
  ~DataProtectionUIController();

  DECLARE_USER_DATA(DataProtectionUIController);

  static DataProtectionUIController* From(
      BrowserWindowInterface* browser_window_interface);

  // controller tied to a `contents_container_view` that is responsible for
  // setting and clearing watermark on the view. It is also responsible for
  // notifying DataProtectionUIController to update screenshot policy.
  class ContentsViewDataProtectionController {
   public:
    ContentsViewDataProtectionController(
        DataProtectionUIController* owner,
        ContentsContainerView* contents_container_view,
        PrefService* prefs);
    ~ContentsViewDataProtectionController();

    ContentsViewDataProtectionController(
        const ContentsViewDataProtectionController&) = delete;
    ContentsViewDataProtectionController& operator=(
        const ContentsViewDataProtectionController&) = delete;

    // DataProtectionNavigationTabControllerObserver:
    void OnWatermarkStringUpdated(const std::string& watermark_string);

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
    void OnScreenshotAllowedUpdated(bool allowed);
#endif  // BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
    bool GetScreenshotAllowed();
#endif

   private:
    void OnWebContentsAttached(views::WebView* web_view);
    void OnWebContentsDetached(views::WebView* web_view);

    raw_ptr<DataProtectionUIController> owner_;
    raw_ptr<ContentsContainerView> contents_container_view_;
    raw_ptr<PrefService> prefs_;

    // Subscriptions for web contents changed
    base::CallbackListSubscription web_contents_attached_subscription_;
    base::CallbackListSubscription web_contents_detached_subscription_;

    // Subscriptions for callbacks
    base::CallbackListSubscription watermark_subscription_;
#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
    base::CallbackListSubscription screenshot_subscription_;
#endif
  };

  void ApplyWatermarkSettings(const std::string& watermark_text,
                              SkColor fill_color,
                              SkColor outline_color,
                              int font_size);

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
  void MaybeUpdateScreenshotSettings();
#endif

 private:
  raw_ptr<BrowserView> browser_view_;
  std::vector<std::unique_ptr<ContentsViewDataProtectionController>>
      contents_view_data_protection_controllers_;
  ui::ScopedUnownedUserData<DataProtectionUIController> scoped_data_holder_;
};

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_UI_CONTROLLER_H_
