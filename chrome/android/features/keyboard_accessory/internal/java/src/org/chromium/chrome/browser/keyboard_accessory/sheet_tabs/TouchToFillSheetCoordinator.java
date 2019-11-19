// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.content.Context;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.RecyclerView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece.Type;

/**
 * This component is a tab that can be added to the ManualFillingCoordinator. This tab
 * allows selecting credentials from a sheet below the keyboard accessory which will be filled when
 * tapped.
 */
public class TouchToFillSheetCoordinator extends AccessorySheetTabCoordinator {
    private AccessorySheetTabModel mModel = new AccessorySheetTabModel();
    private final AccessorySheetTabMediator mMediator =
            new AccessorySheetTabMediator(mModel, AccessoryTabType.TOUCH_TO_FILL,
                    Type.TOUCH_TO_FILL_INFO, AccessoryAction.MANAGE_PASSWORDS);

    /**
     * Creates the touch to fill tab.
     *
     * @param context        The {@link Context} containing resources like icons and layouts for
     *                       this tab.
     * @param scrollListener An optional listener that will be bound to an inflated recycler
     *                       view.
     */
    public TouchToFillSheetCoordinator(
            Context context, @Nullable RecyclerView.OnScrollListener scrollListener) {
        super(
                // TODO(crbug.com/957532): Add an appropriate title, icon, and restructure this
                // class to use an Icon Provider with a static instance of the resource.
                "Touch To Fill",
                AppCompatResources.getDrawable(context, R.drawable.ic_error_grey800_24dp_filled),
                // TODO(crbug.com/957532): Add strings and resources properly.
                // We pass an empty contentDescription, since the Touch to Fill sheet should not be
                // triggered by clicking an icon.
                "", "Touch to Fill sheet is open",
                // TODO(crbug.com/957532): Add a new layout, or generalize the existing one.
                R.layout.password_accessory_sheet, AccessoryTabType.TOUCH_TO_FILL, scrollListener);
    }

    @Override
    AccessorySheetTabMediator getMediator() {
        return mMediator;
    }
}
