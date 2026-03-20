// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import android.content.res.ColorStateList;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.StyleRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the composeplate on the NTP. */
@NullMarked
public class ComposeplateCoordinator {
    private final PropertyModel mModel;
    private final ComposeplateView mView;
    private final int mComposeplateViewMaxiumWidth;

    /**
     * Constructs a new ComposeplateCoordinator.
     *
     * @param parentView The parent {@link ViewGroup} for the composeplate.
     * @param profile The current user profile.
     */
    public ComposeplateCoordinator(ViewGroup parentView, Profile profile) {
        mModel = new PropertyModel(ComposeplateProperties.ALL_KEYS);
        mView = parentView.findViewById(R.id.composeplate_view);
        PropertyModelChangeProcessor.create(mModel, mView, ComposeplateViewBinder::bind);

        mComposeplateViewMaxiumWidth =
                parentView
                        .getResources()
                        .getDimensionPixelSize(R.dimen.composeplate_view_max_width);
    }

    /**
     * Sets the visibility of the composeplate.
     *
     * @param visible Whether the composeplate should be visible.
     * @param isCurrentPage whether the New Tab Page is the current page displayed to the user.
     */
    public void setVisibility(boolean visible, boolean isCurrentPage) {
        if (isCurrentPage && visible != mModel.get(ComposeplateProperties.IS_VISIBLE)) {
            ComposeplateMetricsUtils.recordComposeplateImpression(visible);
        }

        mModel.set(ComposeplateProperties.IS_VISIBLE, visible);
    }

    /**
     * Sets the click listener for the voice search button.
     *
     * @param voiceSearchClickListener The click listener for the voice search button.
     */
    public void setVoiceSearchClickListener(View.OnClickListener voiceSearchClickListener) {
        mModel.set(
                ComposeplateProperties.VOICE_SEARCH_CLICK_LISTENER,
                createEnhancedClickListener(
                        voiceSearchClickListener,
                        ModuleTypeOnStartAndNtp.COMPOSEPLATE_VIEW_VOICE_SEARCH_BUTTON));
    }

    /**
     * Sets the click listener for the lens button.
     *
     * @param lensClickListener The click listener for the lens button.
     */
    public void setLensClickListener(View.OnClickListener lensClickListener) {
        mModel.set(
                ComposeplateProperties.LENS_CLICK_LISTENER,
                createEnhancedClickListener(
                        lensClickListener, ModuleTypeOnStartAndNtp.COMPOSEPLATE_VIEW_LENS_BUTTON));
    }

    /**
     * Sets the click listener for the incognito button.
     *
     * @param incognitoClickListener The click listener for the incognito button.
     */
    public void setIncognitoClickListener(View.OnClickListener incognitoClickListener) {
        mModel.set(
                ComposeplateProperties.INCOGNITO_CLICK_LISTENER,
                createEnhancedClickListener(
                        incognitoClickListener,
                        ModuleTypeOnStartAndNtp.COMPOSEPLATE_VIEW_INCOGNITO_BUTTON));
    }

    /**
     * Sets the click listener for the composeplate button.
     *
     * @param composeplateButtonClickListener The click listener for the composeplate button.
     */
    public void setComposeplateButtonClickListener(
            View.OnClickListener composeplateButtonClickListener) {
        mModel.set(
                ComposeplateProperties.COMPOSEPLATE_BUTTON_CLICK_LISTENER,
                createEnhancedClickListener(
                        composeplateButtonClickListener,
                        ModuleTypeOnStartAndNtp.COMPOSEPLATE_BUTTON));
    }

    /**
     * Sets the width of the composeplate view in LayoutParams and clears its margins. This should
     * be called before the parent view's measure pass to avoid double measurement.
     *
     * @param searchBoxWidthPx The width of the fake search box.
     */
    public void setLayoutWidth(int searchBoxWidthPx) {
        // In landscape mode on tablets, the composeplate view has a maximum width of 680dp.
        // Otherwise, its width matches the fake search box.
        int composeplateViewWidth = Math.min(searchBoxWidthPx, mComposeplateViewMaxiumWidth);
        ViewGroup.MarginLayoutParams layoutParams =
                (ViewGroup.MarginLayoutParams) mView.getLayoutParams();
        if (layoutParams.width == composeplateViewWidth
                && layoutParams.leftMargin == 0
                && layoutParams.rightMargin == 0) {
            return;
        }

        layoutParams.width = composeplateViewWidth;
        layoutParams.leftMargin = 0;
        layoutParams.rightMargin = 0;
    }

    /**
     * Wraps the given {@link View.OnClickListener} to record the click metric before invoking the
     * original listener.
     *
     * @param originalListener The original click listener to be wrapped.
     * @param sectionType The {@link ModuleTypeOnStartAndNtp} to record for the click.
     */
    private View.OnClickListener createEnhancedClickListener(
            View.OnClickListener originalListener, @ModuleTypeOnStartAndNtp int sectionType) {
        return v -> {
            if (originalListener != null) {
                originalListener.onClick(v);
            }
            ComposeplateMetricsUtils.recordComposeplateClick(sectionType);
        };
    }

    public void destroy() {
        mModel.set(ComposeplateProperties.VOICE_SEARCH_CLICK_LISTENER, null);
        mModel.set(ComposeplateProperties.LENS_CLICK_LISTENER, null);
        mModel.set(ComposeplateProperties.INCOGNITO_CLICK_LISTENER, null);
        mModel.set(ComposeplateProperties.COMPOSEPLATE_BUTTON_CLICK_LISTENER, null);
    }

    public void applyWhiteBackgroundWithShadow(boolean apply) {
        mModel.set(ComposeplateProperties.APPLY_WHITE_BACKGROUND_WITH_SHADOW, apply);

        ColorStateList colorStateList =
                ComposeplateUtils.getSearchBoxIconColorTint(mView.getContext(), apply);
        @StyleRes int textStyleResId = ComposeplateUtils.getSearchBoxTextStyleResId(apply);
        mModel.set(ComposeplateProperties.COLOR_STATE_LIST, colorStateList);
        mModel.set(ComposeplateProperties.TEXT_STYLE_RES_ID, textStyleResId);
    }

    public PropertyModel getModelForTesting() {
        return mModel;
    }
}
