// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A mediator for the TabGridIPHItem component, responsible for communicating
 * with the components' coordinator as well as managing the business logic
 * for IPH item show/hide.
 */
class TabGridIphItemMediator implements TabSwitcherMediator.IphProvider {
    private PropertyModel mModel;
    private Profile mProfile;

    TabGridIphItemMediator(PropertyModel model, Profile profile) {
        mModel = model;
        mProfile = profile;
        setupOnClickListeners();
        setupScrimViewObserver();
    }

    private void setupOnClickListeners() {
        View.OnClickListener showIPHOnClickListener = view -> {
            mModel.set(TabGridIphItemProperties.IS_IPH_DIALOG_VISIBLE, true);
        };
        mModel.set(
                TabGridIphItemProperties.IPH_ENTRANCE_SHOW_BUTTON_LISTENER, showIPHOnClickListener);

        View.OnClickListener closeIPHDialogOnClickListener = view -> {
            mModel.set(TabGridIphItemProperties.IS_IPH_DIALOG_VISIBLE, false);
        };
        mModel.set(TabGridIphItemProperties.IPH_DIALOG_CLOSE_BUTTON_LISTENER,
                closeIPHDialogOnClickListener);

        View.OnClickListener closeIPHEntranceOnClickListener = view -> {
            mModel.set(TabGridIphItemProperties.IS_IPH_ENTRANCE_VISIBLE, false);
            // Dismiss the drag-and-drop IPH feature when user explicitly closes the entrance.
            dismissIPHFeature();
        };
        mModel.set(TabGridIphItemProperties.IPH_ENTRANCE_CLOSE_BUTTON_LISTENER,
                closeIPHEntranceOnClickListener);
    }

    private void setupScrimViewObserver() {
        ScrimView.ScrimObserver observer = new ScrimView.ScrimObserver() {
            @Override
            public void onScrimClick() {
                mModel.set(TabGridIphItemProperties.IS_IPH_DIALOG_VISIBLE, false);
            }

            @Override
            public void onScrimVisibilityChanged(boolean visible) {}
        };
        mModel.set(TabGridIphItemProperties.IPH_SCRIM_VIEW_OBSERVER, observer);
    }

    private void dismissIPHFeature() {
        final Tracker tracker = TrackerFactory.getTrackerForProfile(mProfile);
        tracker.shouldTriggerHelpUI(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE);
        tracker.dismissed(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE);
    }

    @Override
    public void maybeShowIPH(boolean isIncognito) {
        final Tracker tracker = TrackerFactory.getTrackerForProfile(mProfile);
        boolean isVisible =
                tracker.wouldTriggerHelpUI(FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE);

        if (isVisible) mModel.set(TabGridIphItemProperties.IS_INCOGNITO, isIncognito);

        mModel.set(TabGridIphItemProperties.IS_IPH_ENTRANCE_VISIBLE, isVisible);
    }
}
