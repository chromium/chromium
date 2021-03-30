// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.previews;

import android.app.Activity;
import android.os.Bundle;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.datareduction.settings.DataReductionPreferenceFragment;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.ui.messages.infobar.SimpleConfirmInfoBarBuilder;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

/**
 * Common utility API used in java land.
 */
public final class HttpsImageCompressionUtils {
    public static final String FROM_LITE_MODE_HTTPS_IMAGE_COMPRESSION_INFOBAR =
            "FromLiteModeHttpsImageCompressionInfoBar";

    /**
     * Actions taken on the infobar. This enum must remain synchronized with the
     * HttpsImageCompressionInfoBarAction enum name in metrics/histograms/enums.xml.
     */
    private static final int HTTPS_IMAGE_COMPRESSION_INFO_BAR_SHOWN = 0;
    private static final int HTTPS_IMAGE_COMPRESSION_INFO_BAR_DISMISSED = 1;
    private static final int HTTPS_IMAGE_COMPRESSION_INFO_BAR_LINK_CLICKED = 2;
    private static final int HTTPS_IMAGE_COMPRESSION_INFO_BAR_MAX_VALUE = 3;

    /**
     * Creates InfoBar that shows https images are optimized in the tab.
     */
    public static boolean createInfoBar(final Tab tab) {
        final Activity activity = TabUtils.getActivity(tab);
        if (activity == null) {
            return false;
        }
        SimpleConfirmInfoBarBuilder.create(tab.getWebContents(),
                new SimpleConfirmInfoBarBuilder.Listener() {
                    @Override
                    public void onInfoBarDismissed() {
                        recordInfoBarAction(HTTPS_IMAGE_COMPRESSION_INFO_BAR_DISMISSED);
                    }

                    @Override
                    public boolean onInfoBarButtonClicked(boolean isPrimary) {
                        return false;
                    }

                    @Override
                    public boolean onInfoBarLinkClicked() {
                        Bundle fragmentArgs = new Bundle();
                        fragmentArgs.putBoolean(
                                FROM_LITE_MODE_HTTPS_IMAGE_COMPRESSION_INFOBAR, true);
                        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
                        settingsLauncher.launchSettingsActivity(tab.getContext(),
                                DataReductionPreferenceFragment.class, fragmentArgs);
                        recordInfoBarAction(HTTPS_IMAGE_COMPRESSION_INFO_BAR_LINK_CLICKED);
                        return true;
                    }
                },
                InfoBarIdentifier.LITE_MODE_HTTPS_IMAGE_COMPRESSION_INFOBAR_ANDROID,
                tab.getContext(), R.drawable.preview_pin_round /* drawableId */,
                activity.getString(
                        R.string.lite_mode_https_image_compression_message) /* message */,
                null /* primaryText */, null /* secondaryText */,
                activity.getString(
                        R.string.lite_mode_https_image_compression_settings_link) /* linkText */,
                false /*autoExpire */);
        recordInfoBarAction(HTTPS_IMAGE_COMPRESSION_INFO_BAR_SHOWN);
        return true;
    }

    private static void recordInfoBarAction(int action) {
        assert action >= 0 && action < HTTPS_IMAGE_COMPRESSION_INFO_BAR_MAX_VALUE;
        RecordHistogram.recordEnumeratedHistogram(
                "SubresourceRedirect.ImageCompressionNotificationInfoBar", action,
                HTTPS_IMAGE_COMPRESSION_INFO_BAR_MAX_VALUE);
    }
}
