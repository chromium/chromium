// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import android.content.Context;
import android.os.IBinder;
import android.os.ResultReceiver;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;

import org.chromium.base.Callback;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.InputMethodManagerWrapper;
import org.chromium.ui.base.WindowAndroid;

/**
 * A wrapper around the default IMMWrapper. Intercepts {@link #showSoftInput()} to
 * trigger PCCT height change when the soft keyboard appears.
 */
public class PartialCustomTabInputMethodWrapper implements InputMethodManagerWrapper {
    private InputMethodManagerWrapper mWrapper;
    private Callback<Runnable> mShowSoftKeyInputCallback;

    public PartialCustomTabInputMethodWrapper(
            Context context, WindowAndroid window, Callback<Runnable> softKeyInputCallback) {
        mWrapper = ImeAdapter.createDefaultInputMethodManagerWrapper(context, window, null);
        mShowSoftKeyInputCallback = softKeyInputCallback;
    }

    @Override
    public void restartInput(View view) {
        mWrapper.restartInput(view);
    }

    @Override
    public void showSoftInput(View view, int flags, ResultReceiver resultReceiver) {
        mShowSoftKeyInputCallback.onResult(
                () -> mWrapper.showSoftInput(view, flags, resultReceiver));
    }

    @Override
    public boolean isActive(View view) {
        return mWrapper.isActive(view);
    }

    @Override
    public boolean hideSoftInputFromWindow(
            IBinder windowToken, int flags, ResultReceiver resultReceiver) {
        return mWrapper.hideSoftInputFromWindow(windowToken, flags, resultReceiver);
    }

    @Override
    public void updateSelection(
            View view, int selStart, int selEnd, int candidatesStart, int candidatesEnd) {
        mWrapper.updateSelection(view, selStart, selEnd, candidatesStart, candidatesEnd);
    }

    @Override
    public void updateCursorAnchorInfo(View view, CursorAnchorInfo cursorAnchorInfo) {
        mWrapper.updateCursorAnchorInfo(view, cursorAnchorInfo);
    }

    @Override
    public void updateExtractedText(
            View view, int token, android.view.inputmethod.ExtractedText text) {
        mWrapper.updateExtractedText(view, token, text);
    }

    @Override
    public void onWindowAndroidChanged(WindowAndroid newWindowAndroid) {
        mWrapper.onWindowAndroidChanged(newWindowAndroid);
    }

    @Override
    public void onInputConnectionCreated() {
        mWrapper.onInputConnectionCreated();
    }
}
