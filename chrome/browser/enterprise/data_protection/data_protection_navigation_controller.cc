// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"

#include <algorithm>

#include "base/feature_list.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/referrer_cache_utils.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_observer.h"
#include "chrome/browser/enterprise/watermark/settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/watermarking/content/watermark_text_container.h"
#include "components/enterprise/watermarking/watermark.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace enterprise_data_protection {

DataProtectionNavigationController::DataProtectionNavigationController(
    tabs::TabInterface* tab_interface)
    : tab_interface_(tab_interface) {
  Observe(tab_interface->GetContents());
  tab_subscriptions_.push_back(
      tab_interface_->RegisterWillDiscardContents(base::BindRepeating(
          &DataProtectionNavigationController::WillDiscardContents,
          weak_ptr_factory_.GetWeakPtr())));

  // Fetch the protection settings for the current page.
  enterprise_data_protection::DataProtectionNavigationObserver::
      ApplyDataProtectionSettings(
          Profile::FromBrowserContext(
              tab_interface_->GetContents()->GetBrowserContext()),
          tab_interface_->GetContents(),
          base::BindOnce(
              &DataProtectionNavigationController::ApplyDataProtectionSettings,
              weak_ptr_factory_.GetWeakPtr(),
              tab_interface_->GetContents()->GetWeakPtr()));

  // If there happens to be a navigation in process then that will be missed,
  // since DidStartNavigation does not trigger.
}

DataProtectionNavigationController::~DataProtectionNavigationController() =
    default;

base::CallbackListSubscription
DataProtectionNavigationController::RegisterWatermarkStringUpdatedCallback(
    WatermarkStringUpdatedCallback callback) {
  return watermark_string_updated_callbacks_.Add(std::move(callback));
}

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
base::CallbackListSubscription
DataProtectionNavigationController::RegisterScreenshotAllowedUpdatedCallback(
    ScreenshotAllowedUpdatedCallback callback) {
  return screenshot_allowed_updated_callbacks_.Add(std::move(callback));
}
#endif  // BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)

void DataProtectionNavigationController::ApplyDataProtectionSettings(
    base::WeakPtr<content::WebContents> expected_web_contents,
    const enterprise_data_protection::UrlSettings& settings) {
  // If the tab was discarded, do nothing.
  if (!expected_web_contents || web_contents() != expected_web_contents.get()) {
    return;
  }

  watermark_text_ = settings.watermark_text;
  watermark_string_updated_callbacks_.Notify(watermark_text_);

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
  screenshot_allowed_ = settings.allow_screenshots;
  screenshot_allowed_updated_callbacks_.Notify(screenshot_allowed_);
#endif  // BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
}

void DataProtectionNavigationController::
    DocumentOnLoadCompletedInPrimaryMainFrame() {
  // It is possible for `clear_watermark_text_on_page_load_` to be set to false
  // even when the watermark should be cleared.  However, in this case there
  // is a queued call to `ApplyDataProtectionSettings()` which will correctly
  // reset the watermark.  The scenario is as followed:
  //
  // 1/ User is viewing a page in Tab A that is watermarked.
  // 2/ User loads a page that should not be watermarked into Tab A.
  // 3/ `DelayApplyDataProtectionSettingsIfEmpty()` is called at navigation
  //     finish time which sets clear_watermark_text_on_page_load_=true.
  //    `DocumentOnLoadCompletedInPrimaryMainFrame()` will be called later.
  // 4/ User switches to Tab B, which may or may not be watermarked.
  //    This calls `ApplyDataProtectionSettings()` setting the watermark
  //    appropriate to Tab B and sets clear_watermark_text_on_page_load_=false.
  // 5/ User switches back to Tab A (which shows a page that should not be
  //    watermarked, as described in step 2 above). This also calls
  //    `ApplyDataProtectionSettings()` setting the watermark
  //    appropriate to Tab A (i.e. clears the watermark) and sets
  //    clear_watermark_text_on_page_load_=false.
  // 6/ `DocumentOnLoadCompletedInPrimaryMainFrame()` is eventually called
  //    which does nothing because clear_watermark_text_on_page_load_==false.
  //    However, the watermark is already cleared in step #5.
  //
  // Note that steps #5 and #6 are racy but the final outcome is correct
  // regardless of the order in which they execute.

  if (clear_watermark_text_on_page_load_) {
    watermark_text_ = std::string();
    clear_watermark_text_on_page_load_ = false;
    watermark_string_updated_callbacks_.Notify(watermark_text_);
  }

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
  if (clear_screenshot_protection_on_page_load_) {
    screenshot_allowed_ = true;
    screenshot_allowed_updated_callbacks_.Notify(screenshot_allowed_);

    clear_screenshot_protection_on_page_load_ = false;
  }
#endif
}

