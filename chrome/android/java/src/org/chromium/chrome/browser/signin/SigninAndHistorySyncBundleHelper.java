// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.os.Bundle;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.base.GaiaId;

/**
 * Helper to serialize/deserialize (@link FullscreenSigninAndHistorySyncConfig} and {@link
 * BottomSheetSigninAndHistorySyncConfig} objects using Bundle, to bypass crash due to the use of
 * Parcelable in activity extras (See https://crbug.com/172602571)
 *
 * <p>TODO(crbug.com/401195865): Remove this class once https://crbug.com/172602571 is fixed.
 */
final class SigninAndHistorySyncBundleHelper {
    private SigninAndHistorySyncBundleHelper() {}

    // Fields of {@link FullscreenSigninConfig}.
    private static final String SIGNIN_CONFIG_TITLE_ID = "Signin.SigninConfig.TitleId";
    private static final String SIGNIN_CONFIG_SUBTITLE_ID = "Signin.SigninConfig.SubtitleId";
    private static final String SIGNIN_CONFIG_DISMISS_TEXT_ID = "Signin.SigninConfig.DismissTextId";
    private static final String SIGNIN_CONFIG_LOGO_ID = "Signin.SigninConfig.LogoId";
    private static final String SIGNIN_CONFIG_DISABLE_SIGNIN = "Signin.SigninConfig.DisableSignin";

    // Fields of {@link HistorySyncConfig}.
    private static final String HISTORY_SYNC_CONFIG_TITLE_ID = "Signin.HistorySyncConfig.TitleId";
    private static final String HISTORY_SYNC_CONFIG_SUBTITLE_ID =
            "Signin.HistorySyncConfig.SubtitleId";

    // Fields common to {@link FullscreenSigninAndHistorySyncConfig} and
    // {@link BottomSheetSigninAndHistorySyncConfig}.
    private static final String HISTORY_OPT_IN_MODE = "Signin.HistoryOptInMode";

    // Fields of {@link AccountPickerBottomSheetStrings}
    private static final String BOTTOM_SHEET_STRINGS_TITLE_ID = "Signin.BottomSheetStrings.TitleId";
    private static final String BOTTOM_SHEET_STRINGS_SUBTITLE_ID =
            "Signin.BottomSheetStrings.SubtitleId";
    private static final String BOTTOM_SHEET_STRINGS_DISMISS_BUTTON_ID =
            "Signin.BottomSheetStrings.DismissButtonId";

    // Fields of {@link BottomSheetSigninAndHistorySyncConfig.
    private static final String BOTTOM_SHEET_NO_ACCOUNT_SIGNIN_MODE =
            "Signin.BottomSheetNoAccountSigninMode";
    private static final String BOTTOM_SHEET_WITH_ACCOUNT_SIGNIN_MODE =
            "Signin.BottomSheetWithAccountSigninMode";
    private static final String BOTTOM_SHEET_SELECTED_ACCOUNT_ID =
            "Signin.BottomSheetSelectedAccountId";

    static Bundle getBundle(@NonNull FullscreenSigninAndHistorySyncConfig config) {
        Bundle bundle = new Bundle();
        bundle.putInt(SIGNIN_CONFIG_TITLE_ID, config.signinConfig.titleId);
        bundle.putInt(SIGNIN_CONFIG_SUBTITLE_ID, config.signinConfig.subtitleId);
        bundle.putInt(SIGNIN_CONFIG_DISMISS_TEXT_ID, config.signinConfig.dismissTextId);
        bundle.putInt(SIGNIN_CONFIG_LOGO_ID, config.signinConfig.logoId);
        bundle.putBoolean(SIGNIN_CONFIG_DISABLE_SIGNIN, config.signinConfig.shouldDisableSignin);
        bundle.putInt(HISTORY_SYNC_CONFIG_TITLE_ID, config.historySyncConfig.titleId);
        bundle.putInt(HISTORY_SYNC_CONFIG_SUBTITLE_ID, config.historySyncConfig.subtitleId);
        bundle.putInt(HISTORY_OPT_IN_MODE, config.historyOptInMode);
        return bundle;
    }

