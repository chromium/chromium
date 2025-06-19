// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the composeplate on the NTP. */
@NullMarked
public class ComposeplateCoordinator {
    private final PropertyModel mModel;
    private final boolean mHideIncognitoButton;

    /**
     * Constructs a new ComposeplateCoordinator.
     *
     * @param parentView The parent {@link ViewGroup} for the composeplate.
     */
    public ComposeplateCoordinator(ViewGroup parentView) {
        mModel = new PropertyModel(ComposeplateProperties.ALL_KEYS);
        View view = parentView.findViewById(R.id.composeplate_view);
        PropertyModelChangeProcessor.create(mModel, view, ComposeplateViewBinder::bind);
        mHideIncognitoButton = ChromeFeatureList.sAndroidComposeplateHideIncognitoButton.getValue();
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
        mModel.set(
                ComposeplateProperties.IS_INCOGNITO_BUTTON_VISIBLE,
                visible && !mHideIncognitoButton);
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
}
