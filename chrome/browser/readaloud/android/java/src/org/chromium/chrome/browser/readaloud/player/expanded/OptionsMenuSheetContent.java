// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
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
                MenuItem.Action.EXPAND);
        mMenu.addItem(
                Item.TRANSLATE,
                R.drawable.translate_24,
                res.getString(R.string.readaloud_translate_menu_title),
                MenuItem.Action.EXPAND);
        mMenu.addItem(
                Item.HIGHLIGHT,
                R.drawable.format_ink_highlighter_24,
                res.getString(R.string.readaloud_highlight_toggle_name),
                MenuItem.Action.TOGGLE);

        mMenu.setItemClickHandler(this::onClick);
    }

    @Override
    public void notifySheetClosed() {
        super.notifySheetClosed();
    }

    private void onClick(int itemId) {
        switch (itemId) {
            case Item.VOICE:
                Log.i(TAG, "Voice menu item not implemented.");
                // TODO: open voice sheet
                break;

            case Item.TRANSLATE:
                Log.i(TAG, "Translate menu item not implemented.");
                // TODO: open translate sheet
                break;

            case Item.HIGHLIGHT:
                // Nothing to do here, MenuItem converts click into a toggle for which a
                // listener is registered elsewhere.
                break;
        }
    }
}
