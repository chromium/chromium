// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.widget.ChromeImageButton;

/** The custom view for the page zoom app menu item. */
@NullMarked
public class PageZoomMenuItemView extends LinearLayout {
    private TextViewWithCompoundDrawables mTitleView;
    private ChromeImageButton mZoomOutButton;
    private TextView mZoomPercentageText;
    private ChromeImageButton mZoomInButton;
    private View mFullscreenContainer;
    private ChromeImageButton mFullscreenButton;
    private @Nullable OnHoverListener mHoverListener;

    public PageZoomMenuItemView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void setOnHoverListener(@Nullable OnHoverListener listener) {
        mHoverListener = listener;
        // We don't call {@code super.setOnHoverListener()} here because we don't want this view to
        // intercept hover.
    }

    @Override
    protected boolean dispatchHoverEvent(MotionEvent event) {
        boolean handled = super.dispatchHoverEvent(event);
        if (mHoverListener != null) {
            mHoverListener.onHover(this, event);
            return true;
        }
        return handled;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.title);
        mZoomOutButton = findViewById(R.id.zoom_out_button);
        mZoomPercentageText = findViewById(R.id.zoom_percentage);
        mZoomInButton = findViewById(R.id.zoom_in_button);
        mFullscreenContainer = findViewById(R.id.fullscreen_container);
        mFullscreenButton = findViewById(R.id.fullscreen_button);
    }

    void setTitle(@Nullable CharSequence title, @Nullable CharSequence contentDescription) {
        mTitleView.setText(title);
        mTitleView.setContentDescription(contentDescription);
    }

    void setIcon(@Nullable Drawable icon) {
        mTitleView.setCompoundDrawablesRelative(
                icon, /* top= */ null, /* end= */ null, /* bottom= */ null);
    }

    void setZoomInCallback(Runnable callback) {
        mZoomInButton.setOnClickListener(v -> callback.run());
    }

    void setZoomOutCallback(Runnable callback) {
        mZoomOutButton.setOnClickListener(v -> callback.run());
    }

    void setZoomInEnabled(boolean enabled) {
        mZoomInButton.setEnabled(enabled);
        mZoomInButton.setFocusable(enabled);
    }

    void setZoomOutEnabled(boolean enabled) {
        mZoomOutButton.setEnabled(enabled);
        mZoomOutButton.setFocusable(enabled);
    }

    void setZoomPercentText(CharSequence text) {
        mZoomPercentageText.setText(text);
    }

    void setFullscreenEnabled(boolean enabled) {
        mFullscreenContainer.setVisibility(enabled ? VISIBLE : GONE);
    }

    void setFullscreenCallback(Runnable callback) {
        mFullscreenButton.setOnClickListener(v -> callback.run());
    }

    TextViewWithCompoundDrawables getTitleViewForTesting() {
        return mTitleView;
    }

    ChromeImageButton getZoomOutButtonForTesting() {
        return mZoomOutButton;
    }

    TextView getZoomPercentageTextForTesting() {
        return mZoomPercentageText;
    }

    ChromeImageButton getZoomInButtonForTesting() {
        return mZoomInButton;
    }

    View getFullscreenContainerForTesting() {
        return mFullscreenContainer;
    }

    ChromeImageButton getFullscreenButtonForTesting() {
        return mFullscreenButton;
    }
}
