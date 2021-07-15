// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.RelativeLayout;

import androidx.core.view.ViewCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.tab_ui.R;

/**
 * The view used by NewTabTile component.
 */
class NewTabTileView extends RelativeLayout {
    private float mRatio = 1f;
    private int mHeightIntercept;
    private ImageView mActionButtonView;

    public NewTabTileView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mActionButtonView = findViewById(R.id.new_tab_button);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        Context context = getContext();
        int width = getMeasuredWidth();
        int height = (int) (getMeasuredWidth() / mRatio
                             + context.getResources().getDimension(R.dimen.tab_grid_favicon_size))
                + mHeightIntercept;
        setMeasuredDimension(width, height);

        // Reposition the plus sign to the middle of the new tab tile.
        View view = findViewById(R.id.new_tab_button);
        view.setTranslationX((width - view.getMeasuredWidth()) / 2);
        view.setTranslationY((height - view.getMeasuredHeight()) / 2);
    }

    // TODO(yuezhanggg): Hook up with aspect ratio code.
    void setAspectRatio(float ratio) {
        mRatio = ratio;
    }

    void setHeightIntercept(int intercept) {
        mHeightIntercept = intercept;
    }

    /**
     * Update color of components based on whether in incognito or not.
     * @param isIncognito Whether the color is used for incognito mode.
     */
    void updateColor(boolean isIncognito) {
        ViewCompat.setBackgroundTintList(this,
                TabUiThemeProvider.getHoveredCardBackgroundTintList(
                        getContext(), isIncognito, false));

        ApiCompatibilityUtils.setImageTintList(mActionButtonView,
                TabUiThemeProvider.getNewTabTilePlusTintList(getContext(), isIncognito));
    }
}
