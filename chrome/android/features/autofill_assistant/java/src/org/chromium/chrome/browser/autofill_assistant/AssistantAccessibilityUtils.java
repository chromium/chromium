// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_AUTO;
import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_NO;

import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Nullable;

/**
 * Common accessibility utilities used by autofill assistant.
 */
public class AssistantAccessibilityUtils {
    /*
     * Sets the importance for accessibility and the description for the given field.
     * */
    public static void setAccessibility(View view, @Nullable String accessibilityHint) {
        view.setContentDescription(accessibilityHint);
        if (accessibilityHint != null && TextUtils.isEmpty(accessibilityHint)) {
            view.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        } else {
            view.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_AUTO);
        }
    }
}
