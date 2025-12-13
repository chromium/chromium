// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import static org.chromium.chrome.browser.logo.LogoUtils.getGoogleLogoDrawable;

import android.content.Context;
import android.graphics.Bitmap;
import android.util.AttributeSet;
import android.widget.ImageView;

import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.R;

/** A layout for previewing a custom NTP background image along with a save and a cancel button. */
@NullMarked
public class UploadImagePreviewLayout extends ConstraintLayout {
    private ImageView mLogoView;

    public UploadImagePreviewLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mLogoView = findViewById(R.id.default_search_engine_logo);
    }

    void setLogo(@Nullable Bitmap logoBitmap) {
        if (logoBitmap == null) {
            mLogoView.setImageDrawable(getGoogleLogoDrawable(getContext()));
        } else {
            mLogoView.setImageBitmap(logoBitmap);
        }
    }

    void setLogoVisibility(int visibility) {
        mLogoView.setVisibility(visibility);
    }
}
