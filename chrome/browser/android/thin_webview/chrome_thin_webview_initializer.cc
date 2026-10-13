// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/thin_webview/chrome_thin_webview_initializer.h"

#include <memory>

#include "chrome/browser/android/web_contents_theme_client.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/chrome_content_subresource_filter_web_contents_helper_factory.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "chrome/browser/ui/autofill/autofill_client_provider.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/web_contents.h"

namespace thin_webview::android {

// static
void ChromeThinWebViewInitializer::Initialize() {
  ThinWebViewInitializer::SetInstance(new ChromeThinWebViewInitializer);
}

void ChromeThinWebViewInitializer::SetUpTheming(
    content::WebContents* web_contents) {
  night_mode::WebContentsThemeClient::CreateForWebContents(web_contents);
}

void ChromeThinWebViewInitializer::AttachTabHelpers(
    content::WebContents* web_contents,
    bool enable_permission_requests,
    bool enable_browser_autofill) {
  if (enable_browser_autofill) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    autofill::AutofillClientProvider& autofill_client_provider =
        autofill::AutofillClientProviderFactory::GetForProfile(profile);
    autofill_client_provider.CreateClientForWebContents(web_contents);
    if (!autofill_client_provider.uses_platform_autofill()) {
      ChromePasswordManagerClient::CreateForWebContents(web_contents);
    }
  }
  CreateSubresourceFilterWebContentsHelper(web_contents);
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents,
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents));
  if (!permissions::PermissionRequestManager::FromWebContents(web_contents)) {
    if (auto* pm_registry =
            performance_manager::PerformanceManagerRegistry::GetInstance()) {
      pm_registry->SetPageType(web_contents,
                               performance_manager::PageType::kTab);
    }
  }
  permissions::PermissionRequestManager::CreateForWebContents(web_contents);
  permissions::PermissionRequestManager::FromWebContents(web_contents)
      ->set_web_contents_supports_permission_requests(
          enable_permission_requests);
}

void ChromeThinWebViewInitializer::SetContextMenuPopulatorFactory(
    content::WebContents* web_contents,
    const base::android::JavaRef<jobject>& jpopulator_factory) {
  ContextMenuHelper::CreateForWebContents(web_contents);
  auto* helper = ContextMenuHelper::FromWebContents(web_contents);
  if (helper) {
    helper->SetPopulatorFactory(jpopulator_factory);
  }
}

}  // namespace thin_webview::android
