// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.os.Bundle;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.google_apis.gaia.GaiaId;

/**
 * Helper to serialize/deserialize (@link FullscreenSigninAndHistorySyncConfig} and {@link
 * BottomSheetSigninAndHistorySyncConfig} objects using Bundle, to bypass crash due to the use of
 * Parcelable in activity extras (See https://crbug.com/172602571)
 *
 * <p>TODO(crbug.com/401195865): Remove this class once https://crbug.com/172602571 is fixed.
 */
@NullMarked
final class SigninAndHistorySyncBundleHelper {
    private SigninAndHistorySyncBundleHelper() {}

    // Fields of {@link FullscreenSigninConfig}.
    private static final String SIGNIN_CONFIG_TITLE = "Signin.SigninConfig.Title";
    private static final String SIGNIN_CONFIG_SUBTITLE = "Signin.SigninConfig.Subtitle";
    private static final String SIGNIN_CONFIG_DISMISS_TEXT = "Signin.SigninConfig.DismissText";
    private static final String SIGNIN_CONFIG_LOGO_ID = "Signin.SigninConfig.LogoId";
    private static final String SIGNIN_CONFIG_DISABLE_SIGNIN = "Signin.SigninConfig.DisableSignin";

    // Fields of {@link HistorySyncConfig}.
    private static final String HISTORY_SYNC_CONFIG_TITLE = "Signin.HistorySyncConfig.Title";
    private static final String HISTORY_SYNC_CONFIG_SUBTITLE = "Signin.HistorySyncConfig.Subtitle";

    // Fields common to {@link FullscreenSigninAndHistorySyncConfig} and
    // {@link BottomSheetSigninAndHistorySyncConfig}.
    private static final String HISTORY_OPT_IN_MODE = "Signin.HistoryOptInMode";

    // Fields of {@link AccountPickerBottomSheetStrings}
    private static final String BOTTOM_SHEET_STRINGS_TITLE = "Signin.BottomSheetStrings.Title";
    private static final String BOTTOM_SHEET_STRINGS_SUBTITLE =
            "Signin.BottomSheetStrings.Subtitle";
    private static final String BOTTOM_SHEET_STRINGS_DISMISS_BUTTON =
            "Signin.BottomSheetStrings.DismissButton";

    // Fields of {@link BottomSheetSigninAndHistorySyncConfig.
    private static final String BOTTOM_SHEET_NO_ACCOUNT_SIGNIN_MODE =
            "Signin.BottomSheetNoAccountSigninMode";
    private static final String BOTTOM_SHEET_WITH_ACCOUNT_SIGNIN_MODE =
            "Signin.BottomSheetWithAccountSigninMode";
    private static final String BOTTOM_SHEET_SELECTED_ACCOUNT_ID =
            "Signin.BottomSheetSelectedAccountId";
    private static final String BOTTOM_SHEET_SHOW_SIGNIN_SNACKBAR =
            "Signin.BottomSheetShouldShowSigninSnackbar";

    static Bundle getBundle(FullscreenSigninAndHistorySyncConfig config) {
        Bundle bundle = new Bundle();
        bundle.putString(SIGNIN_CONFIG_TITLE, config.signinConfig.title);
        bundle.putString(SIGNIN_CONFIG_SUBTITLE, config.signinConfig.subtitle);
        bundle.putString(SIGNIN_CONFIG_DISMISS_TEXT, config.signinConfig.dismissText);
        bundle.putInt(SIGNIN_CONFIG_LOGO_ID, config.signinConfig.logoId);
        bundle.putBoolean(SIGNIN_CONFIG_DISABLE_SIGNIN, config.signinConfig.shouldDisableSignin);
        bundle.putString(HISTORY_SYNC_CONFIG_TITLE, config.historySyncConfig.title);
        bundle.putString(HISTORY_SYNC_CONFIG_SUBTITLE, config.historySyncConfig.subtitle);
        bundle.putInt(HISTORY_OPT_IN_MODE, config.historyOptInMode);
        return bundle;
    }

