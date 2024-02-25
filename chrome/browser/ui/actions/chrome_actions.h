// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ACTIONS_CHROME_ACTIONS_H_
#define CHROME_BROWSER_UI_ACTIONS_CHROME_ACTIONS_H_

namespace actions {
class ActionManager;
}  // namespace actions

// Called during browser startup. Default command actions are created/
// initialized here.
void InitializeChromeActions(actions::ActionManager* manager);

void InitializeActionIdStringMapping();

#endif  // CHROME_BROWSER_UI_ACTIONS_CHROME_ACTIONS_H_
