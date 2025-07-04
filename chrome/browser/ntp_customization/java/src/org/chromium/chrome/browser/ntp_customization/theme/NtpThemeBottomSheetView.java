// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator.NTPThemeBottomSheetSection.CHROME_COLORS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator.NTPThemeBottomSheetSection.CHROME_DEFAULT;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator.NTPThemeBottomSheetSection.THEME_COLLECTIONS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator.NTPThemeBottomSheetSection.UPLOAD_AN_IMAGE;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.Pair;
import android.view.LayoutInflater;
import android.widget.LinearLayout;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator.NTPThemeBottomSheetSection;

/** The view of the "New tab page appearance" bottom sheet. */
@NullMarked
public class NtpThemeBottomSheetView extends ConstraintLayout {
    private NtpThemeListItemView mChromeDefaultSection;
    private NtpThemeListItemView mUploadImageSection;
    private NtpThemeListItemView mChromeColorsSection;
    private NtpThemeListItemView mThemeCollectionsSection;

    public NtpThemeBottomSheetView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        LinearLayout container = findViewById(R.id.theme_list_items_container);
        LayoutInflater inflater = LayoutInflater.from(getContext());

        // Inflate and add each theme section.
        mChromeDefaultSection =
                (NtpThemeListItemView)
                        inflater.inflate(
                                R.layout.ntp_customization_theme_list_chrome_default_item_layout,
                                container,
                                false);
        container.addView(mChromeDefaultSection);

        mUploadImageSection =
                (NtpThemeListItemView)
                        inflater.inflate(
                                R.layout.ntp_customization_theme_list_upload_an_image_item_layout,
                                container,
                                false);
        container.addView(mUploadImageSection);

        mChromeColorsSection =
                (NtpThemeListItemView)
                        inflater.inflate(
                                R.layout.ntp_customization_theme_list_chrome_colors_item_layout,
                                container,
                                false);
        container.addView(mChromeColorsSection);

        mThemeCollectionsSection =
                (NtpThemeListItemView)
                        inflater.inflate(
                                R.layout.ntp_customization_theme_list_theme_collections_item_layout,
                                container,
                                false);
        container.addView(mThemeCollectionsSection);
    }

    void destroy() {
        for (int i = 0; i < NTPThemeBottomSheetSection.NUM_ENTRIES; i++) {
            NtpThemeListItemView child = assumeNonNull(getItemBySectionType(i));
            child.destroy();
        }
    }

    void setSectionTrailingIconVisibility(
            @NTPThemeBottomSheetSection int sectionType, boolean visible) {
        NtpThemeListItemView ntpThemeListItemView =
                assumeNonNull(getItemBySectionType(sectionType));
        ntpThemeListItemView.setTrailingIconVisibility(visible);
    }

    void setSectionOnClickListener(
            @NTPThemeBottomSheetSection int sectionType, OnClickListener onClickListener) {
        NtpThemeListItemView ntpThemeListItemView =
                assumeNonNull(getItemBySectionType(sectionType));
        ntpThemeListItemView.setOnClickListener(onClickListener);
    }

    void setLeadingIconForThemeCollections(Pair<Integer, Integer> drawableSourcePair) {
        Drawable primaryDrawable =
                ContextCompat.getDrawable(getContext(), drawableSourcePair.first);
        Drawable secondaryDrawable =
                ContextCompat.getDrawable(getContext(), drawableSourcePair.second);
        NtpThemeListItemView themeCollectionsItemView =
                assumeNonNull(getItemBySectionType(THEME_COLLECTIONS));
        NtpThemeListThemeCollectionItemIconView themeCollectionsItemIconView =
                themeCollectionsItemView.findViewById(
                        org.chromium.chrome.browser.ntp_customization.R.id.leading_icon);
        themeCollectionsItemIconView.setImageDrawablePair(
                new Pair<>(primaryDrawable, secondaryDrawable));
    }

    @Nullable NtpThemeListItemView getItemBySectionType(
            @NTPThemeBottomSheetSection int sectionType) {
        switch (sectionType) {
            case CHROME_DEFAULT:
                return mChromeDefaultSection;
            case UPLOAD_AN_IMAGE:
                return mUploadImageSection;
            case CHROME_COLORS:
                return mChromeColorsSection;
            case THEME_COLLECTIONS:
                return mThemeCollectionsSection;
            default:
                assert false : "Section type not supported!";
                return null;
        }
    }
}
