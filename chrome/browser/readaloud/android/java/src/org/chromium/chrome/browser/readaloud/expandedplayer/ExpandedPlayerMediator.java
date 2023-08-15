// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.expandedplayer;

import org.chromium.chrome.browser.readaloud.PlayerState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator class responsible for controlling Read Aloud mini player. */
public class ExpandedPlayerMediator extends EmptyBottomSheetObserver {
    private final BottomSheetController mBottomSheetController;
    private final PropertyModel mModel;

    public ExpandedPlayerMediator(
            BottomSheetController bottomSheetController, PropertyModel model) {
        mBottomSheetController = bottomSheetController;
        mBottomSheetController.addObserver(this);
        mModel = model;
    }

    public void destroy() {
        mBottomSheetController.removeObserver(this);
    }

    public void show() {
        @PlayerState
        int state = getState();
        if (state == PlayerState.SHOWING || state == PlayerState.VISIBLE) {
            return;
        }
        setState(PlayerState.SHOWING);
    }

    public void dismiss() {
        @PlayerState
        int state = getState();
        if (state == PlayerState.GONE || state == PlayerState.HIDING) {
            return;
        }
        setState(PlayerState.HIDING);
    }

    public @PlayerState int getState() {
        return mModel.get(ExpandedPlayerProperties.STATE_KEY);
    }

    // from EmptyBottomSheetObserver
    @Override
    public void onSheetOpened(@StateChangeReason int reason) {
        setState(PlayerState.VISIBLE);
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        setState(PlayerState.GONE);
    }

    private void setState(@PlayerState int state) {
        mModel.set(ExpandedPlayerProperties.STATE_KEY, state);
    }
}
