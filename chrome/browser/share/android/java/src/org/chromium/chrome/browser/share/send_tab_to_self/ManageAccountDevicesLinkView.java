// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.provider.Browser;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.browser_ui.widget.RoundedCornerImageView;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** View containing the sharing account's avatar, email and a link to manage its target devices. */
public class ManageAccountDevicesLinkView extends FrameLayout {
    private static final int ACCOUNT_AVATAR_SIZE_DP = 24;

    public ManageAccountDevicesLinkView(Context context) {
        this(context, null);
    }

    public ManageAccountDevicesLinkView(Context context, AttributeSet attrs) {
        super(context, attrs);
        inflateIfVisible();
    }

    @Override
    public void setVisibility(int visibility) {
        super.setVisibility(visibility);
        inflateIfVisible();
    }

    // TODO(crbug.com/1219434): For now the account information is only filled once the view becomes
    // visible, so it can still be declared in the XML if there is no account. After launch, fill
    // the data immediately.
    private void inflateIfVisible() {
        if (getVisibility() != View.VISIBLE) {
            return;
        }

        // The view was already inflated, nothing else to do.
        if (getChildCount() > 0) {
            return;
        }

        LayoutInflater.from(getContext())
                .inflate(R.layout.send_tab_to_self_manage_devices_link, this);

        AccountInfo account = getSharingAccountInfo();
        assert account != null;

        // The avatar can be null in tests.
        if (account.getAccountImage() != null) {
            RoundedCornerImageView avatarView = findViewById(R.id.account_avatar);
            int accountAvatarSizePx =
                    Math.round(ACCOUNT_AVATAR_SIZE_DP * getResources().getDisplayMetrics().density);
            avatarView.setImageBitmap(Bitmap.createScaledBitmap(
                    account.getAccountImage(), accountAvatarSizePx, accountAvatarSizePx, false));
            avatarView.setRoundedCorners(accountAvatarSizePx / 2, accountAvatarSizePx / 2,
                    accountAvatarSizePx / 2, accountAvatarSizePx / 2);
        }

        // The link is opened in a new tab to avoid exiting the current page, which the user
        // possibly wants to share (maybe they just clicked "Manage devices" by mistake).
        SpannableString linkText = SpanApplier.applySpans(
                getResources().getString(
                        R.string.send_tab_to_self_manage_devices_link, account.getEmail()),
                new SpanApplier.SpanInfo("<link>", "</link>",
                        new NoUnderlineClickableSpan(
                                getContext(), this::openManageDevicesPageInNewTab)));
        TextView linkView = findViewById(R.id.manage_devices_link);
        linkView.setText(linkText);
        linkView.setMovementMethod(LinkMovementMethod.getInstance());
    }

    private void openManageDevicesPageInNewTab(View unused) {
        Intent intent =
                new Intent()
                        .setAction(Intent.ACTION_VIEW)
                        .setData(Uri.parse(UrlConstants.GOOGLE_ACCOUNT_DEVICE_ACTIVITY_URL))
                        .setClass(getContext(), ChromeLauncherActivity.class)
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                        .putExtra(Browser.EXTRA_APPLICATION_ID, getContext().getPackageName())
                        .putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        IntentUtils.addTrustedIntentExtras(intent);
        getContext().startActivity(intent);
    }

    private static AccountInfo getSharingAccountInfo() {
        IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        return identityManager.findExtendedAccountInfoByEmailAddress(
                identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN).getEmail());
    }
}
