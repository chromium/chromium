// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Intent;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionPreferences;

/**
 * An InfoBar that lets the user know that Data Saver Lite Mode now also applies to HTTPS pages.
 */
public class PreviewsLitePageInfoBar extends ConfirmInfoBar {
    public static final String FROM_INFOBAR = "FromInfoBar";

    @CalledByNative
    private static InfoBar show(int enumeratedIconId, String message, String linkText) {
        return new PreviewsLitePageInfoBar(
                ResourceId.mapToDrawableId(enumeratedIconId), message, linkText);
    }

    private PreviewsLitePageInfoBar(int iconDrawbleId, String message, String linkText) {
        super(iconDrawbleId, null, message, linkText, null, null);
    }

    @Override
    public void onLinkClicked() {
        super.onLinkClicked();

        final Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                getContext(), DataReductionPreferences.class.getName());
        intent.putExtra(FROM_INFOBAR, true);
        getContext().startActivity(intent);
    }
}
