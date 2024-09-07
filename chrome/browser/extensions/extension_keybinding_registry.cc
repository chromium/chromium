// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_keybinding_registry.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/permissions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/media_keys_listener_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/command.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/context_type.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/ash/media_client/media_client_impl.h"
#endif

namespace {

const char kOnCommandEventName[] = "commands.onCommand";

}  // namespace

namespace extensions {

ExtensionKeybindingRegistry::ExtensionKeybindingRegistry(
    content::BrowserContext* context,
    ExtensionFilter extension_filter,
    Delegate* delegate)
    : browser_context_(context),
      extension_filter_(extension_filter),
      delegate_(delegate),
      shortcut_handling_suspended_(false) {
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context_));
  command_service_observation_.Observe(CommandService::Get(browser_context_));
  media_keys_listener_ = ui::MediaKeysListener::Create(
      this, ui::MediaKeysListener::Scope::kFocused);
}

ExtensionKeybindingRegistry::~ExtensionKeybindingRegistry() {
}

void ExtensionKeybindingRegistry::SetShortcutHandlingSuspended(bool suspended) {
  shortcut_handling_suspended_ = suspended;
  OnShortcutHandlingSuspended(suspended);
}

void ExtensionKeybindingRegistry::RemoveExtensionKeybinding(
    const Extension* extension,
    const std::string& command_name) {
  bool any_media_keys_removed = false;
  auto it = event_targets_.begin();
  while (it != event_targets_.end()) {
    TargetList& target_list = it->second;
    auto target = target_list.begin();
    while (target != target_list.end()) {
      if (target->first == extension->id() &&
          (command_name.empty() || command_name == target->second))
        target = target_list.erase(target);
      else
        target++;
    }

    auto old = it++;
    if (target_list.empty()) {
      // Let each platform-specific implementation get a chance to clean up.
      RemoveExtensionKeybindingImpl(old->first, command_name);

      if (Command::IsMediaKey(old->first)) {
        any_media_keys_removed = true;
        if (media_keys_listener_)
          media_keys_listener_->StopWatchingMediaKey(old->first.key_code());
      }

      event_targets_.erase(old);

      // If a specific command_name was requested, it has now been deleted so no
      // further work is required.
      if (!command_name.empty())
        break;
    }
  }

  // If we're no longer listening to any media keys, tell the browser that
  // it can start handling media keys.
  if (any_media_keys_removed && !IsListeningToAnyMediaKeys()) {
    if (content::MediaKeysListenerManager::
            IsMediaKeysListenerManagerEnabled()) {
      content::MediaKeysListenerManager* media_keys_listener_manager =
          content::MediaKeysListenerManager::GetInstance();
      DCHECK(media_keys_listener_manager);

      media_keys_listener_manager->EnableInternalMediaKeyHandling();
    } else {
#if BUILDFLAG(IS_CHROMEOS_ASH)
      MediaClientImpl::Get()->DisableCustomMediaKeyHandler(browser_context_,
                                                           this);
#endif
    }
  }
}

void ExtensionKeybindingRegistry::Init() {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  if (!registry)
    return;  // ExtensionRegistry can be null during testing.

  for (const scoped_refptr<const Extension>& extension :
       registry->enabled_extensions()) {
    if (ExtensionMatchesFilter(extension.get()))
      AddExtensionKeybindings(extension.get(), std::string());
  }
}

bool ExtensionKeybindingRegistry::ShouldIgnoreCommand(
    const std::string& command) const {
  return Command::IsActionRelatedCommand(command);
}

bool ExtensionKeybindingRegistry::NotifyEventTargets(
    const ui::Accelerator& accelerator) {
  return ExecuteCommands(accelerator, std::string());
}

void ExtensionKeybindingRegistry::CommandExecuted(
    const ExtensionId& extension_id,
    const std::string& command) {
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  if (!extension)
    return;

  base::Value::List args;
  args.Append(command);

  base::Value tab_value;
  if (delegate_) {
    content::WebContents* web_contents =
        delegate_->GetWebContentsForExtension();
    // Grant before sending the event so that the permission is granted before
    // the extension acts on the command. NOTE: The Global Commands handler does
    // not set the delegate as it deals only with named commands (not
    // page/browser actions that are associated with the current page directly).
    ActiveTabPermissionGranter* granter =
        web_contents ? TabHelper::FromWebContents(web_contents)
                           ->active_tab_permission_granter()
                     : nullptr;
    if (granter) {
      granter->GrantIfRequested(extension);
    }

    if (web_contents) {
      // The action APIs (browserAction, pageAction, action) are only available
      // to privileged extension contexts. As such, we deterministically know
      // that the right context type here is privileged.
      constexpr mojom::ContextType context_type =
          mojom::ContextType::kPrivilegedExtension;
      ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
          ExtensionTabUtil::GetScrubTabBehavior(extension, context_type,
                                                web_contents);
      tab_value = base::Value(ExtensionTabUtil::CreateTabObject(
                                  web_contents, scrub_tab_behavior, extension)
                                  .ToValue());
    }
  }

  args.Append(std::move(tab_value));

  auto event =
      std::make_unique<Event>(events::COMMANDS_ON_COMMAND, kOnCommandEventName,
                              std::move(args), browser_context_);
  event->user_gesture = EventRouter::USER_GESTURE_ENABLED;
  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id, std::move(event));
}

