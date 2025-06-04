// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the composeplate on the NTP. */
@NullMarked
public class ComposeplateCoordinator {
    private final PropertyModel mModel;

    /**
     * Constructs a new ComposeplateCoordinator.
     *
     * @param parentView The parent {@link ViewGroup} for the composeplate.
     */
    public ComposeplateCoordinator(ViewGroup parentView) {
        mModel = new PropertyModel(ComposeplateProperties.ALL_KEYS);
        View view = parentView.findViewById(R.id.composeplate_view);
        PropertyModelChangeProcessor.create(mModel, view, ComposeplateViewBinder::bind);
    }

    /**
     * Sets the visibility of the composeplate.
     *
     * @param visible Whether the composeplate should be visible.
     */
    public void setVisibility(boolean visible) {
        mModel.set(ComposeplateProperties.IS_VISIBLE, visible);
    }

    /**
     * Sets the click listener for the voice search button.
     *
     * @param voiceSearchClickListener The click listener for the voice search button.
     */
    public void setVoiceSearchClickListener(View.OnClickListener voiceSearchClickListener) {
        mModel.set(ComposeplateProperties.VOICE_SEARCH_CLICK_LISTENER, voiceSearchClickListener);
    }

    /**
     * Sets the click listener for the lens button.
     *
     * @param lensClickListener The click listener for the lens button.
     */
    public void setLensClickListener(View.OnClickListener lensClickListener) {
        mModel.set(ComposeplateProperties.LENS_CLICK_LISTENER, lensClickListener);
    }

    /**
     * Sets the click listener for the incognito button.
     *
     * @param incognitoClickListener The click listener for the incognito button.
     */
    public void setIncognitoClickListener(View.OnClickListener incognitoClickListener) {
        mModel.set(ComposeplateProperties.INCOGNITO_CLICK_LISTENER, incognitoClickListener);
    }
}
