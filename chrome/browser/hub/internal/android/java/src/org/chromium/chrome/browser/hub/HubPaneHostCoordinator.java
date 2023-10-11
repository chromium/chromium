// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.view.View;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Sets up the component that holds a single pane at a time in the Hub. */
public class HubPaneHostCoordinator {
    private final HubPaneHostMediator mMediator;
    private final HubPaneHostView mView;

    /** Eagerly creates the component, but will not be rooted in the view tree yet. */
    public HubPaneHostCoordinator(Context context) {
        PropertyModel model = new PropertyModel.Builder(HubPaneHostProperties.ALL_KEYS).build();
        mView = new HubPaneHostView(context);
        PropertyModelChangeProcessor.create(model, mView, HubPaneHostViewBinder::bind);
        mMediator = new HubPaneHostMediator(model);
    }

    /** Returns the top level view for this component that caller can add to the view tree. */
    public View getView() {
        return mView;
    }

    /** Cleans up observers and resources. */
    public void destroy() {
        mMediator.destroy();
    }
}
