// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subresource_filter;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

/**
 * Class which aids in publishing test rulesets for SubresourceFilter instrumentation tests. All
 * methods and members must be called on the UI thread.
 */
public final class TestRulesetPublisher {
    private boolean mPublished;

    public void createAndPublishRulesetDisallowingSuffixForTesting(String suffix) {
        TestRulesetPublisherJni.get()
                .createAndPublishRulesetDisallowingSuffixForTesting(this, suffix);
    }

    public boolean isPublished() {
        return mPublished;
    }

    @CalledByNative
    private void onRulesetPublished() {
        mPublished = true;
    }

    @NativeMethods
    interface Natives {
        void createAndPublishRulesetDisallowingSuffixForTesting(
                TestRulesetPublisher obj, String suffix);
    }
}
