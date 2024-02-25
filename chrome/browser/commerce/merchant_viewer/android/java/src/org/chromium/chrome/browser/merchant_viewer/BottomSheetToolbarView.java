// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.DrawableRes;

import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.FadingShadow;
import org.chromium.components.browser_ui.widget.FadingShadowView;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.url.GURL;

/** BottomSheetToolbar UI. */
public class BottomSheetToolbarView {
    private final View mToolbarView;

    /**
     * Construct the BottomSheetToolbarView.
     *
     * @param context The context where the bottom-sheet should be shown.
     */
    public BottomSheetToolbarView(Context context) {
        mToolbarView = LayoutInflater.from(context).inflate(R.layout.sheet_tab_toolbar, null);

        FadingShadowView shadow = mToolbarView.findViewById(R.id.shadow);
        shadow.init(context.getColor(R.color.toolbar_shadow_color), FadingShadow.POSITION_TOP);
    }

    /** Sets the title of the bottom sheet. */
    public void setTitle(String title) {
        TextView toolbarText = mToolbarView.findViewById(R.id.title);
        toolbarText.setText(title);
    }

    /** Sets the second line in the toolbar to the the provided URL. */
    public void setUrl(GURL url) {
        TextView originView = mToolbarView.findViewById(R.id.origin);
        originView.setText(
                UrlFormatter.formatUrlForSecurityDisplay(url, SchemeDisplay.OMIT_HTTP_AND_HTTPS));
    }

    /** Sets the security icon. */
    public void setSecurityIcon(@DrawableRes int resId) {
        ImageView securityIcon = mToolbarView.findViewById(R.id.security_icon);
        securityIcon.setImageResource(resId);
    }

    /** Sets the security icon content description. */
    public void setSecurityIconDescription(String description) {
        ImageView securityIcon = mToolbarView.findViewById(R.id.security_icon);
        securityIcon.setContentDescription(description);
    }

    /** Sets the security icon click callback. */
    public void setSecurityIconClickCallback(Runnable callback) {
        ImageView securityIcon = mToolbarView.findViewById(R.id.security_icon);
        securityIcon.setOnClickListener(
                v -> {
                    if (callback != null) callback.run();
                });
    }

    /** Sets the close button click callback. */
    public void setCloseButtonClickCallback(Runnable callback) {
        ImageView closeButton = mToolbarView.findViewById(R.id.close);
        closeButton.setOnClickListener(
                v -> {
                    if (callback != null) callback.run();
                });
    }

    /** Sets the progress on the progress bar. */
    public void setProgress(float progress) {
        ProgressBar progressBar = mToolbarView.findViewById(R.id.progress_bar);
        progressBar.setProgress(Math.round(progress * 100));
    }

    /** Called to show or hide the progress bar. */
    public void setProgressVisible(boolean visible) {
        ProgressBar progressBar = mToolbarView.findViewById(R.id.progress_bar);
        progressBar.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    /** Sets the favicon icon. */
    public void setFaviconIcon(@DrawableRes int resId) {
        ImageView faviconIcon = mToolbarView.findViewById(R.id.favicon);
        faviconIcon.setImageResource(resId);
    }

    /** Sets the favicon icon drawable. */
    public void setFaviconIconDrawable(Drawable iconDrawable) {
        ImageView faviconIcon = mToolbarView.findViewById(R.id.favicon);
        faviconIcon.setImageDrawable(iconDrawable);
    }

    /** Sets the visibility of favicon icon. */
    public void setFaviconIconVisible(boolean visible) {
        ImageView faviconIcon = mToolbarView.findViewById(R.id.favicon);
        faviconIcon.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    /** Sets the visibility of open-in-new-tab button. */
    public void setOpenInNewTabButtonVisible(boolean visible) {
        ImageView openInNewTabButton = mToolbarView.findViewById(R.id.open_in_new_tab);
        openInNewTabButton.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    /** @return The height of the toolbar in pixels. */
    public int getToolbarHeightPx() {
        return mToolbarView.getHeight();
    }

    /** @return The android {@link View} representing this BottomSheetToolbar. */
    public View getView() {
        return mToolbarView;
    }
}
