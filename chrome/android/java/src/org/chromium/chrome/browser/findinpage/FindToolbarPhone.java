// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.findinpage;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.styles.ChromeColors;

/**
 * A phone specific version of the {@link FindToolbar}.
 */
public class FindToolbarPhone extends FindToolbar {
    /**
     * Creates an instance of a {@link FindToolbarPhone}.
     * @param context The Context to create the {@link FindToolbarPhone} under.
     * @param attrs The AttributeSet used to create the {@link FindToolbarPhone}.
     */
    public FindToolbarPhone(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void handleActivate() {
        assert isWebContentAvailable();
        setVisibility(View.VISIBLE);
        super.handleActivate();
    }

    @Override
    protected void handleDeactivation(boolean clearSelection) {
        setVisibility(View.GONE);
        super.handleDeactivation(clearSelection);
    }

    @Override
    protected void updateVisualsForTabModel(boolean isIncognito) {
        int queryTextColorId;
        int queryHintTextColorId;
        int dividerColorId;
        if (isIncognito) {
            setBackgroundColor(ChromeColors.getDefaultThemeColor(getResources(), true));
            ColorStateList white = ChromeColors.getIconTint(getContext(), true);
            ApiCompatibilityUtils.setImageTintList(mFindNextButton, white);
            ApiCompatibilityUtils.setImageTintList(mFindPrevButton, white);
            ApiCompatibilityUtils.setImageTintList(mCloseFindButton, white);
            queryTextColorId = R.color.find_in_page_query_white_color;
            queryHintTextColorId = R.color.find_in_page_query_incognito_hint_color;
            dividerColorId = R.color.white_alpha_12;
        } else {
            setBackgroundColor(ChromeColors.getDefaultThemeColor(getResources(), false));
            ColorStateList dark = ChromeColors.getIconTint(getContext(), false);
            ApiCompatibilityUtils.setImageTintList(mFindNextButton, dark);
            ApiCompatibilityUtils.setImageTintList(mFindPrevButton, dark);
            ApiCompatibilityUtils.setImageTintList(mCloseFindButton, dark);
            queryTextColorId = R.color.default_text_color;
            queryHintTextColorId = R.color.find_in_page_query_default_hint_color;
            dividerColorId = R.color.divider_bg_color;
        }
        mFindQuery.setTextColor(
                ApiCompatibilityUtils.getColor(getContext().getResources(), queryTextColorId));
        mFindQuery.setHintTextColor(
                ApiCompatibilityUtils.getColor(getContext().getResources(), queryHintTextColorId));
        mDivider.setBackgroundResource(dividerColorId);
    }

    @Override
    protected int getStatusColor(boolean failed, boolean incognito) {
        if (incognito) {
            final int colorResourceId = failed ? R.color.default_red_light : R.color.white_alpha_50;
            return ApiCompatibilityUtils.getColor(getContext().getResources(), colorResourceId);
        }
        return super.getStatusColor(failed, incognito);
    }
}
