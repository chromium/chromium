// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

/**
 * Flags of this type assume native is loaded and the value can be retrieved directly from native.
 */
public class PostNativeFlag extends Flag {
    public PostNativeFlag(String featureName) {
        super(featureName);
    }

    @Override
    public boolean isEnabled() {
        return ChromeFeatureList.isEnabled(mFeatureName);
    }
}
