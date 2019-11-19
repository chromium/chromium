// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.keyboard;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.vr.TextEditActionType;

import java.util.Locale;

/**
 * An immutable class that represents an edit made by the keyboard.
 */
@JNINamespace("vr")
public class TextEditAction {
    @TextEditActionType
    public final int mType;
    public final String mText;
    public final int mNewCursorPosition;

    @CalledByNative
    private static TextEditAction[] createArray(int size) {
        return new TextEditAction[size];
    }

    @VisibleForTesting
    @CalledByNative
    public TextEditAction(@TextEditActionType int type, String text, int newCursorPosition) {
        mType = type;
        mText = text;
        mNewCursorPosition = newCursorPosition;
    }

    @Override
    public String toString() {
        return String.format(Locale.US, "TextEditAction {[%d] Text[%s] Cursor[%d]}", mType, mText,
                mNewCursorPosition);
    }
}
