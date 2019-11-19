// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.net.Uri;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.components.url_formatter.UrlFormatter;

/**
 * This InfoBar is shown to let the user know about a blocked Framebust and offer to
 * continue the redirection by tapping on a link.
 */
public class FramebustBlockInfoBar extends InfoBar {

    private final String mBlockedUrl;

    /** Whether the infobar should be shown as a mini-infobar or a classic expanded one. */
    private boolean mIsExpanded;

    @VisibleForTesting
    public FramebustBlockInfoBar(String blockedUrl) {
        super(R.drawable.infobar_chrome, R.color.infobar_icon_drawable_color, null, null);
        mBlockedUrl = blockedUrl;
    }

    @Override
    public void onButtonClicked(boolean isPrimaryButton) {
        assert isPrimaryButton;
        onButtonClicked(ActionType.OK);
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        layout.setMessage(getString(R.string.redirect_blocked_message));
        InfoBarControlLayout control = layout.addControlLayout();

        ViewGroup ellipsizerView =
                (ViewGroup) LayoutInflater.from(getContext())
                        .inflate(R.layout.infobar_control_url_ellipsizer, control, false);

        // Formatting the URL and requesting to omit the scheme might still include it for some of
        // them (e.g. file, filesystem). We split the output of the formatting to make sure we don't
        // end up duplicating it.
        final String schemeSeparator = "://";
        String scheme = Uri.parse(mBlockedUrl).getScheme();

        // In case mBlockedUrl does not specify a scheme, formatUrlForSecurityDisplay returns an
        // empty string. Temporarily adding scheme separator allows it to parse the URL correctly.
        String urlWithScheme = mBlockedUrl;
        if (scheme == null) {
            scheme = "";
            urlWithScheme = schemeSeparator + mBlockedUrl;
        }

        String textToEllipsize = UrlFormatter
                    .formatUrlForSecurityDisplay(urlWithScheme)
                    .substring(scheme.length() + schemeSeparator.length());

        TextView schemeView = ellipsizerView.findViewById(R.id.url_scheme);
        schemeView.setText(scheme);

        TextView urlView = ellipsizerView.findViewById(R.id.url_minus_scheme);
        // Handle adjusting the text to workaround Android crashes when ellipsizing on old versions.
        // TODO(donnd): remove this class when older versions of Android are no longer supported.
        ((TextViewEllipsizerSafe) urlView).setTextSafely(textToEllipsize);

        ellipsizerView.setOnClickListener(view -> onLinkClicked());

        control.addView(ellipsizerView);
        layout.setButtons(
                getContext().getResources().getString(R.string.always_allow_redirects), null);
    }

    @Override
    protected void createCompactLayoutContent(InfoBarCompactLayout layout) {
        new InfoBarCompactLayout.MessageBuilder(layout)
                .withText(getString(R.string.redirect_blocked_short_message))
                .withLink(R.string.details_link, view -> onLinkClicked())
                .buildAndInsert();
    }

    @Override
    protected boolean usesCompactLayout() {
        return !mIsExpanded;
    }

    @Override
    public void onLinkClicked() {
        if (!mIsExpanded) {
            mIsExpanded = true;
            replaceView(createView());
            return;
        }

        super.onLinkClicked();
    }

    @VisibleForTesting
    public String getBlockedUrl() {
        return mBlockedUrl;
    }

    private String getString(@StringRes int stringResId) {
        return getContext().getString(stringResId);
    }

    @CalledByNative
    private static FramebustBlockInfoBar create(String blockedUrl) {
        return new FramebustBlockInfoBar(blockedUrl);
    }
}
