// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_NAVIGATION_CONTROLLER_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_NAVIGATION_CONTROLLER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_observer.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "content/public/browser/web_contents_observer.h"

namespace tabs {
class TabInterface;
}

namespace enterprise_data_protection {
struct UrlSettings;

// Observes navigations in order to correctly set that tab's Data Protection
// settings based on the SafeBrowsing verdict for said navigation.
// This class is unconditionally created and sets the set for data protection.
// This is used by the view controller to either show watermark or set the
// status of screenshot for the browser.
class DataProtectionNavigationController
    : public content::WebContentsObserver,
      public DataProtectionNavigationDelegate {
 public:
  explicit DataProtectionNavigationController(
      tabs::TabInterface* tab_interface);
  ~DataProtectionNavigationController() override;
  DataProtectionNavigationController(
      const DataProtectionNavigationController&) = delete;
  DataProtectionNavigationController& operator=(
      const DataProtectionNavigationController&) = delete;

  // content::WebContentsObserver
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // DataProtectionNavigationDelegate
  void Cleanup(int64_t navigation_id) override;

  std::string watermark_text() const { return watermark_text_; }

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
  bool screenshot_allowed() const { return screenshot_allowed_; }
#endif

  // callback registration methods
  using WatermarkStringUpdatedCallback =
      base::RepeatingCallback<void(const std::string&)>;
  base::CallbackListSubscription RegisterWatermarkStringUpdatedCallback(
      WatermarkStringUpdatedCallback callback);

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
  using ScreenshotAllowedUpdatedCallback = base::RepeatingCallback<void(bool)>;
  base::CallbackListSubscription RegisterScreenshotAllowedUpdatedCallback(
      ScreenshotAllowedUpdatedCallback callback);
#endif  // BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)

  // Callback is invoked by ApplyDataProtectionSettingsOrDelayIfEmpty.
  void SetCallbackForTesting(base::OnceClosure closure);

 private:
  // Applies data protection settings if there are any to apply, otherwise
  // delay clearing the data protection settings until the page loads.
  //
  // This is called from a finish navigation event to handle the case where
  // the browser view is switching from a tab with data protections enabled to
  // one without. The observer passes `is_same_document` to this callback
  // because, since there is no document onload event for that case, the
  // original document is preserved, and the watermark is therefore cleared when
  // the navigation finishes. See `DocumentOnLoadCompletedInPrimaryMainFrame()`.
  void ApplyDataProtectionSettingsOrDelayIfEmpty(
      base::WeakPtr<content::WebContents> expected_web_contents,
      bool is_same_document,
      const enterprise_data_protection::UrlSettings& settings);

  // Applies data protection settings based on the verdict received by
  // safe-browsing's realtime to `watermark_view_`.
  void ApplyDataProtectionSettings(
      base::WeakPtr<content::WebContents> expected_web_contents,
      const enterprise_data_protection::UrlSettings& settings);

  // TabInterface subscriber. Called when the associated tab is going to be
  // discarded.
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

  raw_ptr<tabs::TabInterface> tab_interface_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  base::OnceClosure
      on_delay_apply_data_protection_settings_if_empty_called_for_testing_;

  // Clear data protections once the page loads.
  // TODO(b/330960313): These bools can be removed once FCP is used as the
  // signal to set the data protections for the current tab.
  bool clear_watermark_text_on_page_load_ = false;
  std::string watermark_text_;
#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
  bool clear_screenshot_protection_on_page_load_ = false;
  bool screenshot_allowed_ = true;
#endif

  // Maps navigation IDs to navigation observers. We take ownership of said
  // navigation observers here because, with added support for
  // same-document navigations, some verdicts arrive after the navigation
  // finishes, and we need the navigation observer to persist after this
  // happens.
  DataProtectionNavigationObserver::NavigationObservers navigation_observers_;

  base::RepeatingCallbackList<void(const std::string&)>
      watermark_string_updated_callbacks_;
#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
  base::RepeatingCallbackList<void(bool)> screenshot_allowed_updated_callbacks_;
#endif

  mutable base::WeakPtrFactory<DataProtectionNavigationController>
      weak_ptr_factory_{this};
};

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_NAVIGATION_CONTROLLER_H_
