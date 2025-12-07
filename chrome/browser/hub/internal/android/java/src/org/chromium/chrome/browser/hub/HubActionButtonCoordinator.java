// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubActionButtonProperties.ACTION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.hub.HubColorMixer.COLOR_MIXER;

import android.view.View;
import android.widget.Button;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Sets up the component that handles the action button in the Hub toolbar. */
@NullMarked
public class HubActionButtonCoordinator {

    private final HubActionButtonMediator mMediator;
    private final Button mActionButton;

    /**
     * Eagerly creates the action button component.
     *
     * @param actionButton The action button view for this component.
     * @param containerView The container view that will host the action button.
     * @param paneManager Interact with the current and all {@link Pane}s.
     * @param hubColorMixer Mixes the Hub Overview Color.
     */
    public HubActionButtonCoordinator(
            Button actionButton,
            View containerView,
            PaneManager paneManager,
            HubColorMixer hubColorMixer) {
        mActionButton = actionButton;

        // Set up touch delegate for the button after container layout is complete
        containerView.post(
                () -> {
                    containerView.setTouchDelegate(
                            HubActionButtonHelper.createTouchDelegate(mActionButton));
                });

        PropertyModel model =
                new PropertyModel.Builder(HubActionButtonProperties.ALL_ACTION_BUTTON_KEYS)
                        .with(COLOR_MIXER, hubColorMixer)
                        .with(ACTION_BUTTON_VISIBLE, true)
                        .build();
        PropertyModelChangeProcessor.create(model, actionButton, HubActionButtonViewBinder::bind);
        mMediator = new HubActionButtonMediator(model, paneManager);
    }

    /**
     * Updates the visibility of the action button.
     *
     * @param visible Whether the action button should be visible.
     */
    public void onActionButtonVisibilityChange(@Nullable Boolean visible) {
        mMediator.onActionButtonVisibilityChange(visible);
    }

    /** Cleans up observers and resources. */
    public void destroy() {
        mMediator.destroy();
    }
}