    static FullscreenSigninAndHistorySyncConfig getFullscreenConfig(Bundle bundle) {

        FullscreenSigninAndHistorySyncConfig.Builder builder =
                new FullscreenSigninAndHistorySyncConfig.Builder(
                        bundle.getString(SIGNIN_CONFIG_TITLE, ""),
                        bundle.getString(SIGNIN_CONFIG_SUBTITLE, ""),
                        bundle.getString(SIGNIN_CONFIG_DISMISS_TEXT, ""),
                        bundle.getString(HISTORY_SYNC_CONFIG_TITLE, ""),
                        bundle.getString(HISTORY_SYNC_CONFIG_SUBTITLE, ""));
        builder.signinLogoId(bundle.getInt(SIGNIN_CONFIG_LOGO_ID, 0));
        builder.shouldDisableSignin(bundle.getBoolean(SIGNIN_CONFIG_DISABLE_SIGNIN, false));
        builder.historyOptInMode(bundle.getInt(HISTORY_OPT_IN_MODE, 0));
        return builder.build();
    }

    static Bundle getBundle(BottomSheetSigninAndHistorySyncConfig config) {
        Bundle bundle = new Bundle();
        bundle.putString(BOTTOM_SHEET_STRINGS_TITLE, config.bottomSheetStrings.titleString);
        bundle.putString(BOTTOM_SHEET_STRINGS_SUBTITLE, config.bottomSheetStrings.subtitleString);
        bundle.putString(
                BOTTOM_SHEET_STRINGS_DISMISS_BUTTON, config.bottomSheetStrings.dismissButtonString);

        bundle.putString(HISTORY_SYNC_CONFIG_TITLE, config.historySyncConfig.title);
        bundle.putString(HISTORY_SYNC_CONFIG_SUBTITLE, config.historySyncConfig.subtitle);

        bundle.putInt(HISTORY_OPT_IN_MODE, config.historyOptInMode);
        bundle.putInt(BOTTOM_SHEET_NO_ACCOUNT_SIGNIN_MODE, config.noAccountSigninMode);
        bundle.putInt(BOTTOM_SHEET_WITH_ACCOUNT_SIGNIN_MODE, config.withAccountSigninMode);
        bundle.putString(
                BOTTOM_SHEET_SELECTED_ACCOUNT_ID,
                config.selectedCoreAccountId == null
                        ? null
                        : config.selectedCoreAccountId.getId().toString());
        bundle.putBoolean(BOTTOM_SHEET_SHOW_SIGNIN_SNACKBAR, config.shouldShowSigninSnackbar);
        return bundle;
    }

    static BottomSheetSigninAndHistorySyncConfig getBottomSheetConfig(Bundle bundle) {
        BottomSheetSigninAndHistorySyncConfig.Builder builder =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                        new AccountPickerBottomSheetStrings.Builder(
                                        bundle.getString(BOTTOM_SHEET_STRINGS_TITLE, ""))
                                .setSubtitleString(bundle.getString(BOTTOM_SHEET_STRINGS_SUBTITLE))
                                .setDismissButtonString(
                                        bundle.getString(BOTTOM_SHEET_STRINGS_DISMISS_BUTTON))
                                .build(),
                        bundle.getInt(BOTTOM_SHEET_NO_ACCOUNT_SIGNIN_MODE, 0),
                        bundle.getInt(BOTTOM_SHEET_WITH_ACCOUNT_SIGNIN_MODE, 0),
                        bundle.getInt(HISTORY_OPT_IN_MODE, 0),
                        bundle.getString(HISTORY_SYNC_CONFIG_TITLE, ""),
                        bundle.getString(HISTORY_SYNC_CONFIG_SUBTITLE, ""));
        String selectedAccountId = bundle.getString(BOTTOM_SHEET_SELECTED_ACCOUNT_ID);
        if (selectedAccountId != null) {
            builder.selectedCoreAccountId(new CoreAccountId(new GaiaId(selectedAccountId)));
        }
        builder.shouldShowSigninSnackbar(bundle.getBoolean(BOTTOM_SHEET_SHOW_SIGNIN_SNACKBAR));
        return builder.build();
    }
}
