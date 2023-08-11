// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.expandedplayer;

import android.content.Context;

import org.chromium.chrome.modules.readaloud.ExpandedPlayer;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

public class ExpandedPlayerCoordinator implements ExpandedPlayer {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private ExpandedPlayerSheetContent mSheetContent;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor<PropertyModel, ExpandedPlayerSheetContent, PropertyKey>
            mModelChangeProcessor;
    private ExpandedPlayerMediator mMediator;

    public ExpandedPlayerCoordinator(Context context, BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
    }

    @Override
    public void show() {
        if (mSheetContent == null) {
            mSheetContent = new ExpandedPlayerSheetContent(mContext, mBottomSheetController);
            mModel = new PropertyModel.Builder(ExpandedPlayerProperties.ALL_KEYS)
                             .with(ExpandedPlayerProperties.STATE_KEY, ExpandedPlayer.State.GONE)
                             .build();
            mModelChangeProcessor = PropertyModelChangeProcessor.create(
                    mModel, mSheetContent, ExpandedPlayerViewBinder::bind);
            mMediator = new ExpandedPlayerMediator(mBottomSheetController, mModel);
        }
        mMediator.show();
    }
}
