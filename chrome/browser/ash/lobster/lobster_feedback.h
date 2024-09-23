// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_FEEDBACK_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_FEEDBACK_H_

#include <string_view>

class Profile;

// Returns true if the feedback is successfully queued for submission.
// Returns false otherwise (e.g. the feedback mechanism is disabled).
bool SendLobsterFeedback(Profile* profile,
                         std::string_view query,
                         std::string_view model_version,
                         std::string_view user_description,
                         std::string_view image_bytes);

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_FEEDBACK_H_
