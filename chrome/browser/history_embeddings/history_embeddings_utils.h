// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_UTILS_H_
#define CHROME_BROWSER_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_UTILS_H_

#include "base/feature_list.h"

class Profile;

namespace content {
class WebUIDataSource;
}

namespace history_embeddings {

// Do not use. For test only.
BASE_DECLARE_FEATURE(kLaunchedHistoryEmbeddings);

// Checks whether the HistoryEmbeddings feature is enabled for `profile`.
bool IsHistoryEmbeddingsEnabledForProfile(Profile* profile);

// Checks whether the HistoryEmbeddingsAnswers feature is enabled for `profile`.
bool IsHistoryEmbeddingsAnswersEnabledForProfile(Profile* profile);

// Return if the feature is enabled and the setting is visible; i.e. if users
// have the option to opt-in/out of the history embeddings behavior.
bool IsHistoryEmbeddingsSettingVisible(Profile* profile);

// Return if the feature is enabled and the setting is visible; i.e. if users
// have the option to opt-in/out of the history embeddings behavior.
bool IsHistoryEmbeddingsAnswersSettingVisible(Profile* profile);

void PopulateSourceForWebUI(content::WebUIDataSource* source, Profile* profile);

// Whether the HistoryEmbeddings feature is enabled. This only checks if the
// feature flags are enabled and does not check the user's opt-in preference
// or eligibility based on the user profile.
bool IsHistoryEmbeddingsFeatureEnabled();

// Whether the HistoryEmbeddingsAnswers feature is enabled. This only checks if
// the feature flags are enabled and does not check the user's opt-in preference
// or eligibility based on the user profile.
bool IsHistoryEmbeddingsAnswersFeatureEnabled();

}  // namespace history_embeddings

#endif  // CHROME_BROWSER_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_UTILS_H_
