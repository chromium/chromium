// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.R;

@NullMarked
public class NtpBackgroundImageLayout extends FrameLayout {
    private @Nullable Bitmap mBitmap;
    private ImageView mBackgroundImageView;
    private View mGradientView;

    public NtpBackgroundImageLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mBackgroundImageView = findViewById(R.id.image_view);
        mGradientView = findViewById(R.id.gradient_view);
    }

    void setBitmap(Bitmap bitmap) {
        mBackgroundImageView.setImageBitmap(bitmap);
        mGradientView.setVisibility(bitmap != null ? View.VISIBLE : View.GONE);
        mBitmap = bitmap;
    }

    void setImageMatrix(Matrix matrix) {
        mBackgroundImageView.setImageMatrix(matrix);
    }

    void setScaleType(ScaleType scaleType) {
        mBackgroundImageView.setScaleType(scaleType);
    }

    void setDensity(int density) {
        assumeNonNull(mBitmap);
        mBitmap.setDensity(density);

        // Re-apply the bitmap to force the ImageView to re-calculate its intrinsic bounds.
        // Without this, the internal drawable cache may use stale metadata after simultaneous
        // changes to system DPI and navigation mode, leading to incorrect scaling.
        mBackgroundImageView.setImageBitmap(mBitmap);
    }
}
