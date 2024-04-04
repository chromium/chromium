// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_CREATE_SHORTCUT_FOR_CURRENT_WEB_CONTENTS_TASK_H_
#define CHROME_BROWSER_SHORTCUTS_CREATE_SHORTCUT_FOR_CURRENT_WEB_CONTENTS_TASK_H_

#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}  // namespace content

namespace shortcuts {

// Stub starting point for shortcut creation.
class CreateShortcutForCurrentWebContentsTask {
 public:
  explicit CreateShortcutForCurrentWebContentsTask(
      base::WeakPtr<content::WebContents> web_contents);
};

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_CREATE_SHORTCUT_FOR_CURRENT_WEB_CONTENTS_TASK_H_
