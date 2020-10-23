// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.RoundedCornerImageView;
import org.chromium.components.infobars.ConfirmInfoBar;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarControlLayout;
import org.chromium.components.infobars.InfoBarLayout;
import org.chromium.components.signin.base.AccountInfo;

/**
 * The Save Password infobar asks the user whether they want to save the password for the site.
 */
public class SavePasswordInfoBar extends ConfirmInfoBar {
    private final String mDetailsMessage;
    private final AccountInfo mAccountInfo;

    @CalledByNative
    private static InfoBar show(int iconId, String message, String detailsMessage,
            String primaryButtonText, String secondaryButtonText, AccountInfo accountInfo) {
        // If accountInfo is empty, no footer will be shown.
        return new SavePasswordInfoBar(iconId, message, detailsMessage, primaryButtonText,
                secondaryButtonText, accountInfo);
    }

    private SavePasswordInfoBar(int iconDrawbleId, String message, String detailsMessage,
            String primaryButtonText, String secondaryButtonText, AccountInfo accountInfo) {
        super(iconDrawbleId, R.color.infobar_icon_drawable_color, null, message, null,
                primaryButtonText, secondaryButtonText);
        mDetailsMessage = detailsMessage;
        mAccountInfo = accountInfo;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        if (!TextUtils.isEmpty(mDetailsMessage)) {
            InfoBarControlLayout detailsMessageLayout = layout.addControlLayout();
            detailsMessageLayout.addDescription(mDetailsMessage);
        }

        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.AUTOFILL_ENABLE_INFO_BAR_ACCOUNT_INDICATION_FOOTER)
                && !TextUtils.isEmpty(mAccountInfo.getEmail())
                && mAccountInfo.getAccountImage() != null) {
            Resources res = layout.getResources();
            int smallIconSize = res.getDimensionPixelSize(R.dimen.infobar_small_icon_size);
            int padding = res.getDimensionPixelOffset(R.dimen.infobar_padding);

            LinearLayout footer = (LinearLayout) LayoutInflater.from(layout.getContext())
                                          .inflate(R.layout.infobar_footer, null, false);

            TextView emailView = (TextView) footer.findViewById(R.id.infobar_footer_email);
            emailView.setText(mAccountInfo.getEmail());

            RoundedCornerImageView profilePicView =
                    (RoundedCornerImageView) footer.findViewById(R.id.infobar_footer_profile_pic);
            Bitmap resizedProfilePic = Bitmap.createScaledBitmap(
                    mAccountInfo.getAccountImage(), smallIconSize, smallIconSize, false);
            profilePicView.setRoundedCorners(
                    smallIconSize / 2, smallIconSize / 2, smallIconSize / 2, smallIconSize / 2);
            profilePicView.setImageBitmap(resizedProfilePic);

            layout.addFooterView(footer);
        }
    }
}
