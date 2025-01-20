// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/browser_instance/browser_app_instance.h"

#include <utility>

#include "components/app_constants/constants.h"
#include "components/exo/shell_surface_util.h"
#include "ui/wm/core/window_util.h"

namespace apps {

namespace {

std::string GetWindowUniqueId(aura::Window* window) {
  const std::string* id = exo::GetShellApplicationId(window);
  return id ? *id : "";
}

}  // namespace

BrowserAppInstance::BrowserAppInstance(base::UnguessableToken id,
                                       Type type,
                                       std::string app_id,
                                       aura::Window* window,
                                       std::string title,
                                       bool is_browser_active,
                                       bool is_web_contents_active,
                                       uint32_t browser_session_id,
                                       uint32_t restored_browser_session_id)
    : id(id),
      type(type),
      app_id(app_id),
      window(window),
      title(title),
      is_browser_active_deprecated(is_browser_active),
      is_web_contents_active(is_web_contents_active),
      browser_session_id(browser_session_id),
      restored_browser_session_id(restored_browser_session_id) {}

BrowserAppInstance::~BrowserAppInstance() = default;

bool BrowserAppInstance::MaybeUpdate(aura::Window* new_window,
                                     std::string new_title,
                                     bool new_is_browser_active,
                                     bool new_is_web_contents_active,
                                     uint32_t new_browser_session_id,
                                     uint32_t new_restored_browser_session_id) {
  if (window == new_window && title == new_title &&
      is_browser_active_deprecated == new_is_browser_active &&
      is_web_contents_active == new_is_web_contents_active &&
      browser_session_id == new_browser_session_id &&
      restored_browser_session_id == new_restored_browser_session_id) {
    return false;
  }
  window = new_window;
  title = std::move(new_title);
  is_browser_active_deprecated = new_is_browser_active;
  is_web_contents_active = new_is_web_contents_active;
  browser_session_id = new_browser_session_id;
  restored_browser_session_id = new_restored_browser_session_id;
  return true;
}

BrowserAppInstanceUpdate BrowserAppInstance::ToUpdate() const {
  BrowserAppInstanceUpdate update;
  update.id = id;
  update.type = type;
  update.app_id = app_id;
  update.window_id = GetWindowUniqueId(window);
  update.title = title;
  update.is_browser_active = is_browser_active_deprecated;
  update.is_web_contents_active = is_web_contents_active;
  update.browser_session_id = browser_session_id;
  update.restored_browser_session_id = restored_browser_session_id;
  return update;
}

bool BrowserAppInstance::is_browser_active() const {
  return wm::IsActiveWindow(window);
}

BrowserWindowInstance::BrowserWindowInstance(
    base::UnguessableToken id,
    aura::Window* window,
    uint32_t browser_session_id,
    uint32_t restored_browser_session_id,
    bool is_incognito,
    bool is_active)
    : id(id),
      window(window),
      browser_session_id(browser_session_id),
      restored_browser_session_id(restored_browser_session_id),
      is_incognito(is_incognito),
      is_active_deprecated(is_active) {}

BrowserWindowInstance::BrowserWindowInstance(BrowserWindowInstanceUpdate update,
                                             aura::Window* window)
    : id(update.id),
      window(window),
      browser_session_id(update.browser_session_id),
      restored_browser_session_id(update.restored_browser_session_id),
      is_incognito(update.is_incognito),
      is_active_deprecated(update.is_active) {}

BrowserWindowInstance::~BrowserWindowInstance() = default;

bool BrowserWindowInstance::MaybeUpdate(bool new_is_active) {
  if (is_active_deprecated == new_is_active) {
    return false;
  }
  is_active_deprecated = new_is_active;
  return true;
}

BrowserWindowInstanceUpdate BrowserWindowInstance::ToUpdate() const {
  return BrowserWindowInstanceUpdate{id,
                                     GetWindowUniqueId(window),
                                     is_active_deprecated,
                                     browser_session_id,
                                     restored_browser_session_id,
                                     is_incognito};
}

// TODO(crbug.com/373972275): This function is now constant and could be
// removed.
std::string BrowserWindowInstance::GetAppId() const {
  return app_constants::kChromeAppId;
}

bool BrowserWindowInstance::is_active() const {
  return wm::IsActiveWindow(window);
}

}  // namespace apps
