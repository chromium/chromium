// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subresource_filter;

import org.chromium.base.annotations.CalledByNative;

/**
 * Class which aids in publishing test rulesets for SubresourceFilter instrumentation tests.
 * All methods and members must be called on the UI thread.
 */
public final class TestRulesetPublisher {
    private boolean mPublished;

    public void createAndPublishRulesetDisallowingSuffixForTesting(String suffix) {
        nativeCreateAndPublishRulesetDisallowingSuffixForTesting(suffix);
    }

    public boolean isPublished() {
        return mPublished;
    }

    @CalledByNative
    private void onRulesetPublished() {
        mPublished = true;
    }

    private native void nativeCreateAndPublishRulesetDisallowingSuffixForTesting(String suffix);
}
