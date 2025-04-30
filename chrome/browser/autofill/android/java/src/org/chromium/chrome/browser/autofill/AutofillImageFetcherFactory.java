// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/** Provides access to {@link AutofillImageFetcher} singleton associated with a {@link Profile}. */
@NullMarked
@JNINamespace("autofill")
public class AutofillImageFetcherFactory {
    private @Nullable static AutofillImageFetcher sAutofillImageFetcherForTesting;

    private AutofillImageFetcherFactory() {}

    /**
     * Retrieves or creates the {@link AutofillImageFetcher} associated with `profile`.
     *
     * <p>Can only be accessed on the main thread.
     *
     * @param profile The {@link Profile} to retrieve the {@link AutofillImageFetcher}.
     * @return The `profile` specific {@link AutofillImageFetcher}.
     */
    public static AutofillImageFetcher getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();

        if (sAutofillImageFetcherForTesting != null) return sAutofillImageFetcherForTesting;

        if (profile == null) {
            throw new IllegalArgumentException(
                    "Attempting to access AutofillImageFetcher with a null profile");
        }
        // Throw an exception if the native pointer isn't initialized yet.
        profile.ensureNativeInitialized();
        return AutofillImageFetcherFactoryJni.get().getForProfile(profile);
    }

    /** Overrides the initialization for tests. */
    public static void setInstanceForTesting(AutofillImageFetcher autofillImageFetcher) {
        sAutofillImageFetcherForTesting = autofillImageFetcher;
        ResettersForTesting.register(() -> sAutofillImageFetcherForTesting = null);
    }

    @NativeMethods
    interface Natives {
        AutofillImageFetcher getForProfile(@JniType("Profile*") Profile profile);
    }
}
