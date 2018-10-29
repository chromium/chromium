// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_RESOURCE_BUNDLE_HELPER_H_
#define CHROME_BROWSER_CHROME_RESOURCE_BUNDLE_HELPER_H_

#include <string>

class ChromeFeatureListCreator;

// Loads the local state, and returns the application locale. An empty return
// value indicates the ResouceBundle couldn't be loaded.
std::string LoadLocalState(
    ChromeFeatureListCreator* chrome_feature_list_creator,
    bool is_running_tests);

#endif  // CHROME_BROWSER_CHROME_RESOURCE_BUNDLE_HELPER_H_
