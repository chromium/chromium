// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckController.PasswordCheckResult;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@NullMarked
class PasswordsCheckPreferenceProperties {
    /** State of the passwords check, one of the {@link PasswordsState} values. */
    static final WritableIntPropertyKey PASSWORDS_STATE = new WritableIntPropertyKey();

    /** Number of compromised passwords; only used when PASSWORDS_STATE is COMPROMISED_EXIST. */
    static final WritableIntPropertyKey COMPROMISED_PASSWORDS_COUNT = new WritableIntPropertyKey();

    /** Listener for the passwords element click events. */
    static final WritableObjectPropertyKey PASSWORDS_CLICK_LISTENER =
            new WritableObjectPropertyKey();

    /** The title for the password check preference. */
    static final ReadableObjectPropertyKey<String> PASSWORDS_TITLE =
            new ReadableObjectPropertyKey<>();

    @IntDef({
        PasswordsState.UNCHECKED,
        PasswordsState.CHECKING,
        PasswordsState.SAFE,
        PasswordsState.COMPROMISED_EXIST,
        PasswordsState.NO_PASSWORDS,
        PasswordsState.ERROR,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PasswordsState {
        int UNCHECKED = 0;
        int CHECKING = 1;
        int SAFE = 2;
        int COMPROMISED_EXIST = 3;
        int NO_PASSWORDS = 4;
        int ERROR = 5;
    }

    static @PasswordsState int passwordsStateFromPasswordCheckResult(
            PasswordCheckResult passwordSafetyCheckResult) {
        if (passwordSafetyCheckResult.getError() != null) {
            return PasswordsState.ERROR;
        }

        if (passwordSafetyCheckResult.getBreachedCount() != null
                && passwordSafetyCheckResult.getBreachedCount() > 0) {
            // If there are some compromised credentials, then password state should be
            // COMPROMISED_EXIST regardless whether loading passwords is finished.
            return PasswordsState.COMPROMISED_EXIST;
        }
        if (passwordSafetyCheckResult.getTotalPasswordsCount() != null
                && passwordSafetyCheckResult.getTotalPasswordsCount() == 0) {
            return PasswordsState.NO_PASSWORDS;
        }
        if (passwordSafetyCheckResult.getTotalPasswordsCount() == null
                || passwordSafetyCheckResult.getBreachedCount() == null) {
            // If passwords loading or password check has not yet finished, then password state is
            // checking because there can be either no passwords at all or no breached credentials.
            assert false
                    : "Non-valid password check result, both total passwords count and breached"
                            + " count should be present in the result.";
            return PasswordsState.CHECKING;
        }
        // If non of the above checks were hit, that means both the passwords loading is
        // finished and 0 breached credentials are obtained.
        return PasswordsState.SAFE;
    }

    static @PasswordsStatus int passwordsStateToNative(@PasswordsState int state) {
        switch (state) {
            case PasswordsState.UNCHECKED:
                // This is not used.
                assert false : "PasswordsState.UNCHECKED has no native equivalent.";
                return PasswordsStatus.ERROR;
            case PasswordsState.CHECKING:
                return PasswordsStatus.CHECKING;
            case PasswordsState.SAFE:
                return PasswordsStatus.SAFE;
            case PasswordsState.COMPROMISED_EXIST:
                return PasswordsStatus.COMPROMISED_EXIST;
            case PasswordsState.NO_PASSWORDS:
                return PasswordsStatus.NO_PASSWORDS;
            case PasswordsState.ERROR:
                return PasswordsStatus.ERROR;
            default:
                assert false : "Unknown PasswordsState value.";
                return PasswordsStatus.ERROR;
        }
    }

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                PASSWORDS_STATE,
                COMPROMISED_PASSWORDS_COUNT,
                PASSWORDS_CLICK_LISTENER,
                PASSWORDS_TITLE
            };

    static PropertyModel createPasswordSafetyCheckModel(String title) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(PASSWORDS_STATE, PasswordsState.UNCHECKED)
                .with(COMPROMISED_PASSWORDS_COUNT, 0)
                .with(PASSWORDS_TITLE, title)
                .build();
    }
}
