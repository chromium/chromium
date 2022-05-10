// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.view.inputmethod.InputConnection;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.vr.keyboard.TextEditAction;
import org.chromium.content_public.browser.WebContents;

/**
 * Helper class for interfacing with the active {@link InputConnection}.
 */
@JNINamespace("vr")
public class VrInputConnection {
    private final long mNativeVrInputConnection;

    @CalledByNative
    private static VrInputConnection create(long nativeVrInputConnection, WebContents contents) {
        return new VrInputConnection(nativeVrInputConnection, contents);
    }

    private VrInputConnection(long nativeVrInputConnection, WebContents contents) {
        mNativeVrInputConnection = nativeVrInputConnection;
    }

    @SuppressWarnings("NewApi")
    @CalledByNative
    public void requestTextState() {
    }

    @SuppressWarnings("NewApi")
    @CalledByNative
    public void onKeyboardEdit(TextEditAction[] edits) {
    }

    @SuppressWarnings("NewApi")
    @CalledByNative
    public void submitInput() {
    }

    @NativeMethods
    interface Natives {
        void updateTextState(long nativeVrInputConnection, VrInputConnection caller, String text);
    }
}
