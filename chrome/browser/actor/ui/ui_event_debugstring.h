// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_UI_EVENT_DEBUGSTRING_H_
#define CHROME_BROWSER_ACTOR_UI_UI_EVENT_DEBUGSTRING_H_

#include "chrome/browser/actor/ui/ui_event.h"

namespace actor::ui {

std::string DebugString(const UiEvent&);
std::string DebugString(const AsyncUiEvent&);
std::string DebugString(const SyncUiEvent&);
}

#endif  // CHROME_BROWSER_ACTOR_UI_UI_EVENT_DEBUGSTRING_H_
