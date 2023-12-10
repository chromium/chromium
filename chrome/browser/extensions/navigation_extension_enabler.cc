// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/navigation_extension_enabler.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/extensions/extension_service.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

NavigationExtensionEnabler::NavigationExtensionEnabler(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<NavigationExtensionEnabler>(*web_contents) {
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(web_contents->GetBrowserContext()));
}

NavigationExtensionEnabler::~NavigationExtensionEnabler() = default;

void NavigationExtensionEnabler::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  PromptToEnableExtensionIfNecessary(load_details.entry->GetURL());
}

void NavigationExtensionEnabler::PromptToEnableExtensionIfNecessary(
    const GURL& url) {
  // Bail out if we're already running a prompt.
  if (!in_progress_prompt_extension_id_.empty()) {
    return;
  }

  // NOTE: We only consider chrome-extension:// urls, and deliberately don't
  // consider hosted app urls. This is because it's really annoying to visit the
  // site associated with a hosted app (like calendar.google.com or
  // drive.google.com) and have it repeatedly prompt you to re-enable an item.
  // Visiting a chrome-extension:// url is a much stronger signal, and, without
  // the item enabled, we won't show anything.
  // TODO(devlin): While true, I still wonder how useful this is. We should get
  // metrics.
  if (!url.SchemeIs(kExtensionScheme)) {
    return;
  }

  const Extension* extension =
      ExtensionRegistry::Get(web_contents()->GetBrowserContext())
          ->disabled_extensions()
          .GetExtensionOrAppByURL(url);
  if (!extension) {
    return;
  }

  ExtensionPrefs* extension_prefs =
      ExtensionPrefs::Get(web_contents()->GetBrowserContext());
  // TODO(devlin): Why do we only consider extensions that escalate permissions?
  // Maybe because it's the only one we have a good prompt for?
  if (!extension_prefs->DidExtensionEscalatePermissions(extension->id())) {
    return;
  }

  // Keep track of the extension id we're prompting for. These must be reset in
  // OnInstallPromptDone.
  in_progress_prompt_extension_id_ = extension->id();

  extension_install_prompt_ =
      std::make_unique<ExtensionInstallPrompt>(web_contents());
  ExtensionInstallPrompt::PromptType type =
      ExtensionInstallPrompt::GetReEnablePromptTypeForExtension(
          web_contents()->GetBrowserContext(), extension);
  extension_install_prompt_->ShowDialog(
      base::BindRepeating(&NavigationExtensionEnabler::OnInstallPromptDone,
                          weak_factory_.GetWeakPtr()),
      extension, nullptr,
      std::make_unique<ExtensionInstallPrompt::Prompt>(type),
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());
}

void NavigationExtensionEnabler::OnInstallPromptDone(
    ExtensionInstallPrompt::DoneCallbackPayload payload) {
  // This dialog doesn't support the "withhold permissions" checkbox.
  DCHECK_NE(payload.result,
            ExtensionInstallPrompt::Result::ACCEPTED_WITH_WITHHELD_PERMISSIONS);

  // The extension was already uninstalled.
  if (in_progress_prompt_extension_id_.empty()) {
    return;
  }

  ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(web_contents()->GetBrowserContext());
  const Extension* extension = extension_registry->GetExtensionById(
      in_progress_prompt_extension_id_, ExtensionRegistry::EVERYTHING);
  CHECK(extension);

  if (payload.result == ExtensionInstallPrompt::Result::ACCEPTED) {
    ExtensionService* extension_service =
        ExtensionSystem::Get(web_contents()->GetBrowserContext())
            ->extension_service();
    // Grant permissions, re-enable the extension, and then reload the tab.
    extension_service->GrantPermissionsAndEnableExtension(extension);
    web_contents()->GetController().Reload(content::ReloadType::NORMAL, true);
  }

  in_progress_prompt_extension_id_.clear();
  extension_install_prompt_.reset();
}

void NavigationExtensionEnabler::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  if (in_progress_prompt_extension_id_.empty() ||
      in_progress_prompt_extension_id_ != extension->id()) {
    return;
  }

  in_progress_prompt_extension_id_ = std::string();
  extension_install_prompt_.reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(NavigationExtensionEnabler);

}  // namespace extensions
