// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/public_session_permission_helper.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/extensions/device_local_account_management_policy_provider.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/manifest_permission_set.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/url_pattern_set.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {
namespace permission_helper {

namespace {

std::unique_ptr<ExtensionInstallPrompt> CreateExtensionInstallPrompt(
    content::WebContents* web_contents) {
  return std::make_unique<ExtensionInstallPrompt>(web_contents);
}

bool PermissionCheckNeeded(const Extension* extension) {
  return !chromeos::DeviceLocalAccountManagementPolicyProvider::IsWhitelisted(
      extension->id());
}

// This class is the internal implementation of HandlePermissionRequest(). It
// contains the actual prompt showing and resolving logic, and it caches the
// user choices.
class PublicSessionPermissionHelper {
 public:
  PublicSessionPermissionHelper();
  PublicSessionPermissionHelper(PublicSessionPermissionHelper&& other);
  ~PublicSessionPermissionHelper();

  bool HandlePermissionRequestImpl(const Extension& extension,
                                   const PermissionIDSet& requested_permissions,
                                   content::WebContents* web_contents,
                                   const RequestResolvedCallback& callback,
                                   const PromptFactory& prompt_factory);

  bool PermissionAllowedImpl(const Extension* extension,
                             APIPermission::ID permission);

 private:
  void ResolvePermissionPrompt(const ExtensionInstallPrompt* prompt,
                               const PermissionIDSet& unprompted_permissions,
                               ExtensionInstallPrompt::Result prompt_result);

  PermissionIDSet FilterAllowedPermissions(const PermissionIDSet& permissions);

  struct RequestCallback {
    RequestCallback(const RequestResolvedCallback& callback,
                    const PermissionIDSet& permission_list);
    RequestCallback(const RequestCallback& other);
    ~RequestCallback();
    RequestResolvedCallback callback;
    PermissionIDSet permission_list;
  };
  using RequestCallbackList = std::vector<RequestCallback>;

  std::set<std::unique_ptr<ExtensionInstallPrompt>, base::UniquePtrComparator>
      prompts_;
  PermissionIDSet prompted_permission_set_;
  PermissionIDSet allowed_permission_set_;
  PermissionIDSet denied_permission_set_;
  RequestCallbackList callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PublicSessionPermissionHelper);
};

PublicSessionPermissionHelper::PublicSessionPermissionHelper() {}

PublicSessionPermissionHelper::PublicSessionPermissionHelper(
    PublicSessionPermissionHelper&& other) = default;

PublicSessionPermissionHelper::~PublicSessionPermissionHelper() {}

bool PublicSessionPermissionHelper::HandlePermissionRequestImpl(
    const Extension& extension,
    const PermissionIDSet& requested_permissions,
    content::WebContents* web_contents,
    const RequestResolvedCallback& callback,
    const PromptFactory& prompt_factory) {
  DCHECK(profiles::ArePublicSessionRestrictionsEnabled());
  if (!PermissionCheckNeeded(&extension)) {
    if (!callback.is_null())
      callback.Run(requested_permissions);
    return true;
  }

  PermissionIDSet unresolved_permissions = PermissionIDSet::Difference(
      requested_permissions, allowed_permission_set_);
  unresolved_permissions = PermissionIDSet::Difference(
      unresolved_permissions, denied_permission_set_);
  if (unresolved_permissions.empty()) {
    // All requested permissions are already resolved.
    if (!callback.is_null())
      callback.Run(FilterAllowedPermissions(requested_permissions));
    return true;
  }

  // Since not all permissions are resolved yet, queue the callback to be called
  // when all of them are resolved.
  if (!callback.is_null())
    callbacks_.push_back(RequestCallback(callback, requested_permissions));

  PermissionIDSet unprompted_permissions = PermissionIDSet::Difference(
      unresolved_permissions, prompted_permission_set_);
  if (unprompted_permissions.empty()) {
    // Some permissions aren't resolved yet, but they are currently being
    // prompted for, so no need to show a prompt.
    return false;
  }

  // Some permissions need prompting, setup the prompt and show it.
  APIPermissionSet new_apis;
  for (const auto& permission : unprompted_permissions) {
    prompted_permission_set_.insert(permission.id());
    new_apis.insert(permission.id());
  }
  auto permission_set = std::make_unique<PermissionSet>(
      std::move(new_apis), ManifestPermissionSet(), URLPatternSet(),
      URLPatternSet());
  auto prompt = prompt_factory.Run(web_contents);

  auto permissions_prompt = std::make_unique<ExtensionInstallPrompt::Prompt>(
      ExtensionInstallPrompt::PERMISSIONS_PROMPT);
  // activeTab has no permission message by default, so one is added here.
  if (unprompted_permissions.ContainsID(APIPermission::kActiveTab)) {
    PermissionMessages messages;
    messages.push_back(PermissionMessage(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_CURRENT_HOST),
        extensions::PermissionIDSet()));
    permissions_prompt->AddPermissions(messages);
  }

