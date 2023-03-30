// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_manager.h"

#include "base/no_destructor.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace chromeos {

// static
extensions::BrowserContextKeyedAPIFactory<EventManager>*
EventManager::GetFactoryInstance() {
  static base::NoDestructor<
      extensions::BrowserContextKeyedAPIFactory<EventManager>>
      instance;
  return instance.get();
}

// static
EventManager* EventManager::Get(content::BrowserContext* browser_context) {
  return extensions::BrowserContextKeyedAPIFactory<EventManager>::Get(
      browser_context);
}

EventManager::EventManager(content::BrowserContext* context)
    : browser_context_(context) {}

EventManager::~EventManager() = default;

}  // namespace chromeos
