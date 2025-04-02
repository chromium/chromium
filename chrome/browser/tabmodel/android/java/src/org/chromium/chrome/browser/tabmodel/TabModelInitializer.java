// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.NullMarked;

/** Class responsible for initializing the tab model infrastructure. */
@NullMarked
public interface TabModelInitializer {
    /** Initialize the {@link TabModelSelector}, {@link TabModel}s, and {@link TabCreator}. */
    void initializeTabModels();
}
