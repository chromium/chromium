// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.miniplayer;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.readaloud.PlayerState;
import org.chromium.chrome.browser.readaloud.miniplayer.MiniPlayerCoordinator.Observer;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator class responsible for controlling Read Aloud mini player. */
class MiniPlayerMediator {
    private final PropertyModel mModel;
    private final Observer mObserver;

    MiniPlayerMediator(PropertyModel model, Observer observer) {
        mModel = model;
        mObserver = observer;
        mModel.set(
                MiniPlayerProperties.ON_CLOSE_CLICK_KEY, (view) -> { mObserver.onCloseClicked(); });
        mModel.set(MiniPlayerProperties.ON_EXPAND_CLICK_KEY,
                (view) -> { mObserver.onExpandRequested(); });
    }

    @PlayerState
    int getState() {
        return mModel.get(MiniPlayerProperties.PLAYER_STATE_KEY);
    }

    void show(boolean animate, @Nullable Playback playback) {
        mModel.set(MiniPlayerProperties.ANIMATE_KEY, animate);
        mModel.set(MiniPlayerProperties.VIEW_VISIBILITY_KEY, View.VISIBLE);
    }

    void dismiss(boolean animate) {
        mModel.set(MiniPlayerProperties.ANIMATE_KEY, animate);
        mModel.set(MiniPlayerProperties.VIEW_VISIBILITY_KEY, View.GONE);
    }
}
