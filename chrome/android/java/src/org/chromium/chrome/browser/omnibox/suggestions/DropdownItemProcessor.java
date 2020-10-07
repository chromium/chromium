// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import org.chromium.base.annotations.MockedInTests;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A processor of omnibox dropdown items.
 */
@MockedInTests
public interface DropdownItemProcessor {
    /**
     * @return The type of view the models created by this processor represent.
     */
    int getViewTypeId();

    /**
     * @return The minimum possible height of the view for this processor.
     */
    int getMinimumViewHeight();

    /**
     * Create a model for views managed by the processor.
     * @return A newly created model.
     */
    PropertyModel createModel();

    /**
     * @see org.chromium.chrome.browser.omnibox.UrlFocusChangeListener#onUrlFocusChange(boolean)
     */
    default void onUrlFocusChange(boolean hasFocus) {}

    /**
     * Signals that native initialization has completed.
     */
    default void onNativeInitialized() {}

    /**
     * Record suggestion impressions for this processor.
     * Purpose of this function is bookkeeping of presented views at the time user finishes
     * interacting with omnibox (whether navigating somewhere, turning off screen, leaving omnibox
     * or closing the app).
     * This call is invoked once for every model created by the processor.
     */
    default void recordItemPresented(PropertyModel model) {}

    /**
     * Signals that the dropdown list is about to be populated with new content.
     */
    default void onSuggestionsReceived() {}
}
