// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "chrome/browser/extensions/api/chrome_device_permissions_prompt.h"
#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"
#include "chrome/browser/extensions/api/messaging/chrome_messaging_delegate.h"
#include "chrome/browser/extensions/api/messaging/chrome_native_message_port_dispatcher.h"
#include "chrome/browser/extensions/extension_action_dispatcher.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/system_display/display_info_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_delegate_impl.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/buildflags/buildflags.h"

// TODO(crbug.com/417770773): This file contains the parts of
// ChromeExtensionsAPIClient that are not yet supported on desktop Android. Once
// this file becomes minimal in size it should be folded into
// chrome_extensions_api_client.cc.

static_assert(BUILDFLAG(ENABLE_EXTENSIONS));

namespace extensions {

void ChromeExtensionsAPIClient::NotifyWebRequestWithheld(
    int render_process_id,
    int render_frame_id,
    const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Track down the ExtensionActionRunner and the extension. Since this is
  // asynchronous, we could hit a null anywhere along the path.
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host) {
    return;
  }
  // We don't count subframes and prerendering blocked actions as yet, since
  // there's no way to surface this to the user. Ignore these (which is also
  // what we do for content scripts).
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents) {
    return;
  }
  extensions::ExtensionActionRunner* runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  if (!runner) {
    return;
  }

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(web_contents->GetBrowserContext())
          ->enabled_extensions()
          .GetByID(extension_id);
  if (!extension) {
    return;
  }

  // If the extension doesn't request access to the tab, return. The user
  // invoking the extension on a site grants access to the tab's origin if
  // and only if the extension requested it; without requesting the tab,
  // clicking on the extension won't grant access to the resource.
  // https://crbug.com/891586.
  // TODO(crbug.com/40076508): We can remove this if extensions require host
  // permissions to the initiator, since then we'll never get into this type
  // of circumstance (the request would be blocked, rather than withheld).
  if (!extension->permissions_data()
           ->withheld_permissions()
           .explicit_hosts()
           .MatchesURL(render_frame_host->GetLastCommittedURL())) {
    return;
  }

  runner->OnWebRequestBlocked(extension);
}

void ChromeExtensionsAPIClient::UpdateActionCount(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    int tab_id,
    int action_count,
    bool clear_badge_text) {
  const Extension* extension =
      ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
          extension_id);
  DCHECK(extension);

  ExtensionAction* action =
      ExtensionActionManager::Get(context)->GetExtensionAction(*extension);
  DCHECK(action);

  action->SetDNRActionCount(tab_id, action_count);

  // The badge text should be cleared if |action| contains explicitly set badge
  // text for the |tab_id| when the preference is then toggled on. In this case,
  // the matched action count should take precedence over the badge text.
  if (clear_badge_text) {
    action->ClearBadgeText(tab_id);
  }

  content::WebContents* tab_contents = nullptr;
  if (ExtensionTabUtil::GetTabById(tab_id, context, /*include_incognito=*/true,
                                   &tab_contents) &&
      tab_contents) {
    ExtensionActionDispatcher::Get(context)->NotifyChange(action, tab_contents,
                                                          context);
  }
}

void ChromeExtensionsAPIClient::ClearActionCount(
    content::BrowserContext* context,
    const Extension& extension) {
  ExtensionAction* action =
      ExtensionActionManager::Get(context)->GetExtensionAction(extension);
  DCHECK(action);

  action->ClearDNRActionCountForAllTabs();

  std::vector<content::WebContents*> contents_to_notify =
      ExtensionTabUtil::GetAllActiveWebContentsForContext(
          context, /*include_incognito=*/true);

  for (auto* active_contents : contents_to_notify) {
    ExtensionActionDispatcher::Get(context)->NotifyChange(
        action, active_contents, context);
  }
}

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
