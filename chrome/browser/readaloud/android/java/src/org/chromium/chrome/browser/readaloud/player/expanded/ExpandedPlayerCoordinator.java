// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import android.content.Context;

import androidx.annotation.Nullable;

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
                    // TODO: implement
                }

                @Override
                public void onSheetOpened(@StateChangeReason int reason) {
                    // TODO: implement
                }

                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    // TODO: notify ExpandedPlayerSheetContent of sheet closed
                }
            };
    private PropertyModel mModel;
    private ExpandedPlayerSheetContent mSheetContent;
    private PropertyModelChangeProcessor<PropertyModel, ExpandedPlayerSheetContent, PropertyKey>
            mModelChangeProcessor;
    private ExpandedPlayerMediator mMediator;

    public ExpandedPlayerCoordinator(Context context, Delegate delegate, PropertyModel model) {
        mContext = context;
        mDelegate = delegate;
        mModel = model;
        mDelegate.getBottomSheetController().addObserver(mBottomSheetObserver);
    }

    public void show() {
        if (mSheetContent == null) {
            makeSheetContent();
            mModelChangeProcessor =
                    PropertyModelChangeProcessor.create(
                            mModel, mSheetContent, ExpandedPlayerViewBinder::bind);
            makeMediator();
        }
        mMediator.show();
    }

    public void makeSheetContent() {
        mSheetContent =
                new ExpandedPlayerSheetContent(mContext, mDelegate.getBottomSheetController());
    }

    public void makeMediator() {
        mMediator = new ExpandedPlayerMediator(mModel);
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

    void setMediatorForTesting(ExpandedPlayerMediator mediator) {
        mMediator = mediator;
    }

    void setSheetContentForTesting(ExpandedPlayerSheetContent sheetContent) {
        mSheetContent = sheetContent;
    }
}
