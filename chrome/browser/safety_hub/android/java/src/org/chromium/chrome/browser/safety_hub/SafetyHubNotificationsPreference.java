// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils.buildMenuListItem;

import android.content.Context;

import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

public class SafetyHubNotificationsPreference extends ChromeBasePreference
        implements ListMenu.Delegate {
    static interface MenuClickListener {
        void onBlockClicked(SafetyHubNotificationsPreference preference);

        void onAllowClicked(SafetyHubNotificationsPreference preference);

        void onAskClicked(SafetyHubNotificationsPreference preference);
    }

    private static final int MENU_BLOCK_NOTIFICATIONS_ITEM_ID = 0;
    private static final int MENU_ALLOW_NOTIFICATIONS_ITEM_ID = 1;
    private static final int MENU_ASK_NOTIFICATIONS_ITEM_ID = 2;

    private final NotificationPermissions mNotificationPermissions;
    private MenuClickListener mMenuClickListener;

    SafetyHubNotificationsPreference(
            Context context, NotificationPermissions notificationPermissions) {
        super(context);

        mNotificationPermissions = notificationPermissions;

        setTitle(mNotificationPermissions.getPrimaryPattern());
        int notificationCount = mNotificationPermissions.getNotificationCount();
        setSummary(
                getContext()
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_notifications_sublabel,
                                notificationCount,
                                notificationCount));

        setSelectable(false);
        setDividerAllowedBelow(false);
        setWidgetLayoutResource(R.layout.safety_hub_list_menu_widget);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        ListMenuButton listMenu = (ListMenuButton) holder.findViewById(R.id.button);
        listMenu.setDelegate(() -> getListMenu());
    }

    @Override
    public void onItemSelected(PropertyModel item) {
        if (mMenuClickListener == null) {
            return;
        }

        int itemId = item.get(ListMenuItemProperties.MENU_ITEM_ID);
        switch (itemId) {
            case MENU_BLOCK_NOTIFICATIONS_ITEM_ID:
                mMenuClickListener.onBlockClicked(this);
                break;
            case MENU_ALLOW_NOTIFICATIONS_ITEM_ID:
                mMenuClickListener.onAllowClicked(this);
                break;
            case MENU_ASK_NOTIFICATIONS_ITEM_ID:
                mMenuClickListener.onAskClicked(this);
                break;
            default:
                assert false : "Not a valid menu item Id.";
        }
    }
    ;

    NotificationPermissions getNotificationsPermissions() {
        return mNotificationPermissions;
    }

    void setMenuClickListener(MenuClickListener menuClickListener) {
        mMenuClickListener = menuClickListener;
    }

    private ListMenu getListMenu() {
        ModelList listItems = new ModelList();
        listItems.add(
                buildMenuListItem(
                        R.string.safety_hub_block_notifications_menu_item,
                        MENU_BLOCK_NOTIFICATIONS_ITEM_ID,
                        0));
        listItems.add(
                buildMenuListItem(
                        R.string.safety_hub_allow_notifications_menu_item,
                        MENU_ALLOW_NOTIFICATIONS_ITEM_ID,
                        0));
        listItems.add(
                buildMenuListItem(
                        R.string.safety_hub_ask_notifications_menu_item,
                        MENU_ASK_NOTIFICATIONS_ITEM_ID,
                        0));
        return BrowserUiListMenuUtils.getBasicListMenu(getContext(), listItems, this);
    }
}
