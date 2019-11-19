// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * A coordinator for TabStripToolbar component.
 */
public class TabStripToolbarCoordinator implements Destroyable {
    private final TabGroupUiToolbarView mToolbarView;
    private final PropertyModel mModel;
    private final PropertyModelChangeProcessor mModelChangeProcessor;

    TabStripToolbarCoordinator(Context context, ViewGroup parentView, PropertyModel model) {
        mModel = model;
        mToolbarView = (TabGroupUiToolbarView) LayoutInflater.from(context).inflate(
                R.layout.bottom_tab_strip_toolbar, parentView, false);

        parentView.addView(mToolbarView);

        mModelChangeProcessor = PropertyModelChangeProcessor.create(
                model, mToolbarView, TabGroupUiToolbarViewBinder::bind);
    }

    View getView() {
        return mToolbarView;
    }

    ViewGroup getTabListContainerView() {
        return mToolbarView.getViewContainer();
    }

    /**
     * Destroy any members that needs clean up.
     */
    @Override
    public void destroy() {
        mModelChangeProcessor.destroy();
    }
}
