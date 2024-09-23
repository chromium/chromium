// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.LayoutInflater;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.RoundedCornerImageView;

/** Provides helper methods for Android Infobars. */
class PasswordInfoBarUtils {
    private PasswordInfoBarUtils() {}

    /**
     * Creates account indication footer used for Password InfoBars.
     *
     * @param context InfoBarLayout's context.
     * @param accountImage Profile picture or monogram of signed-in user.
     * @param email E-mail address to be displayed on the footer.
     * @return Footer view to be added to InfoBar.
     */
    static LinearLayout createAccountIndicationFooter(
            Context context, Bitmap accountImage, String email) {
        int smallIconSize =
                context.getResources().getDimensionPixelSize(R.dimen.infobar_small_icon_size);
        int padding = context.getResources().getDimensionPixelOffset(R.dimen.infobar_padding);
        LinearLayout footer =
                (LinearLayout)
                        LayoutInflater.from(context).inflate(R.layout.infobar_footer, null, false);

        TextView emailView = footer.findViewById(R.id.infobar_footer_email);
        emailView.setText(email);

        RoundedCornerImageView profilePicView =
                footer.findViewById(R.id.infobar_footer_profile_pic);
        Bitmap resizedProfilePic =
                Bitmap.createScaledBitmap(accountImage, smallIconSize, smallIconSize, false);
        profilePicView.setRoundedCorners(
                smallIconSize / 2, smallIconSize / 2, smallIconSize / 2, smallIconSize / 2);
        profilePicView.setImageBitmap(resizedProfilePic);

        return footer;
    }
}
