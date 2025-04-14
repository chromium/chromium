// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.ui.listmenu.BasicListMenu.ListMenuItemType.MENU_ITEM;
import static org.chromium.ui.listmenu.BasicListMenu.buildMenuDivider;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.START_ICON_BITMAP;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;

import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * A converter from (<a
 * href="https://source.chromium.org/chromium/chromium/src/+/main:ui/base/models/menu_model.h">C++
 * MenuModel</a>) to a Java list of {@link ListItem} to be used in context menus.
 */
@NullMarked
public class MenuModelBridge {

    private final long mNativePtr;
    private final boolean mIsIncognito;
    private final List<ListItem> mItems = new ArrayList<>();

    @CalledByNative
    private static MenuModelBridge create(long nativePtr, boolean isIncognito) {
        return new MenuModelBridge(nativePtr, isIncognito);
    }

    /**
     * {@return A {@link MenuModelBridge} instance.}
     *
     * @param nativePtr The {@link Long} address of the MenuModelBridge on the C++ side.
     */
    private MenuModelBridge(long nativePtr, boolean isIncognito) {
        mNativePtr = nativePtr;
        mIsIncognito = isIncognito;
    }

    /** {@return The list of {@link ListItem} held by this {@link MenuModelBridge}.} */
    public List<ListItem> getListItems() {
        return mItems;
    }

    /**
     * Adds a context menu item which triggers a command when activated.
     *
     * @param label The label to display.
     * @param bitmap The icon to display (or null if there should be no icon).
     * @param isEnabled Whether the command is enabled.
     * @param callback The callback to run when the command is activated.
     */
    @CalledByNative
    private void addCommand(
            String label, @Nullable Bitmap bitmap, boolean isEnabled, Runnable callback) {
        PropertyModel.Builder modelBuilder =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(TITLE, label)
                        .with(START_ICON_BITMAP, bitmap)
                        .with(ENABLED, isEnabled)
                        .with(CLICK_LISTENER, (view) -> callback.run());
        mItems.add(new ListItem(MENU_ITEM, modelBuilder.build()));
    }

    /** Adds a divider to the context menu. */
    @CalledByNative
    private void addDivider() {
        mItems.add(buildMenuDivider(mIsIncognito));
    }
}
