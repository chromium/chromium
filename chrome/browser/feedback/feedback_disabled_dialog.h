// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_FEEDBACK_DISABLED_DIALOG_H_
#define CHROME_BROWSER_FEEDBACK_FEEDBACK_DISABLED_DIALOG_H_

#include "ui/gfx/native_ui_types.h"

class Profile;

namespace chrome {

// Shows a browser-modal dialog explaining that feedback cannot be submitted
// for `profile`. `parent` and `profile` must not be null.
void ShowFeedbackDisabledDialog(gfx::NativeWindow parent,
                                const Profile* profile);

}  // namespace chrome

#endif  // CHROME_BROWSER_FEEDBACK_FEEDBACK_DISABLED_DIALOG_H_
