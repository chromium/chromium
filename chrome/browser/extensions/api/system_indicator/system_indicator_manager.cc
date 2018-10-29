// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_indicator/system_indicator_manager.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_icon_observer.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/common/extensions/api/system_indicator.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/gfx/image/image.h"

namespace extensions {

namespace system_indicator = api::system_indicator;

// Observes clicks on a given status icon and forwards the event to the
// appropriate extension.  Handles icon updates, and responsible for creating
// and removing the icon from the notification area during construction and
// destruction.
class ExtensionIndicatorIcon : public StatusIconObserver,
                               public ExtensionActionIconFactory::Observer {
 public:
  static std::unique_ptr<ExtensionIndicatorIcon> Create(
      const Extension* extension,
      ExtensionAction* action,
      Profile* profile,
      StatusTray* status_tray);
  ~ExtensionIndicatorIcon() override;

  // StatusIconObserver implementation.
  void OnStatusIconClicked() override;

  // ExtensionActionIconFactory::Observer implementation.
  void OnIconUpdated() override;

 private:
  ExtensionIndicatorIcon(const Extension* extension,
                         ExtensionAction* action,
                         Profile* profile,
                         StatusTray* status_tray);

  const extensions::Extension* extension_;
  StatusTray* status_tray_;
  StatusIcon* icon_;
  Profile* profile_;
  ExtensionActionIconFactory icon_factory_;
};

std::unique_ptr<ExtensionIndicatorIcon> ExtensionIndicatorIcon::Create(
    const Extension* extension,
    ExtensionAction* action,
    Profile* profile,
    StatusTray* status_tray) {
  std::unique_ptr<ExtensionIndicatorIcon> extension_icon(
      new ExtensionIndicatorIcon(extension, action, profile, status_tray));

  // Check if a status icon was successfully created.
  if (extension_icon->icon_)
    return extension_icon;

  // We could not create a status icon.
  return std::unique_ptr<ExtensionIndicatorIcon>();
}

ExtensionIndicatorIcon::~ExtensionIndicatorIcon() {
  if (icon_) {
    icon_->RemoveObserver(this);
    status_tray_->RemoveStatusIcon(icon_);
  }
}

void ExtensionIndicatorIcon::OnStatusIconClicked() {
  std::unique_ptr<base::ListValue> params(
      api::system_indicator::OnClicked::Create());

  EventRouter* event_router = EventRouter::Get(profile_);
  std::unique_ptr<Event> event(new Event(
      events::SYSTEM_INDICATOR_ON_CLICKED,
      system_indicator::OnClicked::kEventName, std::move(params), profile_));
  event_router->DispatchEventToExtension(extension_->id(), std::move(event));
}

void ExtensionIndicatorIcon::OnIconUpdated() {
  icon_->SetImage(
      icon_factory_.GetIcon(ExtensionAction::kDefaultTabId).AsImageSkia());
}

ExtensionIndicatorIcon::ExtensionIndicatorIcon(const Extension* extension,
                                               ExtensionAction* action,
                                               Profile* profile,
                                               StatusTray* status_tray)
    : extension_(extension),
      status_tray_(status_tray),
      icon_(NULL),
      profile_(profile),
      icon_factory_(profile, extension, action, this) {
  // Get the icon image and tool tip for the status icon. The extension name is
  // used as the tool tip.
  gfx::ImageSkia icon_image =
      icon_factory_.GetIcon(ExtensionAction::kDefaultTabId).AsImageSkia();
  base::string16 tool_tip = base::UTF8ToUTF16(extension_->name());

  icon_ = status_tray_->CreateStatusIcon(
      StatusTray::OTHER_ICON, icon_image, tool_tip);
  if (icon_)
    icon_->AddObserver(this);
}

SystemIndicatorManager::SystemIndicatorManager(Profile* profile,
                                               StatusTray* status_tray)
    : profile_(profile),
      status_tray_(status_tray),
      extension_action_observer_(this),
      extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
  extension_action_observer_.Add(ExtensionActionAPI::Get(profile_));
}

SystemIndicatorManager::~SystemIndicatorManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void SystemIndicatorManager::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void SystemIndicatorManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  RemoveIndicator(extension->id());
}

void SystemIndicatorManager::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (profile_->GetOriginalProfile() != browser_context ||
      extension_action->action_type() != ActionInfo::TYPE_SYSTEM_INDICATOR)
    return;

  std::string extension_id = extension_action->extension_id();
  if (extension_action->GetIsVisible(ExtensionAction::kDefaultTabId)) {
    CreateOrUpdateIndicator(
        ExtensionRegistry::Get(profile_)->enabled_extensions().GetByID(
            extension_id),
        extension_action);
  } else {
    RemoveIndicator(extension_id);
  }
}

bool SystemIndicatorManager::SendClickEventToExtensionForTest(
    const std::string& extension_id) {
  auto it = system_indicators_.find(extension_id);

  if (it == system_indicators_.end())
    return false;

  it->second->OnStatusIconClicked();
  return true;
}

void SystemIndicatorManager::CreateOrUpdateIndicator(
    const Extension* extension,
    ExtensionAction* extension_action) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it = system_indicators_.find(extension->id());
  if (it != system_indicators_.end()) {
    it->second->OnIconUpdated();
    return;
  }

  std::unique_ptr<ExtensionIndicatorIcon> extension_icon =
      ExtensionIndicatorIcon::Create(extension, extension_action, profile_,
                                     status_tray_);
  if (extension_icon)
    system_indicators_[extension->id()] = std::move(extension_icon);
}

void SystemIndicatorManager::RemoveIndicator(const std::string& extension_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  system_indicators_.erase(extension_id);
}

}  // namespace extensions
