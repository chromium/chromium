// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the Shared Tab Group Notice Bottom Sheet. */
public class TabGroupShareNoticeBottomSheetCoordinator {

    private TabGroupShareNoticeBottomSheetView mView;

    interface TabGroupShareNoticeBottomSheetCoordinatorDelegate {
        /** Requests to show the bottom sheet content. */
        boolean requestShowContent();

        /** Hides the bottom sheet. */
        void hide(@StateChangeReason int hideReason);
    }

    private final BottomSheetController mBottomSheetController;
    private final Context mContext;
    private final Profile mProfile;

    /**
     * @param bottomSheetController The controller for the bottom sheet.
     * @param context The {@link Context} to attach the bottom sheet to.
     * @param profile The current user profile.
     */
    public TabGroupShareNoticeBottomSheetCoordinator(
            BottomSheetController bottomSheetController, Context context, Profile profile) {
        mBottomSheetController = bottomSheetController;
        mContext = context;
        mProfile = profile;
    }

    /** Initializes the delegate. */
    @VisibleForTesting
    @NonNull
    TabGroupShareNoticeBottomSheetCoordinatorDelegate initDelegate() {
        return new TabGroupShareNoticeBottomSheetCoordinatorDelegate() {
            @Override
            public boolean requestShowContent() {
                return mBottomSheetController.requestShowContent(mView, /* animate= */ true);
            }

            @Override
            public void hide(@StateChangeReason int hideReason) {
                mBottomSheetController.hideContent(mView, /* animate= */ true, hideReason);
            }
        };
    }

    /** Requests to show the bottom sheet content. */
    public void requestShowContent() {
        mView = new TabGroupShareNoticeBottomSheetView(mContext);

        TabGroupShareNoticeBottomSheetCoordinatorDelegate delegate = initDelegate();
        TabGroupShareNoticeBottomSheetMediator mMediator =
                new TabGroupShareNoticeBottomSheetMediator(
                        mBottomSheetController, delegate, mProfile);

        PropertyModelChangeProcessor.create(
                mMediator.getModel(), mView, TabGroupShareNoticeBottomSheetViewBinder::bind);
        mMediator.requestShowContent();
    }
}
