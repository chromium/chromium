// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.net.Uri;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import java.util.List;

/**
 * A thread-safe thin Java facing implementation of aw_contents_origin_matcher.h This API sends all
 * the data and origin matching requests over to the native implementation so that we can have a
 * standard implementation for our origin matching rules.
 *
 * <p>This class owns a native object so it is critical that when you are done using this, it needs
 * to be cleaned up with the {@link AwContentsOriginMatcher#destroy} method.
 */
@JNINamespace("android_webview")
public class AwContentsOriginMatcher {
    private final Object mLock = new Object();

    private long mNative;

    public AwContentsOriginMatcher() {
        mNative = AwContentsOriginMatcherJni.get().init(this);
    }

    /** Returns if any of the rules in the origin matcher currently match the uri provided. */
    public boolean matchesOrigin(Uri origin) {
        return matchesOriginLocked(origin);
    }

    /**
     * Updates the rules to the list provided. If any badly formed rules are provided, the rules
     * will not be updated and the bad rules will be returned.
     */
    public String[] updateRuleList(final List<String> rules) {
        return updateRuleListLocked(rules);
    }

    /**
     * This method _must_ be called when you are finished with the origin matcher.
     *
     * <p>It will clean up any native references of your code.
     */
    public void destroy() {
        destroyLocked();
    }

    private void ensureNativeExists() {
        if (mNative == 0) {
            throw new IllegalStateException(
                    "AwContentsOriginMatcher did not have access to native implementation. "
                            + "Ensure you don't call this method after cleanup.");
        }
    }

    private boolean matchesOriginLocked(Uri origin) {
        synchronized (mLock) {
            ensureNativeExists();
            return AwContentsOriginMatcherJni.get().matchesOrigin(mNative, origin.toString());
        }
    }

    private String[] updateRuleListLocked(final List<String> rules) {
        synchronized (mLock) {
            ensureNativeExists();
            return AwContentsOriginMatcherJni.get()
                    .updateRuleList(mNative, rules.toArray(new String[] {}));
        }
    }

    private void destroyLocked() {
        synchronized (mLock) {
            ensureNativeExists();
            AwContentsOriginMatcherJni.get().destroy(mNative);
            mNative = 0;
        }
    }

    @NativeMethods
    interface Natives {
        long init(AwContentsOriginMatcher caller);

        boolean matchesOrigin(long nativeAwContentsOriginMatcher, String origin);

        // Returns the list of invalid rules.
        // If there are bad rules, no update is performed
        String[] updateRuleList(long nativeAwContentsOriginMatcher, String[] rules);

        void destroy(long nativeAwContentsOriginMatcher);
    }
}
