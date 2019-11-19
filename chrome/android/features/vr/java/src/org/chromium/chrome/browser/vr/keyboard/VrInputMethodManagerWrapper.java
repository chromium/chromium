// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.keyboard;

import android.content.Context;
import android.os.IBinder;
import android.os.ResultReceiver;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.content_public.browser.InputMethodManagerWrapper;

/**
 * A fake wrapper around Android's InputMethodManager that doesn't really talk to the
 * InputMethodManager and instead talks to the Daydream keyboard.
 */
public class VrInputMethodManagerWrapper implements InputMethodManagerWrapper {
    private static final String TAG = "VrIme";
    private static final boolean DEBUG_LOGS = false;

    private final Context mContext;
    private View mView;
    private BrowserKeyboardInterface mKeyboard;

    /**
     * The interface used by the browser to talk to the the Daydream keyboard.
     */
    public interface BrowserKeyboardInterface {
        /**
         * Show or hide the virtual keyboard.
         */
        void showSoftInput(boolean show);

        /**
         * Update the selection indices.
         */
        void updateIndices(
                int selectionStart, int selectionEnd, int compositionStart, int compositionEnd);
    }

    public VrInputMethodManagerWrapper(Context context, BrowserKeyboardInterface keyboard) {
        mContext = context;
        mKeyboard = keyboard;
    }

    @VisibleForTesting
    public void setBrowserKeyboardInterfaceForTesting(BrowserKeyboardInterface keyboard) {
        mKeyboard = keyboard;
    }

    @VisibleForTesting
    public BrowserKeyboardInterface getBrowserKeyboardInterfaceForTesting() {
        return mKeyboard;
    }

    @Override
    public void restartInput(View view) {
        if (DEBUG_LOGS) Log.i(TAG, "restartInput");
        EditorInfo outAttrs = new EditorInfo();
        view.onCreateInputConnection(outAttrs);
    }

    @Override
    public void showSoftInput(View view, int flags, ResultReceiver resultReceiver) {
        if (DEBUG_LOGS) Log.i(TAG, "showSoftInput");
        mView = view;
        EditorInfo outAttrs = new EditorInfo();
        view.onCreateInputConnection(outAttrs);
        mKeyboard.showSoftInput(true);

        // Since the VR keyboard doesn't take content space, we report back to the ImeAdapter that
        // the keyboard was always showing.
        resultReceiver.send(InputMethodManager.RESULT_UNCHANGED_SHOWN, null);
    }

    @Override
    public boolean isActive(View view) {
        return mView != null && mView == view;
    }

    @Override
    public boolean hideSoftInputFromWindow(
            IBinder windowToken, int flags, ResultReceiver resultReceiver) {
        if (DEBUG_LOGS) Log.i(TAG, "hideSoftInputFromWindow");
        mKeyboard.showSoftInput(false);
        mView = null;
        return false;
    }

    @Override
    public void updateSelection(
            View view, int selStart, int selEnd, int candidatesStart, int candidatesEnd) {
        if (DEBUG_LOGS) {
            Log.i(TAG, "updateSelection: SEL [%d, %d], COM [%d, %d]", selStart, selEnd,
                    candidatesStart, candidatesEnd);
        }
        mKeyboard.updateIndices(selStart, selEnd, candidatesStart, candidatesEnd);
    }

    @Override
    public void updateCursorAnchorInfo(View view, CursorAnchorInfo cursorAnchorInfo) {}

    @Override
    public void updateExtractedText(
            View view, int token, android.view.inputmethod.ExtractedText text) {
        if (DEBUG_LOGS) Log.i(TAG, "updateExtractedText: [%s]", text.text.toString());
    }

    @Override
    public void notifyUserAction() {}
}
