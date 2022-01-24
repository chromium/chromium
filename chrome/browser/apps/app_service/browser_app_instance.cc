// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/browser_app_instance.h"

#include <utility>

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "components/exo/shell_surface_util.h"
#include "extensions/common/constants.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/lacros/window_utility.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace apps {

namespace {

std::string GetWindowUniqueId(aura::Window* window) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const std::string* id = exo::GetShellApplicationId(window);
  return id ? *id : "";
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return lacros_window_utility::GetRootWindowUniqueId(window);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace

BrowserAppInstance::BrowserAppInstance(base::UnguessableToken id,
                                       Type type,
                                       std::string app_id,
                                       aura::Window* window,
                                       std::string title,
                                       bool is_browser_active,
                                       bool is_web_contents_active)
    : id(id),
      type(type),
      app_id(app_id),
      window(window),
      title(title),
      is_browser_active(is_browser_active),
      is_web_contents_active(is_web_contents_active) {}

BrowserAppInstance::BrowserAppInstance(BrowserAppInstanceUpdate update,
                                       aura::Window* window)
    : id(update.id),
      type(update.type),
      app_id(update.app_id),
      window(window),
      title(update.title),
      is_browser_active(update.is_browser_active),
      is_web_contents_active(update.is_web_contents_active) {}

BrowserAppInstance::~BrowserAppInstance() = default;

bool BrowserAppInstance::MaybeUpdate(aura::Window* window,
                                     std::string title,
                                     bool is_browser_active,
                                     bool is_web_contents_active) {
  if (this->window == window && this->title == title &&
      this->is_browser_active == is_browser_active &&
      this->is_web_contents_active == is_web_contents_active) {
    return false;
  }
  this->window = window;
  this->title = std::move(title);
  this->is_browser_active = is_browser_active;
  this->is_web_contents_active = is_web_contents_active;
  return true;
}

BrowserAppInstanceUpdate BrowserAppInstance::ToUpdate() const {
  BrowserAppInstanceUpdate update;
  update.id = id;
  update.type = type;
  update.app_id = app_id;
  update.window_id = GetWindowUniqueId(window);
  update.title = title;
  update.is_browser_active = is_browser_active;
  update.is_web_contents_active = is_web_contents_active;
  return update;
}

BrowserWindowInstance::BrowserWindowInstance(base::UnguessableToken id,
                                             aura::Window* window,
                                             bool is_active)
    : id(id), window(window), is_active(is_active) {}

BrowserWindowInstance::BrowserWindowInstance(BrowserWindowInstanceUpdate update,
                                             aura::Window* window)
    : id(update.id), window(window), is_active(update.is_active) {}

BrowserWindowInstance::~BrowserWindowInstance() = default;

bool BrowserWindowInstance::MaybeUpdate(bool is_active) {
  if (this->is_active == is_active) {
    return false;
  }
  this->is_active = is_active;
  return true;
}

BrowserWindowInstanceUpdate BrowserWindowInstance::ToUpdate() const {
  return BrowserWindowInstanceUpdate{id, GetWindowUniqueId(window), is_active};
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::string BrowserWindowInstance::GetAppId() const {
  return crosapi::browser_util::IsLacrosWindow(window)
             ? extension_misc::kLacrosAppId
             : extension_misc::kChromeAppId;
}
#endif

}  // namespace apps
