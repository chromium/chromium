// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/automation_internal/chrome_automation_internal_api_delegate.h"

#include <memory>
#include <string>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/api/automation.h"
#include "extensions/common/api/automation_internal.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/automation.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/accessibility/ax_tree_id.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/automation_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/arc/accessibility/arc_accessibility_helper_bridge.h"
#endif

namespace extensions {

ChromeAutomationInternalApiDelegate::ChromeAutomationInternalApiDelegate() =
    default;

ChromeAutomationInternalApiDelegate::~ChromeAutomationInternalApiDelegate() =
    default;

bool ChromeAutomationInternalApiDelegate::CanRequestAutomation(
    const Extension* extension,
    const AutomationInfo* automation_info,
    content::WebContents* contents) {
  if (automation_info->desktop)
    return true;

  const GURL& url = contents->GetURL();

  int tab_id = ExtensionTabUtil::GetTabId(contents);
  std::string unused_error;
  return extension->permissions_data()->CanAccessPage(url, tab_id,
                                                      &unused_error);
}

bool ChromeAutomationInternalApiDelegate::EnableTree(
    const ui::AXTreeID& tree_id) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // CrosapiManager may not be initialized on unit testing.
  // Propagate the EnableTree signal to crosapi clients.
  if (crosapi::CrosapiManager::IsInitialized()) {
    crosapi::CrosapiManager::Get()->crosapi_ash()->automation_ash()->EnableTree(
        tree_id);
  }

  arc::ArcAccessibilityHelperBridge* bridge =
      arc::ArcAccessibilityHelperBridge::GetForBrowserContext(
          GetActiveUserContext());
  if (bridge)
    return bridge->EnableTree(tree_id);
#endif
  return false;
}

void ChromeAutomationInternalApiDelegate::EnableDesktop() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // CrosapiManager may not be initialized on unit testing.
  // Propagate the EnableDesktop signal to crosapi clients.
  if (crosapi::CrosapiManager::IsInitialized()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->automation_ash()
        ->EnableDesktop();
  }
#endif

#if defined(USE_AURA)
  AutomationManagerAura::GetInstance()->Enable();
#else
  NOTIMPLEMENTED();
#endif
}

ui::AXTreeID ChromeAutomationInternalApiDelegate::GetAXTreeID() {
#if defined(USE_AURA)
  return AutomationManagerAura::GetInstance()->ax_tree_id();
#else
  NOTIMPLEMENTED();
  return ui::AXTreeIDUnknown();
#endif
}

void ChromeAutomationInternalApiDelegate::SetAutomationEventRouterInterface(
    AutomationEventRouterInterface* router) {
#if defined(USE_AURA)
  AutomationManagerAura::GetInstance()->set_automation_event_router_interface(
      router);
#else
  NOTIMPLEMENTED();
#endif
}

content::BrowserContext*
ChromeAutomationInternalApiDelegate::GetActiveUserContext() {
  // Use the main profile on ChromeOS. Desktop platforms don't have the concept
  // of a "main" profile, so pick the "last used" profile instead.
#if BUILDFLAG(IS_CHROMEOS)
  return ProfileManager::GetActiveUserProfile();
#else
  return ProfileManager::GetLastUsedProfile();
#endif
}

}  // namespace extensions
