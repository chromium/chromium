// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.chrome.modules.readaloud.Player.Delegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

public class ExpandedPlayerCoordinator {
    private final Context mContext;
    private final Delegate mDelegate;
    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                private BottomSheetContent mTrackedContent;

                @Override
                public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
                    if (mTrackedContent == mSheetContent && newContent != mSheetContent) {
                        mMediator.setVisibility(VisibilityState.GONE);
                    }
                    mTrackedContent = newContent;
                }

                @Override
                public void onSheetOpened(@StateChangeReason int reason) {
                    if (mTrackedContent == mSheetContent) {
                        mMediator.setVisibility(VisibilityState.VISIBLE);
                    }
                }

                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    if (mSheetContent != null) {
                        mSheetContent.notifySheetClosed();
                    }
                }
            };
    private PropertyModel mModel;
    private ExpandedPlayerSheetContent mSheetContent;
    private PropertyModelChangeProcessor<PropertyModel, ExpandedPlayerSheetContent, PropertyKey>
            mModelChangeProcessor;
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
        mContext = context;
        mDelegate = delegate;
        mModel = model;
        mMediator = mediator;
        mSheetContent = content;
        mDelegate.getBottomSheetController().addObserver(mBottomSheetObserver);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mSheetContent, ExpandedPlayerViewBinder::bind);
    }

    public void show() {
        mMediator.show();
    }

    public void dismiss() {
        if (mMediator != null) {
            mMediator.dismiss();
        }
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
}
