// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator of the tab sharing toolbar for each tab sharing session. */
@NullMarked
public class TabSharingToolbarCoordinator {
    private final View mView;
    private final PropertyModelChangeProcessor mModelChangeProcessor;
    private final TabSharingToolbarMediator mMediator;

    /**
     * Initializes a new toolbar instance by inflating its layout and setting up the MVC components.
     *
     * @param context The Android context.
     * @param bridge The bridge to the native tab sharing UI.
     * @param tabProvider The provider of the current tab.
     */
    public TabSharingToolbarCoordinator(
            Context context, TabSharingUiBridge bridge, ActivityTabProvider tabProvider) {
        mView = LayoutInflater.from(context).inflate(R.layout.tab_sharing_toolbar, null);

        PropertyModel model =
                new PropertyModel.Builder(TabSharingToolbarProperties.ALL_KEYS).build();
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        model, mView, TabSharingToolbarViewBinder::bind);

        mMediator = new TabSharingToolbarMediator(context, model, bridge, tabProvider);
    }

    public void destroy() {
        mModelChangeProcessor.destroy();
        mMediator.destroy();
    }

    public View getView() {
        return mView;
    }
}
