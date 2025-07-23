// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubBottomToolbarProperties.BOTTOM_TOOLBAR_VISIBLE;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator for the Hub bottom toolbar. This class handles the business logic and state management
 * for the bottom toolbar component.
 */
@NullMarked
public class HubBottomToolbarMediator {
    private final PropertyModel mPropertyModel;
    private final HubBottomToolbarDelegate mDelegate;
    private final Callback<Boolean> mOnVisibilityChange = this::onVisibilityChange;

    /**
     * Creates a new HubBottomToolbarMediator.
     *
     * @param propertyModel The property model to update with toolbar state.
     * @param delegate The delegate that provides visibility information.
     */
    public HubBottomToolbarMediator(
            PropertyModel propertyModel, HubBottomToolbarDelegate delegate) {
        mPropertyModel = propertyModel;
        mDelegate = delegate;

        mDelegate.getBottomToolbarVisibilitySupplier().addObserver(mOnVisibilityChange);
    }

    /** Cleans up observers and unregisters callbacks. */
    public void destroy() {
        mDelegate.getBottomToolbarVisibilitySupplier().removeObserver(mOnVisibilityChange);
    }

    private void onVisibilityChange(Boolean visible) {
        mPropertyModel.set(BOTTOM_TOOLBAR_VISIBLE, visible != null ? visible : false);
    }
}
