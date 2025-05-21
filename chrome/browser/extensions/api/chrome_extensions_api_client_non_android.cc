// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "chrome/browser/extensions/api/chrome_device_permissions_prompt.h"
#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"
#include "chrome/browser/extensions/api/messaging/chrome_messaging_delegate.h"
#include "chrome/browser/extensions/api/messaging/chrome_native_message_port_dispatcher.h"
#include "chrome/browser/extensions/system_display/display_info_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_delegate_impl.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/buildflags/buildflags.h"

// TODO(crbug.com/417770773): This file contains the parts of
// ChromeExtensionsAPIClient that are not yet supported on desktop Android. Once
// this file becomes minimal in size it should be folded into
// chrome_extensions_api_client.cc.

static_assert(BUILDFLAG(ENABLE_EXTENSIONS));

namespace extensions {

void ChromeExtensionsAPIClient::OpenFileUrl(
    const GURL& file_url,
    content::BrowserContext* browser_context) {
  CHECK(file_url.is_valid());
  CHECK(file_url.SchemeIsFile());
  Profile* profile = Profile::FromBrowserContext(browser_context);
  NavigateParams navigate_params(profile, file_url,
                                 ui::PAGE_TRANSITION_FROM_API);
  navigate_params.disposition = WindowOpenDisposition::CURRENT_TAB;
  navigate_params.browser =
      chrome::FindTabbedBrowser(profile, /*match_original_profiles=*/false);
  Navigate(&navigate_params);
}

std::unique_ptr<DevicePermissionsPrompt>
ChromeExtensionsAPIClient::CreateDevicePermissionsPrompt(
    content::WebContents* web_contents) const {
  return std::make_unique<ChromeDevicePermissionsPrompt>(web_contents);
}

std::unique_ptr<SupervisedUserExtensionsDelegate>
ChromeExtensionsAPIClient::CreateSupervisedUserExtensionsDelegate(
    content::BrowserContext* browser_context) const {
  return std::make_unique<SupervisedUserExtensionsDelegateImpl>(
      browser_context);
}

std::unique_ptr<DisplayInfoProvider>
ChromeExtensionsAPIClient::CreateDisplayInfoProvider() const {
  return CreateChromeDisplayInfoProvider();
}

MessagingDelegate* ChromeExtensionsAPIClient::GetMessagingDelegate() {
  if (!messaging_delegate_) {
    messaging_delegate_ = std::make_unique<ChromeMessagingDelegate>();
  }
  return messaging_delegate_.get();
}

std::vector<KeyedServiceBaseFactory*>
ChromeExtensionsAPIClient::GetFactoryDependencies() {
  // clang-format off
  return {
      InstantServiceFactory::GetInstance(),
      SupervisedUserServiceFactory::GetInstance(),
  };
  // clang-format on
}

std::unique_ptr<NativeMessagePortDispatcher>
ChromeExtensionsAPIClient::CreateNativeMessagePortDispatcher(
    std::unique_ptr<NativeMessageHost> host,
    base::WeakPtr<NativeMessagePort> port,
    scoped_refptr<base::SingleThreadTaskRunner> message_service_task_runner) {
  return std::make_unique<ChromeNativeMessagePortDispatcher>(
      std::move(host), std::move(port), std::move(message_service_task_runner));
}

}  // namespace extensions
