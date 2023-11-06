// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_APPS_CROS_APPS_KEY_EVENT_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_CROS_APPS_CROS_APPS_KEY_EVENT_HANDLER_H_

#include "base/memory/raw_ref.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/aura/env.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"

class Profile;
class CrosAppsKeyEventHandler : public KeyedService, public ui::EventHandler {
 public:
  explicit CrosAppsKeyEventHandler(Profile* profile);
  CrosAppsKeyEventHandler(const CrosAppsKeyEventHandler&) = delete;
  CrosAppsKeyEventHandler& operator=(const CrosAppsKeyEventHandler&) = delete;
  ~CrosAppsKeyEventHandler() override;

  // ui::EventHandler
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  // This class is a BrowserContextKeyedService, so it's owned by Profile.
  const raw_ref<Profile> profile_;
};

#endif  // CHROME_BROWSER_CHROMEOS_CROS_APPS_CROS_APPS_KEY_EVENT_HANDLER_H_
