// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.widget.ChipView;
import org.chromium.url.GURL;

/**
 * Specific {@link FrameLayout} that displays the Web Feed footer in the main menu.
 */
public class WebFeedMainMenuItem extends FrameLayout {
    private final Context mContext;

    private GURL mUrl;
    private ImageView mIcon;
    private LargeIconBridge mLargeIconBridge;

    /**
     * Constructs a new {@link WebFeedMainMenuItem} with the appropriate context.
     */
    public WebFeedMainMenuItem(Context context, AttributeSet attrs) {
        super(context, attrs);

        mContext = context;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
    }

    public void initialize(GURL url, LargeIconBridge largeIconBridge) {
        mUrl = url;
        mLargeIconBridge = largeIconBridge;

        initializeFavicon();
        initializeText();
        initializeChipView();
    }

    private void initializeFavicon() {
        mIcon = findViewById(R.id.icon);
        mLargeIconBridge.getLargeIconForUrl(mUrl,
                mContext.getResources().getDimensionPixelSize(R.dimen.web_feed_icon_size),
                this::onFaviconAvailable);
    }

    private void initializeText() {
        // TODO(crbug/1152592): Get metadata from WebFeedBridge.
        TextView itemText = findViewById(R.id.menu_item_text);
        String title = UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(mUrl);
        itemText.setText(title);
    }

    private void initializeChipView() {
        ChipView chipView = findViewById(R.id.chip_view);
        TextView chipTextView = chipView.getPrimaryTextView();

        // TODO(crbug/1152592): Account for following case.
        chipTextView.setText(mContext.getText(R.string.menu_follow));
        chipView.setIcon(AppCompatResources.getDrawable(mContext, R.drawable.ic_add), true);
    }

    /**
     * Passed as the callback to {@link LargeIconBridge#getLargeIconForUrl}.
     */
    private void onFaviconAvailable(@Nullable Bitmap icon, @ColorInt int fallbackColor,
            boolean isColorDefault, @IconType int iconType) {
        // If we didn't get a favicon, generate a monogram instead
        if (icon == null) {
            // TODO(crbug/1152592): Update monogram according to specs.
            RoundedIconGenerator iconGenerator = createRoundedIconGenerator(fallbackColor);
            icon = iconGenerator.generateIconForUrl(mUrl.getSpec());
            mIcon.setImageBitmap(icon);
            // generateIconForUrl() might return null if the URL is empty or the domain cannot be
            // resolved. See https://crbug.com/987101
            if (icon == null) {
                mIcon.setVisibility(View.GONE);
            }
        } else {
            mIcon.setImageBitmap(icon);
        }
    }

    private RoundedIconGenerator createRoundedIconGenerator(@ColorInt int iconColor) {
        Resources resources = mContext.getResources();
        int iconSize = resources.getDimensionPixelSize(R.dimen.web_feed_icon_size);
        int cornerRadius = iconSize / 2;
        int textSize = resources.getDimensionPixelSize(R.dimen.web_feed_monogram_text_size);

        return new RoundedIconGenerator(iconSize, iconSize, cornerRadius, iconColor, textSize);
    }
}
