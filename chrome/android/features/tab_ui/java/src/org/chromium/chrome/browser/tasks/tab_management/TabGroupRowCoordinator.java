// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.LayoutInflater;

import androidx.annotation.NonNull;

import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** A coordinator for TabGroupRow component to display a single row of tab group. */
public class TabGroupRowCoordinator {
    private final TabGroupRowView mView;

    /**
     * Constructor for TabGroupRowCoordinator component.
     *
     * @param context The Android context used to inflate the views.
     * @param savedTabGroup The state of the tab group.
     * @param faviconResolver Used to fetch favicon images for tabs.
     */
    public TabGroupRowCoordinator(
            Context context, SavedTabGroup savedTabGroup, FaviconResolver faviconResolver) {
        PropertyModel model =
                TabGroupRowMediator.buildModel(
                        savedTabGroup,
                        faviconResolver,
                        /* openRunnable= */ null,
                        /* deleteRunnable= */ null);
        mView =
                (TabGroupRowView)
                        LayoutInflater.from(context).inflate(R.layout.tab_group_row, null);

        PropertyModelChangeProcessor.create(model, mView, new TabGroupRowViewBinder());
    }

    /** Get the view component of TabGroupRowCoordinator. */
    public @NonNull TabGroupRowView getView() {
        return mView;
    }
}
