// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.autofill.SubKeyRequester;

/** Provides access to SubKeyRequester with the necessary //chrome dependencies. */
@NullMarked
@JNINamespace("autofill")
public class SubKeyRequesterFactory {
    private static @Nullable SubKeyRequester sSubKeyRequesterForTest;

    private SubKeyRequesterFactory() {}

    public static SubKeyRequester getInstance() {
        if (sSubKeyRequesterForTest != null) return sSubKeyRequesterForTest;
        return SubKeyRequesterFactoryJni.get().getInstance();
    }

    public static void setInstanceForTesting(SubKeyRequester requester) {
        sSubKeyRequesterForTest = requester;
        ResettersForTesting.register(() -> sSubKeyRequesterForTest = null);
    }

    @NativeMethods
    interface Natives {
        SubKeyRequester getInstance();
    }
}
