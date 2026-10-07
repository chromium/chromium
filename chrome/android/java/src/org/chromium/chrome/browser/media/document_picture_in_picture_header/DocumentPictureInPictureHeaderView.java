// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.document_picture_in_picture_header;

import android.content.Context;
import android.content.res.ColorStateList;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

/** The custom view for the Document Picture-in-Picture (PiP) header. */
@NullMarked
public class DocumentPictureInPictureHeaderView extends LinearLayout {
    private ImageView mSecurityIcon;
    private TextView mUrlBar;
    private ImageView mBackToTabButton;

    public DocumentPictureInPictureHeaderView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mSecurityIcon = findViewById(R.id.document_picture_in_picture_header_security_icon);
        mUrlBar = findViewById(R.id.document_picture_in_picture_header_url_bar);
        mBackToTabButton = findViewById(R.id.document_picture_in_picture_header_back_to_tab);
    }

    ImageView getSecurityIconForTesting() {
        return mSecurityIcon;
    }

    TextView getUrlBarForTesting() {
        return mUrlBar;
    }

    ImageView getBackToTabButtonForTesting() {
        return mBackToTabButton;
    }

    void setComponentSize(int componentSize) {
        ViewGroup.LayoutParams backToTabParams = mBackToTabButton.getLayoutParams();
        backToTabParams.width = componentSize;
        backToTabParams.height = componentSize;
        mBackToTabButton.setLayoutParams(backToTabParams);

        ViewGroup.LayoutParams securityIconParams = mSecurityIcon.getLayoutParams();
        securityIconParams.width = componentSize;
        securityIconParams.height = componentSize;
        mSecurityIcon.setLayoutParams(securityIconParams);

        ViewGroup.LayoutParams urlBarParams = mUrlBar.getLayoutParams();
        urlBarParams.height = componentSize;
        mUrlBar.setLayoutParams(urlBarParams);
    }

    void setTintColorList(@Nullable ColorStateList tintColorList) {
        ImageViewCompat.setImageTintList(mBackToTabButton, tintColorList);
        ImageViewCompat.setImageTintList(mSecurityIcon, tintColorList);
    }

    void setBrandedColorScheme(@BrandedColorScheme int brandedColorScheme) {
        mUrlBar.setTextColor(
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        getContext(), brandedColorScheme));
    }

    void setSecurityIconContentDescription(@StringRes int descriptionResId) {
        if (descriptionResId != 0) {
            mSecurityIcon.setContentDescription(getResources().getString(descriptionResId));
        }
    }

    void setSecurityIconResource(@DrawableRes int resId) {
        mSecurityIcon.setImageResource(resId);
    }

    void setSecurityIconClickListener(OnClickListener listener) {
        mSecurityIcon.setOnClickListener(listener);
    }

    void setBackToTabClickListener(OnClickListener listener) {
        mBackToTabButton.setOnClickListener(listener);
    }

    void setBackToTabShown(boolean shown) {
        mBackToTabButton.setVisibility(shown ? VISIBLE : GONE);
    }

    void setUrl(String urlHost) {
        mUrlBar.setText(urlHost);
        mUrlBar.setTooltipText(urlHost);
    }

    void setUrlEllipsizeBehavior(TextUtils.TruncateAt behavior) {
        mUrlBar.setEllipsize(behavior);
    }
}