  // This Unretained is safe because the lifetime of this object is until
  // process exit.
  prompt->ShowDialog(
      base::Bind(&PublicSessionPermissionHelper::ResolvePermissionPrompt,
                 base::Unretained(this), prompt.get(),
                 std::move(unprompted_permissions)),
      &extension,
      nullptr,  // Use the extension icon.
      std::move(permissions_prompt),
      std::move(permission_set),
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  prompts_.insert(std::move(prompt));

  return false;
}

bool PublicSessionPermissionHelper::PermissionAllowedImpl(
    const Extension* extension,
    APIPermission::ID permission) {
  DCHECK(profiles::ArePublicSessionRestrictionsEnabled());
  return !PermissionCheckNeeded(extension) ||
         allowed_permission_set_.ContainsID(permission);
}

void PublicSessionPermissionHelper::ResolvePermissionPrompt(
    const ExtensionInstallPrompt* prompt,
    const PermissionIDSet& unprompted_permissions,
    ExtensionInstallPrompt::Result prompt_result) {
  PermissionIDSet& add_to_set =
      prompt_result == ExtensionInstallPrompt::Result::ACCEPTED ?
          allowed_permission_set_ : denied_permission_set_;
  for (const auto& permission : unprompted_permissions) {
    prompted_permission_set_.erase(permission.id());
    add_to_set.insert(permission.id());
  }

  // Here a list of callbacks to be invoked is created first from callbacks_,
  // then those callbacks are invoked later because a callback can change
  // callbacks_ while we're traversing them.
  RequestCallbackList callbacks_to_invoke;
  for (auto callback = callbacks_.begin(); callback != callbacks_.end(); ) {
    if (prompted_permission_set_.ContainsAnyID(callback->permission_list)) {
      // The request is still waiting on other permissions to be resolved - wait
      // until all of them are resolved before calling the callback.
      callback++;
      continue;
    }
    callbacks_to_invoke.push_back(std::move(*callback));
    callback = callbacks_.erase(callback);
  }
  for (auto callback = callbacks_to_invoke.begin();
       callback != callbacks_to_invoke.end(); callback++) {
    callback->callback.Run(FilterAllowedPermissions(callback->permission_list));
  }

  // Dispose of the prompt as it's not needed anymore.
  auto iter = prompts_.find(prompt);
  DCHECK(iter != prompts_.end());
  prompts_.erase(iter);
}

PermissionIDSet PublicSessionPermissionHelper::FilterAllowedPermissions(
    const PermissionIDSet& permissions) {
  PermissionIDSet allowed_permissions;
  for (auto iter = permissions.begin(); iter != permissions.end(); iter++) {
    if (allowed_permission_set_.ContainsID(*iter)) {
      allowed_permissions.insert(iter->id());
    }
  }
  return allowed_permissions;
}

PublicSessionPermissionHelper::RequestCallback::RequestCallback(
    const RequestResolvedCallback& callback,
    const PermissionIDSet& permission_list)
    : callback(callback), permission_list(permission_list) {}

PublicSessionPermissionHelper::RequestCallback::RequestCallback(
    const RequestCallback& other) = default;

PublicSessionPermissionHelper::RequestCallback::~RequestCallback() {}

base::LazyInstance<std::map<ExtensionId, PublicSessionPermissionHelper>>::Leaky
    g_helpers = LAZY_INSTANCE_INITIALIZER;

}  // namespace

bool HandlePermissionRequest(const Extension& extension,
                             const PermissionIDSet& requested_permissions,
                             content::WebContents* web_contents,
                             const RequestResolvedCallback& callback,
                             const PromptFactory& prompt_factory) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const PromptFactory& factory = prompt_factory.is_null()
                                     ? base::Bind(&CreateExtensionInstallPrompt)
                                     : prompt_factory;
  return g_helpers.Get()[extension.id()].HandlePermissionRequestImpl(
      extension, requested_permissions, web_contents, callback, factory);
}

bool PermissionAllowed(const Extension* extension,
                       APIPermission::ID permission) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_helpers.Get()[extension->id()].PermissionAllowedImpl(extension,
                                                                permission);
}

void ResetPermissionsForTesting() {
  // Clear out the std::map between tests. Just setting g_helpers to
  // LAZY_INSTANCE_INITIALIZER again causes a memory leak (because of the
  // LazyInstance::Leaky).
  g_helpers.Get().clear();
}

}  // namespace permission_helper
}  // namespace extensions
