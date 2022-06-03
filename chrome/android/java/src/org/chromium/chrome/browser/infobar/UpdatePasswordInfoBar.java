// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.text.TextUtils;
import android.widget.Spinner;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.components.infobars.ConfirmInfoBar;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarControlLayout;
import org.chromium.components.infobars.InfoBarControlLayout.InfoBarArrayAdapter;
import org.chromium.components.infobars.InfoBarLayout;
import org.chromium.components.signin.base.AccountInfo;

/**
 * The Update Password infobar offers the user the ability to update a password for the site.
 */
public class UpdatePasswordInfoBar extends ConfirmInfoBar {
    private final String[] mUsernames;
    private final int mUsernameIndex;
    private final String mDetailsMessage;
    private final AccountInfo mAccountInfo;
    private Spinner mUsernamesSpinner;

    @CalledByNative
    private static InfoBar show(int iconId, String[] usernames, int selectedUsername,
            String message, String detailsMessage, String primaryButtonText,
            AccountInfo accountInfo) {
        // If accountInfo is empty, no footer will be shown.
        return new UpdatePasswordInfoBar(iconId, usernames, selectedUsername, message,
                detailsMessage, primaryButtonText, accountInfo);
    }

    private UpdatePasswordInfoBar(int iconDrawableId, String[] usernames, int selectedUsername,
            String message, String detailsMessage, String primaryButtonText,
            AccountInfo accountInfo) {
        super(iconDrawableId, R.color.infobar_icon_drawable_color, null, message, null,
                primaryButtonText, null);
        mDetailsMessage = detailsMessage;
        mUsernames = usernames;
        mUsernameIndex = selectedUsername;
        mAccountInfo = accountInfo;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);

        InfoBarControlLayout usernamesLayout = layout.addControlLayout();
        if (mUsernames.length > 1) {
            InfoBarArrayAdapter<String> usernamesAdapter =
                    new InfoBarArrayAdapter<String>(getContext(), mUsernames);
            mUsernamesSpinner = usernamesLayout.addSpinner(
                    R.id.password_infobar_accounts_spinner, usernamesAdapter);
            mUsernamesSpinner.setSelection(mUsernameIndex);
        } else {
            usernamesLayout.addDescription(mUsernames[0]);
        }

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

    @CalledByNative
    private int getSelectedUsername() {
        return mUsernames.length == 1 ? 0 : mUsernamesSpinner.getSelectedItemPosition();
    }
}
