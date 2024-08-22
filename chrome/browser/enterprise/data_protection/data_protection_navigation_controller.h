// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_NAVIGATION_CONTROLLER_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_NAVIGATION_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "content/public/browser/web_contents_observer.h"

namespace tabs {
class TabInterface;
}

namespace enterprise_data_protection {
struct UrlSettings;

// Observes navigations in order to correctly set that tab's Data Protection
// settings based on the SafeBrowsing verdict for said navigation.
// This class is unconditionally created, but will do nothing if data protection
// is disabled.
class DataProtectionNavigationController : public content::WebContentsObserver {
 public:
  explicit DataProtectionNavigationController(
      tabs::TabInterface* tab_interface);
  ~DataProtectionNavigationController() override;

  // Callback is invoked by ApplyDataProtectionSettingsOrDelayIfEmpty.
  void SetCallbackForTesting(base::OnceClosure closure);

 private:
  // TabInterface subscriber. Called when the associated tab enters the
  // foreground.
  void TabForegrounded(tabs::TabInterface* tab);

  // TabInterface subscriber. Called when the associated tab is going to be
  // discarded.
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);
  // Applies data protection settings if there are any to apply, otherwise
  // delay clearing the data protection settings until the page loads.
  //
  // This is called from a finish navigation event to handle the case where the
  // browser view is switching from a tab with data protections enabled to one
  // without.  At the end of the navigation, the existing page is still visible
  // to the user since the UI has not yet refreshed.  In this case the
  // protections should remain in place.  Once the document finishes loading,
  // `ApplyDataProtectionSettings()` will be called.  See
  // `DocumentOnLoadCompletedInPrimaryMainFrame()`.
  void ApplyDataProtectionSettingsOrDelayIfEmpty(
      base::WeakPtr<content::WebContents> expected_web_contents,
      const enterprise_data_protection::UrlSettings& settings);

  // Applies data protection settings based on the verdict received by
  // safe-browsing's realtime to `watermark_view_`.
  void ApplyDataProtectionSettings(
      base::WeakPtr<content::WebContents> expected_web_contents,
      const enterprise_data_protection::UrlSettings& settings);

  // content::WebContentsObserver
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

  // Clear data protections once the page loads.
  // TODO(b/330960313): These bools can be removed once FCP is used as the
  // signal to set the data protections for the current tab.
  bool clear_watermark_text_on_page_load_ = false;
#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
  bool clear_screenshot_protection_on_page_load_ = false;
#endif

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  raw_ptr<tabs::TabInterface> tab_interface_;

  base::OnceClosure
      on_delay_apply_data_protection_settings_if_empty_called_for_testing_;

  mutable base::WeakPtrFactory<DataProtectionNavigationController>
      weak_ptr_factory_{this};
};

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_NAVIGATION_CONTROLLER_H_
