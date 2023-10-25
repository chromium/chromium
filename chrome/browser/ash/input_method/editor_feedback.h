// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_FEEDBACK_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_FEEDBACK_H_

#include "chrome/browser/profiles/profile.h"
#include "components/feedback/feedback_uploader.h"

namespace ash::input_method {

// Returns true if the feedback is successfully queued for submission.
// Returns false otherwise (e.g. the feedback mechanism is disabled).
bool SendEditorFeedback(Profile* profile, std::string_view description);

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_FEEDBACK_H_
