// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/chrome_browser_main_extra_parts_lacros.h"

#include "chrome/browser/lacros/automation_manager_lacros.h"
#include "chrome/browser/lacros/task_manager_lacros.h"

ChromeBrowserMainExtraPartsLacros::ChromeBrowserMainExtraPartsLacros() =
    default;
ChromeBrowserMainExtraPartsLacros::~ChromeBrowserMainExtraPartsLacros() =
    default;

void ChromeBrowserMainExtraPartsLacros::PostBrowserStart() {
  automation_manager_ = std::make_unique<AutomationManagerLacros>();
  task_manager_provider_ = std::make_unique<crosapi::TaskManagerLacros>();
}
