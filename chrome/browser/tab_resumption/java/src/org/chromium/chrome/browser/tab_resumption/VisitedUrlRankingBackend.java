// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/** A SuggestionBackend backed by the native fetch and rank services. */
@JNINamespace("tab_resumption::jni")
public class VisitedUrlRankingBackend implements SuggestionBackend {
    private long mNativeVisitedUrlRankingBackend;

    VisitedUrlRankingBackend(Profile profile) {
        mNativeVisitedUrlRankingBackend = VisitedUrlRankingBackendJni.get().init(profile);
    }

    /** Implements {@link SuggestionBackend} */
    @Override
    public void destroy() {
        VisitedUrlRankingBackendJni.get().destroy(mNativeVisitedUrlRankingBackend);
        mNativeVisitedUrlRankingBackend = 0;
    }

    /** Implements {@link SuggestionBackend} */
    @Override
    public void triggerUpdate() {
        // TODO(crbug.com/333579087): Implement.
    }

    /** Implements {@link SuggestionBackend} */
    @Override
    public void setUpdateObserver(Runnable updateObserver) {
        // TODO(crbug.com/333579087): Implement.
    }

    /** Implements {@link SuggestionBackend} */
    @Override
    public void readCached(Callback<List<SuggestionEntry>> callback) {
        List<SuggestionEntry> suggestions = new ArrayList<SuggestionEntry>();

        // TODO(crbug.com/333579087): Replace stub with implementation.
        callback.onResult(suggestions);
    }

    @NativeMethods
    interface Natives {
        long init(@JniType("Profile*") Profile profile);

        void destroy(long nativeVisitedUrlRankingBackend);
    }
}