void DataProtectionNavigationController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // The referrer chain should stay the same in the cache for as long as the
  // main document doesn't change.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  auto* service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext());
  if (!service) {
    return;
  }

  // Only cache referrer chain data if a policy that uses it has been enabled.
  if (service->IsConnectorEnabled(
          enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY) ||
      service->IsConnectorEnabled(
          enterprise_connectors::AnalysisConnector::FILE_ATTACHED) ||
      service->IsConnectorEnabled(
          enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED) ||
#if BUILDFLAG(IS_CHROMEOS)
      service->IsConnectorEnabled(
          enterprise_connectors::AnalysisConnector::FILE_TRANSFER) ||
#endif
      service->IsConnectorEnabled(
          enterprise_connectors::AnalysisConnector::PRINT) ||
      service->GetReportingSettings().has_value() ||
      service->GetAppliedRealTimeUrlCheck() ==
          enterprise_connectors::EnterpriseRealTimeUrlCheckMode::
              REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED) {
    enterprise_connectors::SetReferrerChain(
        navigation_handle->GetURL(), *navigation_handle->GetWebContents());
  }
}

void DataProtectionNavigationController::
    ApplyDataProtectionSettingsOrDelayIfEmpty(
        base::WeakPtr<content::WebContents> expected_web_contents,
        bool is_same_document,
        const enterprise_data_protection::UrlSettings& settings) {
  // If discarded, do nothing.
  if (!expected_web_contents || expected_web_contents.get() != web_contents()) {
    return;
  }

#if BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)
  if (!settings.allow_screenshots) {
    screenshot_allowed_ = settings.allow_screenshots;
    screenshot_allowed_updated_callbacks_.Notify(screenshot_allowed_);

  } else {
    // Screenshot protection should be cleared.  Delay that until the page
    // finishes loading.
    clear_screenshot_protection_on_page_load_ = true;
  }
#endif  // BUILDFLAG(ENTERPRISE_SCREENSHOT_PROTECTION)

  // Regardless of whether watermark text is empty, attach it as web contents
  // user data so that other browser process code can draw watermarks outside
  // of the context of a navigation (ex. when printing).
  Profile* profile =
      Profile::FromBrowserContext(expected_web_contents->GetBrowserContext());
  enterprise_watermark::WatermarkBlock block =
      enterprise_watermark::DrawWatermarkToPaintRecord(
          settings.watermark_text,
          enterprise_watermark::GetFillColor(profile->GetPrefs()),
          enterprise_watermark::GetOutlineColor(profile->GetPrefs()),
          enterprise_watermark::GetFontSize(profile->GetPrefs()));
  enterprise_watermark::WatermarkTextContainer::CreateForWebContents(
      expected_web_contents.get());
  enterprise_watermark::WatermarkTextContainer::FromWebContents(
      expected_web_contents.get())
      ->SetWatermarkText(
          block.record.ToSkPicture(SkRect::MakeWH(block.width, block.height)),
          block.width, block.height);

  // For same document navigations, watermark clearing has to happen here,
  // because there is no document onload event that is invoked.
  clear_watermark_text_on_page_load_ =
      settings.watermark_text.empty() && !is_same_document;

  if (!clear_watermark_text_on_page_load_) {
    watermark_text_ = settings.watermark_text;
    watermark_string_updated_callbacks_.Notify(watermark_text_);
  }

  if (!on_delay_apply_data_protection_settings_if_empty_called_for_testing_
           .is_null()) {
    std::move(
        on_delay_apply_data_protection_settings_if_empty_called_for_testing_)
        .Run();
  }
}

void DataProtectionNavigationController::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  auto navigation_observer = enterprise_data_protection::
      DataProtectionNavigationObserver::CreateForNavigationIfNeeded(
          this, tab_interface_->GetBrowserWindowInterface()->GetProfile(),
          navigation_handle,
          base::BindOnce(&DataProtectionNavigationController::
                             ApplyDataProtectionSettingsOrDelayIfEmpty,
                         weak_ptr_factory_.GetWeakPtr(),
                         web_contents()->GetWeakPtr(),
                         navigation_handle->IsSameDocument()));

  if (navigation_observer) {
    navigation_observers_.emplace(navigation_handle->GetNavigationId(),
                                  std::move(navigation_observer));
  }
}

void DataProtectionNavigationController::Cleanup(int64_t navigation_id) {
  // Not all navigation IDs passed to this cleanup will have been added to the
  // map, DataProtectionNavigationObserver tracks all navigations that happen
  // during its lifetime.
  navigation_observers_.erase(navigation_id);
}

void DataProtectionNavigationController::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  WebContentsObserver::Observe(new_contents);
}

void DataProtectionNavigationController::SetCallbackForTesting(
    base::OnceClosure closure) {
  on_delay_apply_data_protection_settings_if_empty_called_for_testing_ =
      std::move(closure);
}

}  // namespace enterprise_data_protection
