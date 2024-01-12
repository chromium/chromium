// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_SHORTCUT_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_SHORTCUT_METRICS_H_

namespace apps {

extern const char kShortcutLaunchSourceHistogram[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ShortcutActionSource {
  kLauncher = 0,
  kShelf = 1,
  kMaxValue = kShelf
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ShortcutPinAction { kPin = 0, kUnpin = 1, kMaxValue = kUnpin };

// Records that the user has launched a shortcut, and indicates whether it
// happened via the launcher or shelf.
void RecordShortcutLaunchSource(const ShortcutActionSource action_source);

// Records that the user removed a shortcut, and indicates whether it happened
// via the launcher context menu or shelf context menu.
void RecordShortcutRemovalSource(const ShortcutActionSource action_source);

// Records when the user pins or unpins a shortcut in the shelf. Pinning
// or unpinning can be triggered via the context menu in shortcut item in
// the launcher or shelf.
void RecordShortcutPinAction(const ShortcutPinAction pin_action);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_SHORTCUT_METRICS_H_
