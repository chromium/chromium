// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.chrome.browser.ui.TabObscuringHandlerSupplier;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.AccessibilityUtil;

/**
 * Provides default implementations of {@link AssistantStaticDependencies} for Chrome.
 */
public interface AssistantStaticDependenciesChrome extends AssistantStaticDependencies {
    @Override
    default AccessibilityUtil getAccessibilityUtil() {
        return ChromeAccessibilityUtil.get();
    }

    @Override
    @Nullable
    default AssistantTabObscuringUtil getTabObscuringUtilOrNull(WindowAndroid windowAndroid) {
        TabObscuringHandler tabObscuringHandler =
                TabObscuringHandlerSupplier.getValueOrNullFrom(windowAndroid);
        assert tabObscuringHandler != null;
        if (tabObscuringHandler == null) {
            return null;
        }

        return new AssistantTabObscuringUtilChrome(tabObscuringHandler);
    }

    @Override
    default AssistantInfoPageUtil getInfoPageUtil() {
        return new AssistantInfoPageUtilChrome();
    }

    @Override
    default AssistantFeedbackUtil getFeedbackUtil() {
        return new AssistantFeedbackUtilChrome();
    }
}