bool ExtensionKeybindingRegistry::IsAcceleratorRegistered(
    const ui::Accelerator& accelerator) const {
  return event_targets_.find(accelerator) != event_targets_.end();
}

void ExtensionKeybindingRegistry::AddEventTarget(
    const ui::Accelerator& accelerator,
    const ExtensionId& extension_id,
    const std::string& command_name) {
  event_targets_[accelerator].push_back(
      std::make_pair(extension_id, command_name));
  // Shortcuts except media keys have only one target in the list. See comment
  // about |event_targets_|.
  if (!Command::IsMediaKey(accelerator)) {
    DCHECK_EQ(1u, event_targets_[accelerator].size());
  } else {
    if (media_keys_listener_)
      media_keys_listener_->StartWatchingMediaKey(accelerator.key_code());

    // Tell the browser that it should not handle media keys, since we're going
    // to handle them.
    if (content::MediaKeysListenerManager::
            IsMediaKeysListenerManagerEnabled()) {
      content::MediaKeysListenerManager* media_keys_listener_manager =
          content::MediaKeysListenerManager::GetInstance();
      DCHECK(media_keys_listener_manager);

      media_keys_listener_manager->DisableInternalMediaKeyHandling();
    } else {
#if BUILDFLAG(IS_CHROMEOS_ASH)
      MediaClientImpl::Get()->EnableCustomMediaKeyHandler(browser_context_,
                                                          this);
#endif
    }
  }
}

bool ExtensionKeybindingRegistry::GetFirstTarget(
    const ui::Accelerator& accelerator,
    ExtensionId* extension_id,
    std::string* command_name) const {
  auto targets = event_targets_.find(accelerator);
  if (targets == event_targets_.end())
    return false;

  DCHECK(!targets->second.empty());
  auto first_target = targets->second.begin();
  *extension_id = first_target->first;
  *command_name = first_target->second;
  return true;
}

bool ExtensionKeybindingRegistry::IsEventTargetsEmpty() const {
  return event_targets_.empty();
}

void ExtensionKeybindingRegistry::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (ExtensionMatchesFilter(extension))
    AddExtensionKeybindings(extension, std::string());
}

void ExtensionKeybindingRegistry::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  if (ExtensionMatchesFilter(extension))
    RemoveExtensionKeybinding(extension, std::string());
}

void ExtensionKeybindingRegistry::OnExtensionCommandAdded(
    const ExtensionId& extension_id,
    const Command& command) {
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  // During install and uninstall the extension won't be found. We'll catch
  // those events above, with the OnExtension[Unloaded|Loaded], so we ignore
  // this event.
  if (!extension || !ExtensionMatchesFilter(extension))
    return;

  // Component extensions trigger OnExtensionLoaded() for extension
  // installs as well as loads. This can cause adding of multiple key
  // targets.
  if (extension->location() == mojom::ManifestLocation::kComponent)
    return;

  AddExtensionKeybindings(extension, command.command_name());
}

void ExtensionKeybindingRegistry::OnExtensionCommandRemoved(
    const ExtensionId& extension_id,
    const Command& command) {
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  // During install and uninstall the extension won't be found. We'll catch
  // those events above, with the OnExtension[Unloaded|Loaded], so we ignore
  // this event.
  if (!extension || !ExtensionMatchesFilter(extension))
    return;

  RemoveExtensionKeybinding(extension, command.command_name());
}

void ExtensionKeybindingRegistry::OnCommandServiceDestroying() {
  command_service_observation_.Reset();
}

void ExtensionKeybindingRegistry::OnMediaKeysAccelerator(
    const ui::Accelerator& accelerator) {
  NotifyEventTargets(accelerator);
}

bool ExtensionKeybindingRegistry::ExtensionMatchesFilter(
    const Extension* extension) {
  switch (extension_filter_) {
    case ALL_EXTENSIONS:
      return true;
    case PLATFORM_APPS_ONLY:
      return extension->is_platform_app();
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return false;
}

bool ExtensionKeybindingRegistry::ExecuteCommands(
    const ui::Accelerator& accelerator,
    const ExtensionId& extension_id) {
  auto targets = event_targets_.find(accelerator);
  if (targets == event_targets_.end() || targets->second.empty())
    return false;

  bool executed = false;
  for (TargetList::const_iterator it = targets->second.begin();
       it != targets->second.end(); it++) {
    if (!EventRouter::Get(browser_context_)
             ->ExtensionHasEventListener(it->first, kOnCommandEventName)) {
      continue;
    }

    if (extension_id.empty() || it->first == extension_id) {
      CommandExecuted(it->first, it->second);
      executed = true;
    }
  }

  return executed;
}

bool ExtensionKeybindingRegistry::IsListeningToAnyMediaKeys() const {
  for (const auto& accelerator_target : event_targets_) {
    if (Command::IsMediaKey(accelerator_target.first))
      return true;
  }
  return false;
}

}  // namespace extensions
