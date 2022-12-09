// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Helper methods for accessibility.
 */
@JNINamespace("autofill")
public class AutofillAccessibilityUtils {
    // Avoid instantiation by accident.
    private AutofillAccessibilityUtils() {}

    @CalledByNative
    private static void announce(String message) {
        AccessibilityManager am =
                (AccessibilityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.ACCESSIBILITY_SERVICE);
        if (am == null || !am.isEnabled() || !am.isTouchExplorationEnabled()) {
            return;
        }

        AccessibilityEvent accessibilityEvent = AccessibilityEvent.obtain();
        accessibilityEvent.setEventType(AccessibilityEvent.TYPE_ANNOUNCEMENT);
        accessibilityEvent.getText().add(message);

        am.sendAccessibilityEvent(accessibilityEvent);
    }
}
