// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.support.v7.content.res.AppCompatResources;
import android.widget.ImageView;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.instantapps.InstantAppsBannerData;
import org.chromium.chrome.browser.widget.DualControlLayout;
import org.chromium.components.url_formatter.UrlFormatter;

/**
 * Infobar that asks the user whether they want to use an instant app for a particular website.
 */
public class InstantAppsInfoBar extends ConfirmInfoBar {

    private InstantAppsBannerData mData;

    protected InstantAppsInfoBar(InstantAppsBannerData data) {
        super(0, data.getIcon(), data.getAppName(), null, data.getPrimaryActionLabel(), null);
        mData = data;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);

        layout.setIsUsingBigIcon();
        layout.setMessage(mData.getAppName());
        layout.getMessageLayout().addDescription(
                UrlFormatter.formatUrlForSecurityDisplayOmitScheme(mData.getUrl()));
        layout.getPrimaryButton().setButtonColor(AppCompatResources.getColorStateList(
                getContext(), R.color.app_banner_install_button_bg));
    }

    @Override
    protected void setButtons(InfoBarLayout layout, String primaryText, String secondaryText) {
        ImageView playLogo = new ImageView(layout.getContext());
        playLogo.setImageResource(R.drawable.google_play);
        layout.setBottomViews(
                primaryText, playLogo, DualControlLayout.DualControlLayoutAlignment.APART);
    }

    @CalledByNative
    private static InfoBar create(InstantAppsBannerData data) {
        return new InstantAppsInfoBar(data);
    }

}
