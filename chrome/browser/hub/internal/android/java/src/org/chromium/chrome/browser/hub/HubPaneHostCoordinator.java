// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Sets up the component that holds a single pane at a time in the Hub. */
public class HubPaneHostCoordinator {
    private final HubPaneHostMediator mMediator;

    /**
     * Eagerly creates the component, but will not be rooted in the view tree yet.
     *
     * @param hubPaneHostView The root view of this component. Inserted into hierarchy for us.
     * @param paneSupplier A way to observe and get the current {@link Pane}.
     */
    public HubPaneHostCoordinator(
            HubPaneHostView hubPaneHostView, ObservableSupplier<Pane> paneSupplier) {
        PropertyModel model = new PropertyModel.Builder(HubPaneHostProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(model, hubPaneHostView, HubPaneHostViewBinder::bind);
        mMediator = new HubPaneHostMediator(model, paneSupplier);
    }

    /** Cleans up observers and resources. */
    public void destroy() {
        mMediator.destroy();
    }
}
