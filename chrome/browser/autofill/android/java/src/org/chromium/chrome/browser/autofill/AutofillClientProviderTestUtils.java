// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;

/**
 * Helper functions for testing Android Autofill in Chrome. It allows to call JNI methods from tests
 * without generating the JNI glue code for non-test targets.
 */
@NullMarked
@JNINamespace("autofill::test")
class AutofillClientProviderTestUtils {
    /**
     * @see {@link AutofillClientProviderUtils#setAutofillAvailabilityToUseForTesting}
     */
    @CalledByNative
    private static void setAutofillAvailability(
            @AndroidAutofillAvailabilityStatus Integer availability) {
        AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(availability);
    }

    /**
     * Convenience method to call (@link #setAutofillAvailability) with null since the java-typic
     * use of {@link ResettersForTesting} does not work in native tests.
     */
    @CalledByNative
    private static void resetAutofillAvailability() {
        AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(null);
    }

    private AutofillClientProviderTestUtils() {}
}
