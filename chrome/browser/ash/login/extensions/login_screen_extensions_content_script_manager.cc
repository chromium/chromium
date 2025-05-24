// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/extensions/login_screen_extensions_content_script_manager.h"

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "url/gurl.h"

namespace ash {

namespace {

// URL which is allowlisted to be matched by login screen extension's content
// scripts.
constexpr char kImprivataContentScriptURL[] =
    "https://idp.cloud.imprivata.com/";

}  // namespace

LoginScreenExtensionsContentScriptManager::
    LoginScreenExtensionsContentScriptManager(Profile* signin_original_profile)
    : signin_original_profile_(signin_original_profile),
      extension_system_(
          extensions::ExtensionSystem::Get(signin_original_profile_)) {
  DCHECK(signin_original_profile_);
  DCHECK(extension_system_);

  auto* const extension_registry =
      extensions::ExtensionRegistry::Get(signin_original_profile_);
  DCHECK(extension_registry);
  extension_registry_observation_.Observe(extension_registry);
}

LoginScreenExtensionsContentScriptManager::
    ~LoginScreenExtensionsContentScriptManager() = default;

void LoginScreenExtensionsContentScriptManager::Shutdown() {
  extension_registry_observation_.Reset();
  weak_factory_.InvalidateWeakPtrs();
}

void LoginScreenExtensionsContentScriptManager::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (!extension->is_login_screen_extension())
    return;

  for (const std::unique_ptr<extensions::UserScript>& script :
       extensions::ContentScriptsInfo::GetContentScripts(extension)) {
    if (!script->MatchesURL(GURL(kImprivataContentScriptURL))) {
      LOG(WARNING) << "Disabling extension: " << extension->id() << " / "
                   << extension->name()
                   << " because it is using disallowed content scripts.";
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &LoginScreenExtensionsContentScriptManager::DisableExtension,
              weak_factory_.GetWeakPtr(), browser_context, extension->id()));
      return;
    }
  }
}

void LoginScreenExtensionsContentScriptManager::DisableExtension(
    content::BrowserContext* browser_context,
    const extensions::ExtensionId& extension_id) {
  auto* extension_registrar =
      extensions::ExtensionRegistrar::Get(browser_context);
  extension_registrar->DisableExtension(
      extension_id, {extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY});
}

}  // namespace ash
