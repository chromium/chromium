// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_INITIALIZE_FEATURE_LIST_ANDROID_H_
#define CHROME_BROWSER_ANDROID_INITIALIZE_FEATURE_LIST_ANDROID_H_

namespace variations::android {

// Returns whether Java called InitializeFeatureList.initializeLocalState() or
// InitializeFeatureList.initializeFeatureList(), which create the ThreadPool,
// the BrowserTaskExecutor, and Local State before content main runs.
bool DidInitLocalStateEarly();

// Creates the FeatureList and starts the ThreadPool if
// InitializeFeatureList.initializeFeatureList() has not already done so (e.g.
// because the first-run variations seed was being fetched). Must be called
// only when DidInitLocalStateEarly() is true.
void EnsureFeatureListInitialized();

}  // namespace variations::android

#endif  // CHROME_BROWSER_ANDROID_INITIALIZE_FEATURE_LIST_ANDROID_H_
