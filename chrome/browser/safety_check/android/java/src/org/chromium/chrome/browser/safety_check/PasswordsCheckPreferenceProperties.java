// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.password_check.PasswordCheckUIStatus;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper.PasswordCheckBackendException;
import org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckController.PasswordCheckResult;
import org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckNativeException;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

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
        PasswordsState.OFFLINE,
        PasswordsState.NO_PASSWORDS,
        PasswordsState.SIGNED_OUT,
        PasswordsState.QUOTA_LIMIT,
        PasswordsState.ERROR,
        PasswordsState.BACKEND_VERSION_NOT_SUPPORTED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PasswordsState {
        int UNCHECKED = 0;
        int CHECKING = 1;
        int SAFE = 2;
        int COMPROMISED_EXIST = 3;
        int OFFLINE = 4;
        int NO_PASSWORDS = 5;
        int SIGNED_OUT = 6;
        int QUOTA_LIMIT = 7;
        int ERROR = 8;
        int BACKEND_VERSION_NOT_SUPPORTED = 9;
    }

    static @PasswordsState int passwordsStatefromErrorState(@PasswordCheckUIStatus int state) {
        switch (state) {
            case PasswordCheckUIStatus.ERROR_OFFLINE:
                return PasswordsState.OFFLINE;
            case PasswordCheckUIStatus.ERROR_NO_PASSWORDS:
                return PasswordsState.NO_PASSWORDS;
            case PasswordCheckUIStatus.ERROR_SIGNED_OUT:
                return PasswordsState.SIGNED_OUT;
            case PasswordCheckUIStatus.ERROR_QUOTA_LIMIT:
            case PasswordCheckUIStatus.ERROR_QUOTA_LIMIT_ACCOUNT_CHECK:
                return PasswordsState.QUOTA_LIMIT;
            case PasswordCheckUIStatus.CANCELED:
            case PasswordCheckUIStatus.ERROR_UNKNOWN:
                return PasswordsState.ERROR;
            default:
                assert false : "Unknown PasswordCheckUIStatus value.";
        }
        // Never reached.
        return PasswordsState.UNCHECKED;
    }

    static @PasswordsState int passwordsStateFromPasswordCheckResult(
            PasswordCheckResult passwordSafetyCheckResult) {
        if (passwordSafetyCheckResult.getError() != null) {
            Exception error = passwordSafetyCheckResult.getError();
            if (error instanceof PasswordCheckBackendException
                    && ((PasswordCheckBackendException) error).errorCode
                            == CredentialManagerError.BACKEND_VERSION_NOT_SUPPORTED) {
                return PasswordsState.BACKEND_VERSION_NOT_SUPPORTED;
            }
            if (error instanceof PasswordCheckNativeException) {
                return passwordsStatefromErrorState(
                        ((PasswordCheckNativeException) error).errorCode);
            }
            return PasswordsState.ERROR;
        }

        if (passwordSafetyCheckResult.getBreachedCount().isPresent()
                && passwordSafetyCheckResult.getBreachedCount().getAsInt() > 0) {
            // If there are some compromised credentials, then password state should be
            // COMPROMISED_EXIST regardless whether loading passwords is finished.
            return PasswordsState.COMPROMISED_EXIST;
        }
        if (passwordSafetyCheckResult.getTotalPasswordsCount().isPresent()
                && passwordSafetyCheckResult.getTotalPasswordsCount().getAsInt() == 0) {
            return PasswordsState.NO_PASSWORDS;
        }
        if (!passwordSafetyCheckResult.getTotalPasswordsCount().isPresent()
                || !passwordSafetyCheckResult.getBreachedCount().isPresent()) {
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
            case PasswordsState.BACKEND_VERSION_NOT_SUPPORTED:
                // PasswordsState.BACKEND_VERSION_NOT_SUPPORTED has no native equivalent, so
                // converting it to just error.
                return PasswordsStatus.ERROR;
            case PasswordsState.CHECKING:
                return PasswordsStatus.CHECKING;
            case PasswordsState.SAFE:
                return PasswordsStatus.SAFE;
            case PasswordsState.COMPROMISED_EXIST:
                return PasswordsStatus.COMPROMISED_EXIST;
            case PasswordsState.OFFLINE:
                return PasswordsStatus.OFFLINE;
            case PasswordsState.NO_PASSWORDS:
                return PasswordsStatus.NO_PASSWORDS;
            case PasswordsState.SIGNED_OUT:
                return PasswordsStatus.SIGNED_OUT;
            case PasswordsState.QUOTA_LIMIT:
                return PasswordsStatus.QUOTA_LIMIT;
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
