// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.os.Handler;
import android.view.KeyEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.vr.keyboard.TextEditAction;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.WebContents;

/**
 * Helper class for interfacing with the active {@link InputConnection}.
 */
@JNINamespace("vr")
public class VrInputConnection {
    private static final String TAG = "VrIC";
    private static final boolean DEBUG_LOGS = false;
    private static final int CHARS_AROUND_CURSOR = 100;

    private final long mNativeVrInputConnection;
    private ImeAdapter mImeAdapter;
    private Handler mImeThreadResponseHandler;

    @CalledByNative
    private static VrInputConnection create(long nativeVrInputConnection, WebContents contents) {
        return new VrInputConnection(nativeVrInputConnection, contents);
    }

    private VrInputConnection(long nativeVrInputConnection, WebContents contents) {
        mNativeVrInputConnection = nativeVrInputConnection;
        mImeAdapter = ImeAdapter.fromWebContents(contents);
    }

    @SuppressWarnings("NewApi")
    @CalledByNative
    public void requestTextState() {
        if (DEBUG_LOGS) Log.i(TAG, "requestTextState");
        InputConnection ic = mImeAdapter.getActiveInputConnection();
        if (ic == null) return;
        if (mImeThreadResponseHandler == null) {
            mImeThreadResponseHandler = new Handler();
        }
        ic.getHandler().post(new Runnable() {
            @Override
            public void run() {
                ic.beginBatchEdit();
                CharSequence before = ic.getTextBeforeCursor(CHARS_AROUND_CURSOR, 0);
                CharSequence selected = ic.getSelectedText(0);
                CharSequence after = ic.getTextAfterCursor(CHARS_AROUND_CURSOR, 0);
                final String textState = (before != null ? before.toString() : "")
                        + (selected != null ? selected.toString() : "")
                        + (after != null ? after.toString() : "");
                ic.endBatchEdit();
                if (DEBUG_LOGS) Log.i(TAG, "text state" + textState);
                // The text state is obtained on the IME thread and the response is sent back to the
                // thread that created this connection.
                mImeThreadResponseHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        VrInputConnectionJni.get().updateTextState(
                                mNativeVrInputConnection, VrInputConnection.this, textState);
                    }
                });
            }
        });
    }

    @SuppressWarnings("NewApi")
    @CalledByNative
    public void onKeyboardEdit(TextEditAction[] edits) {
        if (edits.length == 0) return;
        if (DEBUG_LOGS) Log.i(TAG, "onKeyboardEdit [%d]", edits.length);
        InputConnection ic = mImeAdapter.getActiveInputConnection();
        assert ic != null;
        ic.getHandler().post(new Runnable() {
            @Override
            public void run() {
                ic.beginBatchEdit();
                for (TextEditAction edit : edits) {
                    if (DEBUG_LOGS) Log.i(TAG, "processing edit: %s", edit.toString());
                    switch (edit.mType) {
                        case TextEditActionType.COMMIT_TEXT:
                            ic.commitText(edit.mText, edit.mNewCursorPosition);
                            break;
                        case TextEditActionType.DELETE_TEXT:
                            // We only have delete actions for backspace. We cannot use
                            // deleteSurroundingText, because it is not treated as a user action.
                            // Instead, we submit the raw key event.
                            assert edit.mNewCursorPosition == -1;
                            ic.sendKeyEvent(
                                    new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
                            ic.sendKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL));
                            break;
                        case TextEditActionType.SET_COMPOSING_TEXT:
                            ic.setComposingText(edit.mText, edit.mNewCursorPosition);
                            break;
                        case TextEditActionType.CLEAR_COMPOSING_TEXT:
                            ic.setComposingText("", 1);
                            break;
                        default:
                            assert false;
                    }
                }
                ic.endBatchEdit();
            }
        });
    }

    @SuppressWarnings("NewApi")
    @CalledByNative
    public void submitInput() {
        if (DEBUG_LOGS) Log.i(TAG, "submitInput");
        InputConnection ic = mImeAdapter.getActiveInputConnection();
        assert ic != null;
        ic.getHandler().post(new Runnable() {
            @Override
            public void run() {
                ic.performEditorAction(EditorInfo.IME_ACTION_GO);
            }
        });
    }

    @NativeMethods
    interface Natives {
        void updateTextState(long nativeVrInputConnection, VrInputConnection caller, String text);
    }
}
