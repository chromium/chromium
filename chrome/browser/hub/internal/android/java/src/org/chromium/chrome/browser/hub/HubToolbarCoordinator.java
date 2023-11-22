// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

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
     */
    public HubToolbarCoordinator(HubToolbarView hubToolbarView, PaneManager paneManager) {
        PropertyModel model = new PropertyModel.Builder(HubToolbarProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(model, hubToolbarView, HubToolbarViewBinder::bind);
        mMediator = new HubToolbarMediator(model, paneManager);
    }

    /** Cleans up observers and resources. */
    public void destroy() {
        mMediator.destroy();
    }
}
