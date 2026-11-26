// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc.settings;

import android.content.Context;
import android.content.DialogInterface;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ttc.R;

/**
 * A dialog that records a TTC keyboard shortcut from a physical keyboard. Saving is only possible
 * once a valid shortcut has been pressed that neither Chrome nor TTC already uses.
 */
@NullMarked
public class TtcShortcutRecorderDialog implements DialogInterface.OnKeyListener {
    private final AlertDialog mDialog;
    private final LinearLayout mKeycaps;
    private final TextView mMessage;
    private final String mUnavailableShortcut;
    private @Nullable String mRecordedShortcut;

    /**
     * @param context The context to show the dialog in.
     * @param title The title of the dialog: the name of the shortcut being recorded.
     * @param unavailableShortcut The pref value of TTC's other shortcut, which can't be recorded.
     * @param onSave Called with the pref value of the recorded shortcut when the user saves it.
     * @param onDismiss Called when the dialog is dismissed, however that happens.
     */
    public TtcShortcutRecorderDialog(
            Context context,
            CharSequence title,
            String unavailableShortcut,
            Callback<String> onSave,
            Runnable onDismiss) {
        // Compared with what a key press records, which names the shortcut the way it's saved.
        mUnavailableShortcut = TtcShortcutUtils.canonicalize(unavailableShortcut);
        View view =
                LayoutInflater.from(context).inflate(R.layout.ttc_shortcut_recorder_dialog, null);
        mKeycaps = view.findViewById(R.id.ttc_shortcut_recorder_keycaps);
        mMessage = view.findViewById(R.id.ttc_shortcut_recorder_message);
        mDialog =
                new AlertDialog.Builder(context, R.style.ThemeOverlay_BrowserUI_AlertDialog)
                        .setTitle(title)
                        .setView(view)
                        .setPositiveButton(
                                R.string.save,
                                (dialog, which) -> {
                                    if (mRecordedShortcut != null) {
                                        onSave.onResult(mRecordedShortcut);
                                    }
                                })
                        .setNegativeButton(R.string.cancel, null)
                        .setOnKeyListener(this)
                        .setOnDismissListener(dialog -> onDismiss.run())
                        .create();
    }

    /** Shows the dialog. */
    public void show() {
        mDialog.show();
        update(/* shownShortcut= */ "", getString(R.string.ttc_settings_shortcut_dialog_message));
    }

    /** Dismisses the dialog, without saving. */
    public void dismiss() {
        mDialog.dismiss();
    }

    @Override
    public boolean onKey(DialogInterface dialog, int keyCode, KeyEvent event) {
        // Leave the keys that close the dialog to it, and those that move around in it or press its
        // buttons, unless modifiers make them a shortcut to record, valid or not. Shift+Tab moves
        // focus back.
        if (keyCode == KeyEvent.KEYCODE_BACK || keyCode == KeyEvent.KEYCODE_ESCAPE) return false;
        if (isNavigationKey(keyCode)
                && (event.hasNoModifiers()
                        || (keyCode == KeyEvent.KEYCODE_TAB
                                && event.hasModifiers(KeyEvent.META_SHIFT_ON)))) {
            return false;
        }
        // A held key repeats, but records the same shortcut.
        if (event.getAction() != KeyEvent.ACTION_DOWN
                || event.getRepeatCount() != 0
                || KeyEvent.isModifierKey(keyCode)) {
            return true;
        }

        String shortcut = TtcShortcutUtils.fromKeyEvent(event);
        @TtcShortcutStatus int status = TtcShortcutUtils.getStatus(shortcut);
        mRecordedShortcut = null;
        if (status == TtcShortcutStatus.INVALID) {
            update(
                    /* shownShortcut= */ "",
                    getString(R.string.ttc_settings_shortcut_dialog_invalid));
        } else if (status == TtcShortcutStatus.CONFLICTS_WITH_BROWSER_SHORTCUT) {
            update(shortcut, getString(R.string.ttc_settings_shortcut_dialog_browser_shortcut));
        } else if (shortcut.equals(mUnavailableShortcut)) {
            update(shortcut, getString(R.string.ttc_settings_shortcut_dialog_in_use));
        } else {
            mRecordedShortcut = shortcut;
            // The message is a live region, so this tells screen reader users which shortcut was
            // recorded, and that they can save it.
            String keys = String.join("+", TtcShortcutUtils.getKeyLabels(shortcut));
            update(
                    shortcut,
                    mMessage.getContext()
                            .getString(R.string.ttc_settings_shortcut_dialog_recorded, keys));
        }
        return true;
    }

    private static boolean isNavigationKey(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_ENTER:
            case KeyEvent.KEYCODE_NUMPAD_ENTER:
            case KeyEvent.KEYCODE_TAB:
            case KeyEvent.KEYCODE_DPAD_UP:
            case KeyEvent.KEYCODE_DPAD_DOWN:
            case KeyEvent.KEYCODE_DPAD_LEFT:
            case KeyEvent.KEYCODE_DPAD_RIGHT:
            case KeyEvent.KEYCODE_DPAD_CENTER:
            case KeyEvent.KEYCODE_SPACE:
                return true;
            default:
                return false;
        }
    }

    private String getString(@StringRes int id) {
        return mMessage.getContext().getString(id);
    }

    private void update(String shownShortcut, String message) {
        TtcShortcutPreference.bindKeycaps(mKeycaps, shownShortcut);
        mMessage.setText(message);
        mDialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(mRecordedShortcut != null);
    }

    AlertDialog getDialogForTesting() {
        return mDialog;
    }

    @Nullable String getRecordedShortcutForTesting() {
        return mRecordedShortcut;
    }
}
