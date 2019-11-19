// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/navigation_observer.h"

#include "base/bind.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"

using content::NavigationController;
using content::NavigationEntry;

namespace extensions {

namespace {
// Whether to repeatedly prompt for the same extension id.
bool g_repeat_prompting = false;
}

NavigationObserver::NavigationObserver(Profile* profile) : profile_(profile) {
  RegisterForNotifications();
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile));
}

NavigationObserver::~NavigationObserver() {}

void NavigationObserver::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);

  NavigationController* controller =
      content::Source<NavigationController>(source).ptr();
  if (!profile_->IsSameProfile(
          Profile::FromBrowserContext(controller->GetBrowserContext())))
    return;

  PromptToEnableExtensionIfNecessary(controller);
}

// static
void NavigationObserver::SetAllowedRepeatedPromptingForTesting(bool allowed) {
  g_repeat_prompting = allowed;
}

void NavigationObserver::RegisterForNotifications() {
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 content::NotificationService::AllSources());
}

void NavigationObserver::PromptToEnableExtensionIfNecessary(
    NavigationController* nav_controller) {
  // Bail out if we're already running a prompt.
  if (!in_progress_prompt_extension_id_.empty())
    return;

  NavigationEntry* nav_entry = nav_controller->GetVisibleEntry();
  if (!nav_entry)
    return;

  const GURL& url = nav_entry->GetURL();

  // NOTE: We only consider chrome-extension:// urls, and deliberately don't
  // consider hosted app urls. This is because it's really annoying to visit the
  // site associated with a hosted app (like calendar.google.com or
  // drive.google.com) and have it repeatedly prompt you to re-enable an item.
  // Visiting a chrome-extension:// url is a much stronger signal, and, without
  // the item enabled, we won't show anything.
  // TODO(devlin): While true, I still wonder how useful this is. We should get
  // metrics.
  if (!url.SchemeIs(kExtensionScheme))
    return;

  const Extension* extension = ExtensionRegistry::Get(profile_)
                                   ->disabled_extensions()
                                   .GetExtensionOrAppByURL(url);
  if (!extension)
    return;

  // Try not to repeatedly prompt the user about the same extension.
  if (!prompted_extensions_.insert(extension->id()).second &&
      !g_repeat_prompting) {
    return;
  }

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile_);
  // TODO(devlin): Why do we only consider extensions that escalate permissions?
  // Maybe because it's the only one we have a good prompt for?
  if (extension_prefs->DidExtensionEscalatePermissions(extension->id())) {
    // Keep track of the extension id and nav controller we're prompting for.
    // These must be reset in OnInstallPromptDone.
    in_progress_prompt_extension_id_ = extension->id();
    in_progress_prompt_navigation_controller_ = nav_controller;

    extension_install_prompt_.reset(
        new ExtensionInstallPrompt(nav_controller->GetWebContents()));
    ExtensionInstallPrompt::PromptType type =
        ExtensionInstallPrompt::GetReEnablePromptTypeForExtension(profile_,
                                                                  extension);
    extension_install_prompt_->ShowDialog(
        base::Bind(&NavigationObserver::OnInstallPromptDone,
                   weak_factory_.GetWeakPtr()),
        extension, nullptr,
        std::make_unique<ExtensionInstallPrompt::Prompt>(type),
        ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  }
}

void NavigationObserver::OnInstallPromptDone(
    ExtensionInstallPrompt::Result result) {
  // The extension was already uninstalled.
  if (in_progress_prompt_extension_id_.empty())
    return;

  ExtensionRegistry* extension_registry = ExtensionRegistry::Get(profile_);
  const Extension* extension = extension_registry->GetExtensionById(
      in_progress_prompt_extension_id_, ExtensionRegistry::EVERYTHING);
  CHECK(extension);

  if (result == ExtensionInstallPrompt::Result::ACCEPTED) {
    NavigationController* nav_controller =
        in_progress_prompt_navigation_controller_;
    CHECK(nav_controller);

    ExtensionService* extension_service =
        ExtensionSystem::Get(profile_)->extension_service();
    // Grant permissions, re-enable the extension, and then reload the tab.
    extension_service->GrantPermissionsAndEnableExtension(extension);
    nav_controller->Reload(content::ReloadType::NORMAL, true);
  } else {
    // TODO(devlin): These metrics aren't very useful, since they're lumped in
    // with the same for re-enabling/canceling when the extension first gets
    // disabled, which is likely significantly more common (though impossible to
    // tell). We need to separate these.
    std::string histogram_name =
       result == ExtensionInstallPrompt::Result::USER_CANCELED
            ? "ReEnableCancel"
            : "ReEnableAbort";
    ExtensionService::RecordPermissionMessagesHistogram(extension,
                                                        histogram_name.c_str());
  }

  in_progress_prompt_extension_id_.clear();
  in_progress_prompt_navigation_controller_ = nullptr;
  extension_install_prompt_.reset();
}

void NavigationObserver::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  if (in_progress_prompt_extension_id_.empty() ||
      in_progress_prompt_extension_id_ != extension->id()) {
    return;
  }

  in_progress_prompt_extension_id_ = std::string();
  in_progress_prompt_navigation_controller_ = nullptr;
  extension_install_prompt_.reset();
}

}  // namespace extensions