    static FullscreenSigninAndHistorySyncConfig getFullscreenConfig(@NonNull Bundle bundle) {

        FullscreenSigninAndHistorySyncConfig.Builder builder =
                new FullscreenSigninAndHistorySyncConfig.Builder();
        builder.signinTitleId(bundle.getInt(SIGNIN_CONFIG_TITLE_ID, 0));
        builder.signinSubtitleId(bundle.getInt(SIGNIN_CONFIG_SUBTITLE_ID, 0));
        builder.signinDismissTextId(bundle.getInt(SIGNIN_CONFIG_DISMISS_TEXT_ID, 0));
        builder.signinLogoId(bundle.getInt(SIGNIN_CONFIG_LOGO_ID, 0));
        builder.shouldDisableSignin(bundle.getBoolean(SIGNIN_CONFIG_DISABLE_SIGNIN, false));
        builder.historySyncTitleId(bundle.getInt(HISTORY_SYNC_CONFIG_TITLE_ID, 0));
        builder.historySyncSubtitleId(bundle.getInt(HISTORY_SYNC_CONFIG_SUBTITLE_ID, 0));
        builder.historyOptInMode(bundle.getInt(HISTORY_OPT_IN_MODE, 0));
        return builder.build();
    }

    static Bundle getBundle(@NonNull BottomSheetSigninAndHistorySyncConfig config) {
        Bundle bundle = new Bundle();
        bundle.putInt(BOTTOM_SHEET_STRINGS_TITLE_ID, config.bottomSheetStrings.titleStringId);
        bundle.putInt(BOTTOM_SHEET_STRINGS_SUBTITLE_ID, config.bottomSheetStrings.subtitleStringId);
        bundle.putInt(
                BOTTOM_SHEET_STRINGS_DISMISS_BUTTON_ID,
                config.bottomSheetStrings.dismissButtonStringId);

        bundle.putInt(HISTORY_SYNC_CONFIG_TITLE_ID, config.historySyncConfig.titleId);
        bundle.putInt(HISTORY_SYNC_CONFIG_SUBTITLE_ID, config.historySyncConfig.subtitleId);

        bundle.putInt(HISTORY_OPT_IN_MODE, config.historyOptInMode);
        bundle.putInt(BOTTOM_SHEET_NO_ACCOUNT_SIGNIN_MODE, config.noAccountSigninMode);
        bundle.putInt(BOTTOM_SHEET_WITH_ACCOUNT_SIGNIN_MODE, config.withAccountSigninMode);
        bundle.putString(
                BOTTOM_SHEET_SELECTED_ACCOUNT_ID,
                config.selectedCoreAccountId == null
                        ? null
                        : config.selectedCoreAccountId.getId().toString());
        return bundle;
    }

    static BottomSheetSigninAndHistorySyncConfig getBottomSheetConfig(@NonNull Bundle bundle) {
        BottomSheetSigninAndHistorySyncConfig.Builder builder =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                        new AccountPickerBottomSheetStrings.Builder(
                                        bundle.getInt(BOTTOM_SHEET_STRINGS_TITLE_ID, 0))
                                .setSubtitleStringId(
                                        bundle.getInt(BOTTOM_SHEET_STRINGS_SUBTITLE_ID, 0))
                                .setDismissButtonStringId(
                                        bundle.getInt(BOTTOM_SHEET_STRINGS_DISMISS_BUTTON_ID, 0))
                                .build(),
                        bundle.getInt(BOTTOM_SHEET_NO_ACCOUNT_SIGNIN_MODE, 0),
                        bundle.getInt(BOTTOM_SHEET_WITH_ACCOUNT_SIGNIN_MODE, 0),
                        bundle.getInt(HISTORY_OPT_IN_MODE, 0));
        builder.historySyncTitleId(bundle.getInt(HISTORY_SYNC_CONFIG_TITLE_ID, 0));
        builder.historySyncSubtitleId(bundle.getInt(HISTORY_SYNC_CONFIG_SUBTITLE_ID, 0));
        String selectedAccountId = bundle.getString(BOTTOM_SHEET_SELECTED_ACCOUNT_ID);
        if (selectedAccountId != null) {
            builder.selectedCoreAccountId(new CoreAccountId(new GaiaId(selectedAccountId)));
        }
        return builder.build();
    }
}
