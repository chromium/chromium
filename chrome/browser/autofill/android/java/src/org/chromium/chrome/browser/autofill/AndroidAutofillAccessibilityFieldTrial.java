// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.chromium.build.annotations.NullMarked;

/**
 * This interface allows downstream implementations to enroll clients for a field trial that
 * deprecates the use of compatibility mode using accessibility APIs. The trial has no effect on the
 * behavior of the browser but reflects whether a server-provided configuration affects it already.
 */
@NullMarked
public interface AndroidAutofillAccessibilityFieldTrial {
    static final String AUTOFILL_VIA_A11Y_DEPRECATION_DEFAULT = "Default";

    /**
     * Determine whether a server-provided config affects this client. If so, enroll the client into
     * a synthetic trial to catch regressions early.
     */
    default String getTrialGroupForPackage() {
        return AUTOFILL_VIA_A11Y_DEPRECATION_DEFAULT;
    }
}
