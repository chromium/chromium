// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.readaloud.player.InteractionHandler;
import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/** Bottom sheet content for Read Aloud expanded player speed options menu. */
class SpeedMenuSheetContent extends MenuSheetContent {
    private static final String TAG = "ReadAloudSpeed";
    private final Context mContext;
    private final PropertyModel mModel;
    private InteractionHandler mInteractionHandler;
    private float[] mSpeeds = {0.5f, 0.8f, 1.0f, 1.2f, 1.5f, 2.0f, 3.0f, 4.0f};

    SpeedMenuSheetContent(
            Context context,
            BottomSheetContent parent,
            BottomSheetController bottomSheetController,
            PropertyModel model) {
        super(context, parent, bottomSheetController, R.string.readaloud_options_menu_title);
        mContext = context;
        mModel = model;
        setUp();
    }

    @VisibleForTesting
    SpeedMenuSheetContent(
            Context context,
            BottomSheetContent parent,
            BottomSheetController bottomSheetController,
            Menu menu,
            PropertyModel model) {
        super(context, parent, bottomSheetController, R.string.readaloud_options_menu_title, menu);
        mContext = context;
        mModel = model;
        setUp();
    }

    private void setUp() {
        float currentSpeed = mModel.get(PlayerProperties.SPEED);
        for (int i = 0; i < mSpeeds.length; i++) {
            String speedString =
                    mContext.getResources()
                            .getString(R.string.readaloud_speed, speedFormatter(mSpeeds[i]));
            MenuItem item = mMenu.addItem(i, 0, speedString, MenuItem.Action.RADIO, speedString);
            if (mSpeeds[i] == currentSpeed) {
                item.setValue(true);
            }
        }
    }

    void setInteractionHandler(InteractionHandler handler) {
        mInteractionHandler = handler;
        mMenu.setRadioTrueHandler(
                (itemId) -> {
                    handler.onSpeedChange(mSpeeds[itemId]);
                });
    }

    public static String speedFormatter(float speed) {
        return String.format(((speed % 1 < 0.01) ? "%.0f" : "%.1f"), speed);
    }
}
