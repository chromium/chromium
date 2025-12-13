// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.junit.Assert.assertEquals;

import android.os.Bundle;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.google_apis.gaia.GaiaId;

/** Unit tests for {@link SigninAndHistorySyncBundleHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SigninAndHistorySyncBundleHelperTest {

    @Test
    @SmallTest
    public void testPutAndGetFullscreenSigninAndHistorySyncConfig() {
        final FullscreenSigninAndHistorySyncConfig initialConfig =
                new FullscreenSigninAndHistorySyncConfig.Builder(
                                "title", "subtitle", "dismiss", "history title", "history subtitle")
                        .historyOptInMode(HistorySyncConfig.OptInMode.REQUIRED)
                        .signinLogoId(3)
                        .shouldDisableSignin(true)
                        .build();

        Bundle bundle = SigninAndHistorySyncBundleHelper.getBundle(initialConfig);
        final FullscreenSigninAndHistorySyncConfig extractedConfig =
                SigninAndHistorySyncBundleHelper.getFullscreenConfig(bundle);

        assertEquals(initialConfig, extractedConfig);
    }

    @Test
    @SmallTest
    public void testPutAndGetBottomSheetSigninAndHistorySyncConfig() {
        final BottomSheetSigninAndHistorySyncConfig initialConfig =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                                new AccountPickerBottomSheetStrings.Builder("title")
                                        .setSubtitleString("subtitle")
                                        .setDismissButtonString("dismiss")
                                        .build(),
                                BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode
                                        .BOTTOM_SHEET,
                                BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode
                                        .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                HistorySyncConfig.OptInMode.REQUIRED,
                                "history title",
                                "history subtitle")
                        .selectedCoreAccountId(new CoreAccountId(new GaiaId("gaia-id")))
                        .build();

        Bundle bundle = SigninAndHistorySyncBundleHelper.getBundle(initialConfig);
        final BottomSheetSigninAndHistorySyncConfig extractedConfig =
                SigninAndHistorySyncBundleHelper.getBottomSheetConfig(bundle);

        assertEquals(initialConfig, extractedConfig);
    }
}
