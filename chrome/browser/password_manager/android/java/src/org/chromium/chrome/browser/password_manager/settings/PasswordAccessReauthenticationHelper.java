// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.content.Context;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.device_reauth.ReauthResult;
import org.chromium.chrome.browser.password_manager.R;
import org.chromium.chrome.browser.password_manager.settings.ReauthenticationManager.ReauthScope;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A helper to perform a user's reauthentication for a specific {@link ReauthReason}. Only a single
 * reauthentication can happen at a given time.
 */
public class PasswordAccessReauthenticationHelper {
    public static final String SETTINGS_REAUTHENTICATION_HISTOGRAM =
            "PasswordManager.ReauthToAccessPasswordInSettings";

    /**
     * The reason for the reauthentication.
     *
     * <p>TODO(crbug.com/40170183): Remove the edit reason once the password check credential editor
     * is completely replaced with the new one.
     */
    @IntDef({ReauthReason.VIEW_PASSWORD, ReauthReason.EDIT_PASSWORD, ReauthReason.COPY_PASSWORD})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ReauthReason {
        /** A reauthentication is required for viewing a password. */
        int VIEW_PASSWORD = 0;

        /** A reauthentication is required for editing a password. */
        int EDIT_PASSWORD = 1;

        /** Reauthentication is required in order to copy a password. */
        int COPY_PASSWORD = 2;
    }

    private final Context mContext;
    private final FragmentManager mFragmentManager;
    private Callback<Boolean> mCallback;

    public PasswordAccessReauthenticationHelper(Context context, FragmentManager fragmentManager) {
        mContext = context;
        mFragmentManager = fragmentManager;
    }

    public boolean canReauthenticate() {
        return ReauthenticationManager.isScreenLockSetUp(mContext);
    }

    /**
     * Asks the user to reauthenticate. Requires {@link #canReauthenticate()}.
     * @param reason The {@link ReauthReason} for the reauth.
     * @param callback A {@link Callback}. Will invoke {@link Callback#onResult} with whether the
     *         user passed or dismissed the reauth screen.
     */
    public void reauthenticate(@ReauthReason int reason, Callback<Boolean> callback) {
        assert canReauthenticate();
        assert mCallback == null;

        // Invoke the handler immediately if an authentication is still valid.
        if (ReauthenticationManager.authenticationStillValid(ReauthScope.ONE_AT_A_TIME)) {
            RecordHistogram.recordEnumeratedHistogram(
                    SETTINGS_REAUTHENTICATION_HISTOGRAM,
                    ReauthResult.SKIPPED,
                    ReauthResult.MAX_VALUE + 1);

            callback.onResult(true);
            return;
        }

        mCallback = callback;

        switch (reason) {
            case ReauthReason.VIEW_PASSWORD:
                ReauthenticationManager.displayReauthenticationFragment(
                        R.string.lockscreen_description_view,
                        View.NO_ID,
                        mFragmentManager,
                        ReauthScope.ONE_AT_A_TIME);
                break;
            case ReauthReason.EDIT_PASSWORD:
                ReauthenticationManager.displayReauthenticationFragment(
                        R.string.lockscreen_description_edit,
                        View.NO_ID,
                        mFragmentManager,
                        ReauthScope.ONE_AT_A_TIME);
                break;
            case ReauthReason.COPY_PASSWORD:
                ReauthenticationManager.displayReauthenticationFragment(
                        R.string.lockscreen_description_copy,
                        View.NO_ID,
                        mFragmentManager,
                        ReauthScope.ONE_AT_A_TIME);
                break;
        }
    }

    /**
     * Shows a toast to the user nudging them to set up a screen lock. Intended to be called in case
     * {@link #canReauthenticate()} returns false.
     */
    public void showScreenLockToast(@ReauthReason int reason) {
        if (reason == ReauthReason.COPY_PASSWORD) {
            Toast.makeText(
                            mContext,
                            R.string.password_entry_copy_set_screen_lock,
                            Toast.LENGTH_LONG)
                    .show();
            return;
        }
        Toast.makeText(mContext, R.string.password_entry_view_set_screen_lock, Toast.LENGTH_LONG)
                .show();
    }

    /**
     * Invoked when a reauthentication might have happened. Invokes {@link Callback#onResult}
     * with whether the user passed the reauthentication challenge.
     * No-op if {@link #mCallback} is null.
     */
    public void onReauthenticationMaybeHappened() {
        if (mCallback != null) {
            mCallback.onResult(
                    ReauthenticationManager.authenticationStillValid(ReauthScope.ONE_AT_A_TIME));
            mCallback = null;
        }
    }
}
