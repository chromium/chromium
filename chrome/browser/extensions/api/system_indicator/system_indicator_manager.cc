// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_indicator/system_indicator_manager.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_icon_observer.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/common/extensions/api/system_indicator.h"
#include "chrome/common/extensions/api/system_indicator/system_indicator_handler.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/common/extension.h"
#include "extensions/common/icons/extension_icon_set.h"

namespace extensions {

namespace system_indicator = api::system_indicator;

// Observes clicks on a given status icon and forwards the event to the
// appropriate extension.  Handles icon updates, and responsible for creating
// and removing the icon from the notification area during construction and
// destruction.
class ExtensionIndicatorIcon : public StatusIconObserver,
                               public IconImage::Observer {
 public:
  static std::unique_ptr<ExtensionIndicatorIcon> Create(
      const Extension& extension,
      const ExtensionIconSet& icon_set,
      Profile* profile,
      StatusTray* status_tray);

  ExtensionIndicatorIcon(const ExtensionIndicatorIcon&) = delete;
  ExtensionIndicatorIcon& operator=(const ExtensionIndicatorIcon&) = delete;

  ~ExtensionIndicatorIcon() override;

  // Sets the dynamic icon for the indicator.
  void SetDynamicIcon(gfx::Image dynamic_icon);

  // StatusIconObserver:
  void OnStatusIconClicked() override;

 private:
  ExtensionIndicatorIcon(const Extension& extension,
                         const ExtensionIconSet& manifest_icon_set,
                         Profile* profile,
                         StatusTray* status_tray);

  // IconImage::Observer:
  void OnExtensionIconImageChanged(IconImage* image) override;

  raw_ptr<const Extension> extension_;
  raw_ptr<StatusTray> status_tray_;
  raw_ptr<StatusIcon, DanglingUntriaged> status_icon_;
  raw_ptr<Profile> profile_;
  IconImage manifest_icon_;
  gfx::Image dynamic_icon_;
};

std::unique_ptr<ExtensionIndicatorIcon> ExtensionIndicatorIcon::Create(
    const Extension& extension,
    const ExtensionIconSet& icon_set,
    Profile* profile,
    StatusTray* status_tray) {
  // Private ctor, so have to use WrapUnique.
  auto extension_icon = base::WrapUnique(
      new ExtensionIndicatorIcon(extension, icon_set, profile, status_tray));

  // Check if a status icon was successfully created.
  if (extension_icon->status_icon_)
    return extension_icon;

  // We could not create a status icon.
  return nullptr;
}

ExtensionIndicatorIcon::~ExtensionIndicatorIcon() {
  if (status_icon_) {
    status_icon_->RemoveObserver(this);
    status_tray_->RemoveStatusIcon(status_icon_);
  }
}

void ExtensionIndicatorIcon::SetDynamicIcon(gfx::Image dynamic_icon) {
  dynamic_icon_ = std::move(dynamic_icon);
  status_icon_->SetImage(dynamic_icon_.AsImageSkia());
}

void ExtensionIndicatorIcon::OnStatusIconClicked() {
  auto params(api::system_indicator::OnClicked::Create());

  EventRouter* event_router = EventRouter::Get(profile_);
  std::unique_ptr<Event> event(new Event(
      events::SYSTEM_INDICATOR_ON_CLICKED,
      system_indicator::OnClicked::kEventName, std::move(params), profile_));
  event_router->DispatchEventToExtension(extension_->id(), std::move(event));
}

void ExtensionIndicatorIcon::OnExtensionIconImageChanged(IconImage* image) {
  if (dynamic_icon_.IsEmpty())  // Don't override a dynamically-set icon.
    status_icon_->SetImage(manifest_icon_.image().AsImageSkia());
}

ExtensionIndicatorIcon::ExtensionIndicatorIcon(
    const Extension& extension,
    const ExtensionIconSet& manifest_icon_set,
    Profile* profile,
    StatusTray* status_tray)
    : extension_(&extension),
      status_tray_(status_tray),
      status_icon_(nullptr),
      profile_(profile),
      manifest_icon_(profile,
                     &extension,
                     manifest_icon_set,
                     ExtensionAction::ActionIconSize(),
                     ExtensionAction::FallbackIcon().AsImageSkia(),
                     this) {
  // Get the icon image and tool tip for the status icon. The extension name is
  // used as the tool tip.
  gfx::ImageSkia icon_skia = manifest_icon_.image().AsImageSkia();
  std::u16string tool_tip = base::UTF8ToUTF16(extension_->name());

  status_icon_ = status_tray_->CreateStatusIcon(StatusTray::OTHER_ICON,
                                                icon_skia, tool_tip);
  if (status_icon_)
    status_icon_->AddObserver(this);
}

SystemIndicatorManager::SystemIndicator::SystemIndicator() {}
SystemIndicatorManager::SystemIndicator::~SystemIndicator() = default;

SystemIndicatorManager::SystemIndicatorManager(Profile* profile,
                                               StatusTray* status_tray)
    : profile_(profile), status_tray_(status_tray) {
  extension_registry_observation_.Observe(ExtensionRegistry::Get(profile_));
}

SystemIndicatorManager::~SystemIndicatorManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void SystemIndicatorManager::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void SystemIndicatorManager::SetSystemIndicatorDynamicIcon(
    const Extension& extension,
    gfx::Image icon) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(base::Contains(system_indicators_, extension.id()));
  auto& indicator = system_indicators_[extension.id()];
  indicator.dynamic_icon = icon;
  if (indicator.system_tray_indicator)
    indicator.system_tray_indicator->SetDynamicIcon(std::move(icon));
}

void SystemIndicatorManager::SetSystemIndicatorEnabled(
    const Extension& extension,
    bool is_enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(base::Contains(system_indicators_, extension.id()));
  auto& indicator = system_indicators_[extension.id()];
  bool is_already_enabled = !!indicator.system_tray_indicator;
  if (is_already_enabled == is_enabled)
    return;

  if (is_enabled) {
    indicator.system_tray_indicator = ExtensionIndicatorIcon::Create(
        extension, indicator.manifest_icon_set, profile_, status_tray_);
    // Note: The system tray indicator creation can fail.
    if (indicator.system_tray_indicator && !indicator.dynamic_icon.IsEmpty()) {
      indicator.system_tray_indicator->SetDynamicIcon(indicator.dynamic_icon);
    }
  } else {
    indicator.system_tray_indicator.reset();
  }
}

void SystemIndicatorManager::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!base::Contains(system_indicators_, extension->id()));
  const ExtensionIconSet* indicator_icon =
      SystemIndicatorHandler::GetSystemIndicatorIcon(*extension);
  if (!indicator_icon)
    return;

  auto& indicator = system_indicators_[extension->id()];
  indicator.manifest_icon_set = *indicator_icon;
}

void SystemIndicatorManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());
  system_indicators_.erase(extension->id());
}

bool SystemIndicatorManager::SendClickEventToExtensionForTest(
    const ExtensionId& extension_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it = system_indicators_.find(extension_id);

  if (it == system_indicators_.end() ||
      it->second.system_tray_indicator == nullptr) {
    return false;
  }

  it->second.system_tray_indicator->OnStatusIconClicked();
  return true;
}

}  // namespace extensions
