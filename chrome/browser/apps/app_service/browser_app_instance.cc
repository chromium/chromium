// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/browser_app_instance.h"

#include <utility>

namespace apps {

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

BrowserAppInstance::~BrowserAppInstance() = default;
BrowserAppInstance::BrowserAppInstance(BrowserAppInstance&&) = default;
BrowserAppInstance& BrowserAppInstance::operator=(BrowserAppInstance&&) =
    default;

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

BrowserWindowInstance::BrowserWindowInstance(base::UnguessableToken id,
                                             aura::Window* window,
                                             bool is_active)
    : id(id), window(window), is_active(is_active) {}

BrowserWindowInstance::~BrowserWindowInstance() = default;
BrowserWindowInstance::BrowserWindowInstance(BrowserWindowInstance&&) = default;
BrowserWindowInstance& BrowserWindowInstance::operator=(
    BrowserWindowInstance&&) = default;

bool BrowserWindowInstance::MaybeUpdate(bool is_active) {
  if (this->is_active == is_active) {
    return false;
  }
  this->is_active = is_active;
  return true;
}

}  // namespace apps
