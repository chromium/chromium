// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
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

        if (mAccountInfo != null && !TextUtils.isEmpty(mAccountInfo.getEmail())
                && mAccountInfo.getAccountImage() != null) {
            layout.addFooterView(PasswordInfoBarUtils.createAccountIndicationFooter(
                    layout.getContext(), mAccountInfo.getAccountImage(), mAccountInfo.getEmail()));
        }
    }
}
