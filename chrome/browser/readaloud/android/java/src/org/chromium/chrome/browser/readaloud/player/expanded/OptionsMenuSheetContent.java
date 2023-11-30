// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.readaloud.player.InteractionHandler;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Bottom sheet content for Read Aloud expanded player advanced options menu. */
class OptionsMenuSheetContent extends MenuSheetContent {
    private static final String TAG = "ReadAloudOptions";
    private final Context mContext;
    private final PropertyModel mModel;
    private VoiceMenuSheetContent mVoiceSheet;
    private InteractionHandler mHandler;

    @IntDef({Item.VOICE, Item.TRANSLATE, Item.HIGHLIGHT})
    @Retention(RetentionPolicy.SOURCE)
    @interface Item {
        int VOICE = 0;
        int TRANSLATE = 1;
        int HIGHLIGHT = 2;
    }

    OptionsMenuSheetContent(
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
    OptionsMenuSheetContent(
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
        Resources res = mContext.getResources();

        mMenu.addItem(
                Item.VOICE,
                R.drawable.graphic_eq_24,
                res.getString(R.string.readaloud_voice_menu_title),
                MenuItem.Action.EXPAND,
                res.getString(R.string.readaloud_voice_menu_title));
        mMenu.addItem(
                Item.HIGHLIGHT,
                R.drawable.format_ink_highlighter_24,
                res.getString(R.string.readaloud_highlight_toggle_name),
                MenuItem.Action.TOGGLE,
                res.getString(R.string.readaloud_highlight_toggle_name));

        mMenu.setItemClickHandler(this::onClick);
    }

    void setInteractionHandler(InteractionHandler handler) {
        mHandler = handler;
        mMenu.getItem(Item.HIGHLIGHT)
                .setToggleHandler(
                        (value) -> {
                            handler.onHighlightingChange(value);
                        });
    }

    void setHighlightingSupported(boolean supported) {
        mMenu.getItem(Item.HIGHLIGHT).setItemEnabled(supported);
    }

    void setHighlightingEnabled(boolean enabled) {
        mMenu.getItem(Item.HIGHLIGHT).setValue(enabled);
    }

    @Override
    public void notifySheetClosed(BottomSheetContent closingContent) {
        super.notifySheetClosed(closingContent);
        if (closingContent == mVoiceSheet && mHandler != null) {
            mHandler.onVoiceMenuClosed();
        }
        if (mVoiceSheet != null) {
            mVoiceSheet.notifySheetClosed(closingContent);
        }
    }

    @Nullable
    VoiceMenuSheetContent getVoiceMenu() {
        return mVoiceSheet;
    }

    private void onClick(int itemId) {
        switch (itemId) {
            case Item.VOICE:
                if (mVoiceSheet == null) {
                    mVoiceSheet =
                            new VoiceMenuSheetContent(
                                    mContext,
                                    /* parent= */ this,
                                    getBottomSheetController(),
                                    mModel);
                }
                openSheet(mVoiceSheet);
                break;

            case Item.HIGHLIGHT:
                // Nothing to do here, MenuItem converts click into a toggle for which a
                // listener is registered elsewhere.
                break;
        }
    }
}
