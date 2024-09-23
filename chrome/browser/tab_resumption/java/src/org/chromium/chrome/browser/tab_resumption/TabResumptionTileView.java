// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.DISPLAY_TEXT_MAX_LINES_DEFAULT;
import static org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.DISPLAY_TEXT_MAX_LINES_WITH_REASON;

import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.widget.RoundedCornerImageView;
import org.chromium.components.browser_ui.widget.chips.ChipView;

/**
 * The view for a tab suggestion tile. These tile comes in two variants: A larger one for the
 * "single-tile" case, and a smaller one for the "multi-tile" case.
 */
public class TabResumptionTileView extends RelativeLayout {
    static final String SEPARATE_COMMA = ", ";
    static final String SEPARATE_PERIOD = ". ";

    private RoundedCornerImageView mIconView;
    private TextView mTilePreInfoView;
    private TextView mTileDisplayView;
    private TextView mTilePostInfoView;

    private final int mSalientImageCornerRadiusPx;

    public TabResumptionTileView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        mSalientImageCornerRadiusPx =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.tab_resumption_module_icon_rounded_corner_radius);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mIconView = findViewById(R.id.tile_icon);
        mTilePreInfoView = findViewById(R.id.tile_pre_info_text);
        mTileDisplayView = findViewById(R.id.tile_display_text);
        mTilePostInfoView = findViewById(R.id.tile_post_info_text);
    }

    void destroy() {
        setOnLongClickListener(null);
        setOnClickListener(null);
    }

    /**
     * Assigns all texts for the "single-tile" case and returns the content description string.
     *
     * @param preInfoText Info to show above main text.
     * @param appChipText Text on the app chip.
     * @param displayText Main text (page title).
     * @param postInfoText Info to show below main text.
     */
    public String setSuggestionTextsSingle(
            @Nullable String preInfoText,
            @Nullable String appChipText,
            String displayText,
            String postInfoText) {
        boolean showPreInfoText = !TextUtils.isEmpty(preInfoText);
        mTilePreInfoView.setText(preInfoText);
        mTilePreInfoView.setVisibility(showPreInfoText ? VISIBLE : GONE);

        mTileDisplayView.setMaxLines(
                showPreInfoText
                        ? DISPLAY_TEXT_MAX_LINES_WITH_REASON
                        : DISPLAY_TEXT_MAX_LINES_DEFAULT);
        mTileDisplayView.setText(displayText);
        ((TextView) findViewById(R.id.tile_post_info_text)).setText(postInfoText);

        StringBuilder stringBuilder = new StringBuilder();
        if (showPreInfoText) {
            stringBuilder.append(preInfoText);
            stringBuilder.append(SEPARATE_COMMA);
        }
        if (appChipText != null) {
            stringBuilder.append(appChipText);
            stringBuilder.append(SEPARATE_COMMA);
        }
        stringBuilder.append(displayText);
        stringBuilder.append(SEPARATE_COMMA);
        stringBuilder.append(postInfoText);
        stringBuilder.append(SEPARATE_PERIOD);

        String contentDescription = stringBuilder.toString();
        setContentDescription(contentDescription);
        return contentDescription;
    }

    /**
     * Replaces the pref-info text with an app chip that displays the info (icon/label) of the app
     * generated the history entry.
     *
     * @param packageManager {@link PackageManager} to get the app info.
     * @param entryType Type of the suggestion entry.
     * @param appId ID of the app that has opened this entry.
     * @return Text on the chip view, 'From YouTube', for example. {@code null} if not shown.
     */
    public @Nullable String maybeShowAppChip(
            PackageManager packageManager, int entryType, String appId) {
        if (entryType != SuggestionEntryType.HISTORY) return null;

        TextView titleView = findViewById(R.id.tile_display_text);
        var titleLayout = (RelativeLayout.LayoutParams) titleView.getLayoutParams();
        AppInfo appInfo = AppInfo.create(packageManager, appId);
        String label = null;
        if (appInfo.isValid()) {
            label = getResources().getString(R.string.history_app_attribution, appInfo.getLabel());
            int chipPaddingStart =
                    getResources()
                            .getDimensionPixelSize(
                                    R.dimen.tab_resumption_module_appchip_padding_start);
            ChipView chipView = findViewById(R.id.tile_app_chip);
            chipView.setPaddingRelative(
                    chipPaddingStart,
                    chipView.getPaddingTop(),
                    chipView.getPaddingEnd(),
                    chipView.getPaddingBottom());
            chipView.getPrimaryTextView().setText(label);
            chipView.setIcon(appInfo.getIcon(), false);
            chipView.setVisibility(View.VISIBLE);
            titleLayout.addRule(RelativeLayout.BELOW, chipView.getId());
        } else {
            titleLayout.removeRule(RelativeLayout.BELOW);
            titleLayout.addRule(RelativeLayout.BELOW, R.id.tile_pre_info_text);
        }
        findViewById(R.id.tile_pre_info_text).setVisibility(View.GONE);
        return label;
    }

    /**
     * Assigns all texts for the "multi-tile" case and returns the content description string.
     *
     * @param displayText Main text (page title).
     * @param postInfoText Info to show below main text.
     */
    public String setSuggestionTextsMulti(String displayText, String postInfoText) {
        ((TextView) findViewById(R.id.tile_display_text)).setText(displayText);
        ((TextView) findViewById(R.id.tile_post_info_text)).setText(postInfoText);

        // Construct a content description from the TabResumptionTileView. This string will be used
        // to construct the content description of its parent view TabResumptionModuleView which
        // currently has no text accessible to TalkBack. When TabResumptionModuleView is focused,
        // TalkBack will sequentially read all translated strings from its subviews, using
        // SEPARATE_COMMA as a delimiter between each.
        StringBuilder stringBuilder = new StringBuilder();
        stringBuilder.append(displayText);
        stringBuilder.append(SEPARATE_COMMA);
        stringBuilder.append(postInfoText);
        stringBuilder.append(SEPARATE_PERIOD);

        String contentDescription = stringBuilder.toString();
        setContentDescription(contentDescription);
        return contentDescription;
    }

    /** Assigns the main URL image. */
    public void setImageDrawable(Drawable drawable) {
        mIconView.setImageDrawable(drawable);
    }

    /** Updates the image view to show a salient image. */
    public void updateForSalientImage() {
        mIconView.setPadding(0, 0, 0, 0);
        mIconView.setRoundedCorners(
                mSalientImageCornerRadiusPx,
                mSalientImageCornerRadiusPx,
                mSalientImageCornerRadiusPx,
                mSalientImageCornerRadiusPx);
    }

    /**
     * Updates the post info text of the tile. It consists of a domain URL for a local Tab, and a
     * domain URL + the device info for a remote Tab.
     */
    public void updatePostInfoView(String postInfoText) {
        mTilePostInfoView.setText(postInfoText);
    }
}
