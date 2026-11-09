// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_PREFS_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_PREFS_H_

class PrefRegistrySimple;

namespace task_manager {

// Registers the task manager related prefs.
void RegisterPrefs(PrefRegistrySimple* registry);

// Returns true if the user is allowed to end processes.
bool IsEndProcessEnabled();

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_PREFS_H_
