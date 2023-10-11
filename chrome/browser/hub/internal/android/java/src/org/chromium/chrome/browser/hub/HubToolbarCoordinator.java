// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.view.View;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Sets up the component that handles the toolbar of the Hub. */
public class HubToolbarCoordinator {
    private final HubToolbarMediator mMediator;
    private final HubToolbarView mView;

    /** Eagerly creates the component, but will not be rooted in the view tree yet. */
    public HubToolbarCoordinator(Context context) {
        PropertyModel model = new PropertyModel.Builder(HubToolbarProperties.ALL_KEYS).build();
        mView = new HubToolbarView(context);
        PropertyModelChangeProcessor.create(model, mView, HubToolbarViewBinder::bind);
        mMediator = new HubToolbarMediator(model);
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
