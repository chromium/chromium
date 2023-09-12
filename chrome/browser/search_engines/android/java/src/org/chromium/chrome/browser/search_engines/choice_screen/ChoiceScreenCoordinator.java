// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for search engine choice screen UIs. It implements the UI component for the choice
 * screen itself, and is intended to be embedded into a coordinator that will be responsible for the
 * context in which to show the screen, for example in a dialog or in a fragment.
 */
class ChoiceScreenCoordinator {
    private final ChoiceScreenView mChoiceScreenView;

    /**
     * Constructs the choice screen.
     *
     * @param activity Activity in which the screen will be displayed.
     * @param delegate Provides access to the data needed to display the screen and persist the
     *                 results.
     */
    ChoiceScreenCoordinator(Activity activity, ChoiceScreenDelegate delegate) {
        PropertyModel propertyModel = ChoiceScreenProperties.createPropertyModel();

        mChoiceScreenView = (ChoiceScreenView) LayoutInflater.from(activity).inflate(
                org.chromium.chrome.browser.search_engines.R.layout.search_engine_choice_view,
                /*root=*/null);

        PropertyModelChangeProcessor.create(
                propertyModel, mChoiceScreenView, ChoiceScreenViewBinder::bindContentView);

        new ChoiceScreenMediator(propertyModel, delegate);
    }

    /** Returns the {@link View} representing the choice screen. */
    View getContentView() {
        return mChoiceScreenView;
    }
}
