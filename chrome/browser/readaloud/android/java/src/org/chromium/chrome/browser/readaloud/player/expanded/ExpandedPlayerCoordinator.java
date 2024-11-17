// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import android.content.Context;
import android.content.res.Configuration;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.readaloud.player.InteractionHandler;
import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.chrome.modules.readaloud.Player.Delegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

public class ExpandedPlayerCoordinator implements ConfigurationChangedObserver {
    private final Delegate mDelegate;
    private boolean mSheetVisible;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                private BottomSheetContent mTrackedContent;

                @Override
                public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
                    // Other than tracking the visibility of the expanded sheet, this also tracks
                    // if other, non-ReadAloud sheet are displayed in order to hide the mini player.
                    if (mTrackedContent == mSheetContent && newContent != mSheetContent) {
                        mMediator.setVisibility(VisibilityState.GONE);
                        mMediator.setShowMiniPlayerOnDismiss(true);
                    } else if (!isReadAloudSecondarySheet(newContent)) {
                        mMediator.setShowMiniPlayerOnDismiss(true);
                    }

                    // If showing the player again, resume player UI updates.
                    if (newContent == mSheetContent) {
                        mMediator.setHiddenAndPlaying(false);
                    }

                    mTrackedContent = newContent;
                }

                @Override
                public void onSheetOpened(@StateChangeReason int reason) {
                    mSheetVisible = true;

                    InteractionHandler handler = mModel.get(PlayerProperties.INTERACTION_HANDLER);
                    if (handler != null) {
                        handler.onShouldHideMiniPlayer();
                    }

                    if (mTrackedContent == mSheetContent) {
                        mMediator.setVisibility(VisibilityState.VISIBLE);
                        mMediator.setHiddenAndPlaying(false);
                    }
                }

                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    mSheetVisible = false;
                    InteractionHandler handler = mModel.get(PlayerProperties.INTERACTION_HANDLER);
                    // null only in tests
                    if (mSheetContent != null) {
                        BottomSheetContent closingSheet =
                                mDelegate.getBottomSheetController().getCurrentSheetContent();
                        mSheetContent.notifySheetClosed(closingSheet);
                        // If we're dismissing for a reason other than showing a menu sheet, notify
                        // about closing.
                        if (!isReadAloudSecondarySheet(closingSheet)
                                && mMediator.getShowMiniPlayerOnDismiss()
                                && handler != null) {
                            handler.onShouldRestoreMiniPlayer();
                        }
                    }
                }

                private boolean isReadAloudSecondarySheet(@Nullable BottomSheetContent content) {
                    return (content != null
                            && (content instanceof OptionsMenuSheetContent
                                    || content instanceof SpeedMenuSheetContent));
                }
            };
    private PropertyModel mModel;
    private ExpandedPlayerSheetContent mSheetContent;
    private ExpandedPlayerMediator mMediator;

    public ExpandedPlayerCoordinator(Context context, Delegate delegate, PropertyModel model) {
        this(
                context,
                delegate,
                model,
                new ExpandedPlayerMediator(model),
                new ExpandedPlayerSheetContent(
                        context, delegate.getBottomSheetController(), model));
    }

    @VisibleForTesting
    ExpandedPlayerCoordinator(
            Context context,
            Delegate delegate,
            PropertyModel model,
            ExpandedPlayerMediator mediator,
            ExpandedPlayerSheetContent content) {
        mDelegate = delegate;
        mModel = model;
        mMediator = mediator;
        mSheetContent = content;
        mDelegate.getBottomSheetController().addObserver(mBottomSheetObserver);
        PropertyModelChangeProcessor.create(mModel, mSheetContent, ExpandedPlayerViewBinder::bind);
    }

    public void show() {
        mMediator.show();
    }

    public void dismiss() {
        dismiss(/* showMiniPlayer= */ false);
    }

    public void dismiss(boolean showMiniPlayer) {
        if (mMediator != null) {
            mMediator.setShowMiniPlayerOnDismiss(showMiniPlayer);
            mMediator.dismiss();
        }
    }

    /** Returns true if a bottom sheet is currently visible. */
    public boolean anySheetShowing() {
        return mSheetVisible;
    }

    public @VisibilityState int getVisibility() {
        if (mMediator == null) {
            return VisibilityState.GONE;
        }
        return mMediator.getVisibility();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    void setSheetContent(ExpandedPlayerSheetContent sheetContent) {
        mSheetContent = sheetContent;
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        mSheetContent.onOrientationChange(newConfig.orientation);
    }
}
