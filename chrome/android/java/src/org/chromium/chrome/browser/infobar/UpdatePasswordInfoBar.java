// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.text.TextUtils;
import android.widget.Spinner;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.chrome.browser.infobar.InfoBarControlLayout.InfoBarArrayAdapter;

/**
 * The Update Password infobar offers the user the ability to update a password for the site.
 */
public class UpdatePasswordInfoBar extends ConfirmInfoBar {
    private final String[] mUsernames;
    private final int mUsernameIndex;
    private final String mDetailsMessage;
    private Spinner mUsernamesSpinner;

    @CalledByNative
    private static InfoBar show(int enumeratedIconId, String[] usernames, int selectedUsername,
            String message, String detailsMessage, String primaryButtonText) {
        return new UpdatePasswordInfoBar(ResourceId.mapToDrawableId(enumeratedIconId), usernames,
                selectedUsername, message, detailsMessage, primaryButtonText);
    }

    private UpdatePasswordInfoBar(int iconDrawableId, String[] usernames, int selectedUsername,
            String message, String detailsMessage, String primaryButtonText) {
        super(iconDrawableId, R.color.infobar_icon_drawable_color, null, message, null,
                primaryButtonText, null);
        mDetailsMessage = detailsMessage;
        mUsernames = usernames;
        mUsernameIndex = selectedUsername;
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
    }

    @CalledByNative
    private int getSelectedUsername() {
        return mUsernames.length == 1 ? 0 : mUsernamesSpinner.getSelectedItemPosition();
    }
}
