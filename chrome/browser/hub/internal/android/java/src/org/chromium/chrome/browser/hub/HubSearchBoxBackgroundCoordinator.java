// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator of the Hub's Search Box Background. */
public class HubSearchBoxBackgroundCoordinator {
    private final @NonNull PropertyModel mModel;

    /**
     * Creates the {@link HubSearchBoxBackgroundCoordinator}.
     *
     * @param containerView The view that the hub is attached to.
     */
    public HubSearchBoxBackgroundCoordinator(@NonNull FrameLayout containerView) {
        View backgroundView = containerView.findViewById(R.id.search_box_background);
        mModel = HubSearchBoxBackgroundProperties.create();
        PropertyModelChangeProcessor.create(
                mModel, backgroundView, HubSearchBoxBackgroundViewBinder::bind);
    }

    /** Sets whether the background should show. */
    public void setShouldShowBackground(boolean shouldShow) {
        mModel.set(HubSearchBoxBackgroundProperties.SHOW_BACKGROUND, shouldShow);
    }

    /** Sets the background color scheme for hub search. */
    public void setBackgroundColorScheme(@HubColorScheme int colorScheme) {
        // TODO(crbug.com/378124473): Instead of using the background color, use the current
        // tab's toolbar color for a more seamless animation transition. This will involve modifying
        // the model to take in a ColorInt propagated from the toolbar rather than use the
        // HubColorScheme to determine what color to set.
        mModel.set(HubSearchBoxBackgroundProperties.COLOR_SCHEME, colorScheme);
    }
}
