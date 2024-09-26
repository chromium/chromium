// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_UTILS_H_
#define CHROME_BROWSER_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_UTILS_H_

class Profile;

namespace content {
class WebUIDataSource;
}

namespace history_embeddings {

bool IsHistoryEmbeddingsEnabledForProfile(Profile* profile);

// Return if the feature is enabled and the setting is visible; i.e. if users
// have the option to opt-in/out of the history embeddings behavior.
bool IsHistoryEmbeddingsSettingVisible(Profile* profile);

void PopulateSourceForWebUI(content::WebUIDataSource* source);

}  // namespace history_embeddings

#endif  // CHROME_BROWSER_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_UTILS_H_
