// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.AccessibilityUtil;

/**
 * Generic static dependencies interface. The concrete implementation will depend on the browser
 * framework, i.e., WebLayer vs. Chrome.
 */
@JNINamespace("autofill_assistant")
public interface AssistantStaticDependencies {
    AccessibilityUtil getAccessibilityUtil();

    /**
     * Returns a utility for obscuring all tabs. NOTE: Each call returns a new instance that can
     * only unobscure what it obscured!
     */
    @Nullable
    AssistantTabObscuringUtil getTabObscuringUtilOrNull(WindowAndroid windowAndroid);

    @CalledByNative
    AssistantInfoPageUtil getInfoPageUtil();

    AssistantFeedbackUtil getFeedbackUtil();
}
