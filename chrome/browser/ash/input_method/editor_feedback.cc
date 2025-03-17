// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_feedback.h"

#include <string>
#include <string_view>

#include "chrome/browser/feedback/feedback_uploader_chrome.h"  // IWYU pragma: keep - Required for pointer cast.
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/specialized_features/feedback.h"
#include "components/feedback/feedback_constants.h"

namespace ash::input_method {

bool SendEditorFeedback(Profile* profile, std::string_view description) {
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
      *uploader, feedback::kOrcaFeedbackProductId, std::string(description));
  return true;
}
}  // namespace ash::input_method
