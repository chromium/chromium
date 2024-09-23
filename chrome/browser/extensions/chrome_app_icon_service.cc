// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_app_icon_service.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/task/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/extensions/chrome_app_icon_service_factory.h"

namespace extensions {

// static
ChromeAppIconService* ChromeAppIconService::Get(
    content::BrowserContext* context) {
  return ChromeAppIconServiceFactory::GetInstance()->GetForBrowserContext(
      context);
}

ChromeAppIconService::ChromeAppIconService(content::BrowserContext* context)
    : context_(context) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  app_updater_ = std::make_unique<ShelfExtensionAppUpdater>(
      this, context, false /* extensions_only */);
#endif

  observation_.Observe(ExtensionRegistry::Get(context_));
}

ChromeAppIconService::~ChromeAppIconService() = default;

void ChromeAppIconService::Shutdown() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  app_updater_.reset();
#endif
}

std::unique_ptr<ChromeAppIcon> ChromeAppIconService::CreateIcon(
    ChromeAppIconDelegate* delegate,
    const std::string& app_id,
    int resource_size_in_dip,
    const ResizeFunction& resize_function) {
  std::unique_ptr<ChromeAppIcon> icon = std::make_unique<ChromeAppIcon>(
      delegate, context_,
      base::BindOnce(&ChromeAppIconService::OnIconDestroyed,
                     weak_ptr_factory_.GetWeakPtr()),
      app_id, resource_size_in_dip, resize_function);

  icon_map_[icon->app_id()].insert(icon.get());
  return icon;
}

std::unique_ptr<ChromeAppIcon> ChromeAppIconService::CreateIcon(
    ChromeAppIconDelegate* delegate,
    const std::string& app_id,
    int resource_size_in_dip) {
  return CreateIcon(delegate, app_id, resource_size_in_dip, ResizeFunction());
}

void ChromeAppIconService::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  OnAppUpdated(extension->id());
}

void ChromeAppIconService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  OnAppUpdated(extension->id());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ChromeAppIconService::OnAppUpdated(
    content::BrowserContext* browser_context,
    const std::string& app_id,
    bool reload_icon) {
  if (!reload_icon)
    return;

  OnAppUpdated(app_id);
}
#endif

void ChromeAppIconService::OnAppUpdated(const std::string& app_id) {
  IconMap::const_iterator it = icon_map_.find(app_id);
  if (it == icon_map_.end())
    return;
  // Set can be updated during the UpdateIcon call.
  const std::set<raw_ptr<ChromeAppIcon, SetExperimental>> icons_to_update =
      it->second;
  for (ChromeAppIcon* icon : icons_to_update) {
    if (it->second.count(icon))
      icon->UpdateIcon();
  }
}

void ChromeAppIconService::OnIconDestroyed(ChromeAppIcon* icon) {
  DCHECK(icon);
  auto it = icon_map_.find(icon->app_id());
  CHECK(it != icon_map_.end(), base::NotFatalUntil::M130);
  it->second.erase(icon);
  if (it->second.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ChromeAppIconService::MaybeCleanupIconSet,
                       weak_ptr_factory_.GetWeakPtr(), icon->app_id()));
  }
}

void ChromeAppIconService::MaybeCleanupIconSet(const std::string& app_id) {
  auto it = icon_map_.find(app_id);
  if (it != icon_map_.end() && it->second.empty())
    icon_map_.erase(it);
}

}  // namespace extensions
