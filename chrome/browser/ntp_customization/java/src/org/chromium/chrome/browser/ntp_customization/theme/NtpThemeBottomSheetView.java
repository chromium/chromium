// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.CHROME_COLOR;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.COLOR_FROM_HEX;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.DEFAULT;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.IMAGE_FROM_DISK;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.THEME_COLLECTION;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.Pair;
import android.view.LayoutInflater;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.R;

/** The view of the "New tab page appearance" bottom sheet. */
@NullMarked
public class NtpThemeBottomSheetView extends ScrollView {
    private NtpThemeListItemView mDefaultSection;
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
        mDefaultSection =
                (NtpThemeListItemView)
                        inflater.inflate(
                                R.layout.ntp_customization_theme_list_chrome_default_item_layout,
                                container,
                                false);
        container.addView(mDefaultSection);

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
        for (int i = 0; i < NtpBackgroundImageType.NUM_ENTRIES; i++) {
            if (i == COLOR_FROM_HEX) continue;

            NtpThemeListItemView child = assumeNonNull(getItemBySectionType(i));
            child.destroy();
        }
    }

    void setSectionTrailingIconVisibility(
            @NtpBackgroundImageType int sectionType, boolean visible) {
        NtpThemeListItemView ntpThemeListItemView =
                assumeNonNull(getItemBySectionType(sectionType));
        ntpThemeListItemView.setTrailingIconVisibility(visible);
    }

    void setSectionOnClickListener(
            @NtpBackgroundImageType int sectionType, OnClickListener onClickListener) {
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
                assumeNonNull(getItemBySectionType(THEME_COLLECTION));
        NtpThemeListThemeCollectionItemIconView themeCollectionsItemIconView =
                themeCollectionsItemView.findViewById(
                        org.chromium.chrome.browser.ntp_customization.R.id.leading_icon);
        themeCollectionsItemIconView.setImageDrawablePair(
                new Pair<>(primaryDrawable, secondaryDrawable));
    }

    @Nullable NtpThemeListItemView getItemBySectionType(@NtpBackgroundImageType int sectionType) {
        switch (sectionType) {
            case DEFAULT:
                return mDefaultSection;
            case IMAGE_FROM_DISK:
                return mUploadImageSection;
            case CHROME_COLOR:
            case COLOR_FROM_HEX:
                return mChromeColorsSection;
            case THEME_COLLECTION:
                return mThemeCollectionsSection;
            default:
                assert false : "Section type not supported!";
                return null;
        }
    }
}
