// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.annotation.IntDef;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeCall;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Collects information about the HTTP headers passed into an Intent as Browser.EXTRA_HEADERS and
 * records UMA. Call {@link #recordHeader} for each header in the Intent and {@link #report}
 * afterwards.
 *
 * Lifecycle: Create an instance of this class for each Intent whose Headers you want to record.
 * Thread safety: All methods on this class should be called on the UI thread.
 */
public class IntentHeadersRecorder {
    /** Determines whether a header is CORS Safelisted or not. */
    @JNINamespace("chrome::android")
    /* package */ static class HeaderClassifier {
        /* package */ boolean isCorsSafelistedHeader(String name, String value) {
            return nativeIsCorsSafelistedHeader(name, value);
        }

        @NativeCall("HeaderClassifier")
        private static native boolean nativeIsCorsSafelistedHeader(String name, String value);
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({IntentHeadersResult.FIRST_PARTY_NO_HEADERS,
            IntentHeadersResult.FIRST_PARTY_ONLY_SAFE_HEADERS,
            IntentHeadersResult.FIRST_PARTY_UNSAFE_HEADERS,
            IntentHeadersResult.THIRD_PARTY_NO_HEADERS,
            IntentHeadersResult.THIRD_PARTY_ONLY_SAFE_HEADERS,
            IntentHeadersResult.THIRD_PARTY_UNSAFE_HEADERS})
    @VisibleForTesting
    @interface IntentHeadersResult {
        // Don't reuse or reorder values. If you add something, update NUM_ENTRIES.
        int FIRST_PARTY_NO_HEADERS = 0;
        int FIRST_PARTY_ONLY_SAFE_HEADERS = 1;
        int FIRST_PARTY_UNSAFE_HEADERS = 2;
        int THIRD_PARTY_NO_HEADERS = 3;
        int THIRD_PARTY_ONLY_SAFE_HEADERS = 4;
        int THIRD_PARTY_UNSAFE_HEADERS = 5;
        int NUM_ENTRIES = 6;
    }

    private final HeaderClassifier mClassifier;
    private int mSafeHeaders;
    private int mUnsafeHeaders;

    /** Creates this class with a custom classifier (for testing). */
    public IntentHeadersRecorder(HeaderClassifier classifier) {
        mClassifier = classifier;
    }

    /** Creates this class with a classifier that checks Chrome native code. */
    public IntentHeadersRecorder() {
        this(new HeaderClassifier());
    }

    /* Records that a HTTP header has been used. */
    public void recordHeader(String name, String value) {
        if (mClassifier.isCorsSafelistedHeader(name, value)) mSafeHeaders++;
        else mUnsafeHeaders++;
    }

    /**
     * Logs the types of headers that have previously been {@link #recordHeader}ed.
     * @param firstParty Whether the Intent is from a first or third party app. As it is just for
     *                   logging, this is not security sensitive.
     */
    public void report(boolean firstParty) {
        if (firstParty) {
            if (mSafeHeaders == 0 && mUnsafeHeaders == 0) {
                record(IntentHeadersResult.FIRST_PARTY_NO_HEADERS);
            } else if (mUnsafeHeaders == 0) {
                record(IntentHeadersResult.FIRST_PARTY_ONLY_SAFE_HEADERS);
            } else {
                record(IntentHeadersResult.FIRST_PARTY_UNSAFE_HEADERS);
            }
        } else {
            if (mSafeHeaders == 0 && mUnsafeHeaders == 0) {
                record(IntentHeadersResult.THIRD_PARTY_NO_HEADERS);
            } else if (mUnsafeHeaders == 0) {
                record(IntentHeadersResult.THIRD_PARTY_ONLY_SAFE_HEADERS);
            } else {
                record(IntentHeadersResult.THIRD_PARTY_UNSAFE_HEADERS);
            }
        }
    }

    private static void record(@IntentHeadersResult int result) {
        RecordHistogram.recordEnumeratedHistogram("Android.IntentHeaders", result,
                IntentHeadersResult.NUM_ENTRIES);
    }
}
