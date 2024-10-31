// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.app.Activity;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Sets up the component that handles the toolbar of the Hub. */
public class HubToolbarCoordinator {
    private final HubToolbarMediator mMediator;

    /**
     * Eagerly creates the component, but will not be rooted in the view tree yet.
     *
     * @param hubToolbarView The root view of this component. Inserted into hierarchy for us.
     * @param paneManager Interact with the current and all {@link Pane}s.
     * @param menuButtonCoordinator Root component for the app menu.
     * @param tracker Used to record user engagement events.
     * @param searchActivityClient A client for the search activity, used to launch search.
     */
    public HubToolbarCoordinator(
            @NonNull Activity activity,
            @NonNull HubToolbarView hubToolbarView,
            @NonNull PaneManager paneManager,
            @NonNull MenuButtonCoordinator menuButtonCoordinator,
            @NonNull Tracker tracker,
            @NonNull SearchActivityClient searchActivityClient) {
        PropertyModel model = new PropertyModel.Builder(HubToolbarProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(model, hubToolbarView, HubToolbarViewBinder::bind);
        mMediator =
                new HubToolbarMediator(activity, model, paneManager, tracker, searchActivityClient);

        MenuButton menuButton = hubToolbarView.findViewById(R.id.menu_button_wrapper);
        menuButtonCoordinator.setMenuButton(menuButton);
    }

    /** Returns the button view for a given pane if present. */
    public View getPaneButton(@PaneId int paneId) {
        return mMediator.getButton(paneId);
    }

    /** Cleans up observers and resources. */
    public void destroy() {
        mMediator.destroy();
    }
}
