// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_feedback.h"

#include <string>
#include <string_view>
#include <utility>

#include "chrome/browser/feedback/feedback_uploader_chrome.h"  // IWYU pragma: keep - Required for pointer cast.
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/specialized_features/feedback.h"
#include "components/feedback/feedback_constants.h"

bool SendLobsterFeedback(Profile* profile,
                         std::string description,
                         std::string_view image_bytes) {
  // NOTE: `FeedbackUploaderFactoryChrome` (in //chrome/browser/feedback/)
  // returns different instances to `FeedbackUploaderFactory` (in
  // //components/feedback/content). The correct instance should be obtained
  // from `FeedbackUploaderFactoryChrome`.
  feedback::FeedbackUploader* uploader =
      feedback::FeedbackUploaderFactoryChrome::GetForBrowserContext(profile);
  if (!uploader) {
    return false;
  }

  specialized_features::SendFeedback(
      *uploader, feedback::kLobsterFeedbackProductId, std::move(description),
      std::string(image_bytes));
  return true;
}
