// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils.buildMenuListItem;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.browser.settings.FaviconLoader;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.FaviconViewUtils;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

public class SafetyHubNotificationsPreference extends ChromeBasePreference
        implements ListMenu.Delegate {
    static interface MenuClickListener {
        void onAllowClicked(SafetyHubNotificationsPreference preference);

        void onResetClicked(SafetyHubNotificationsPreference preference);
    }

    private static final int MENU_RESET_NOTIFICATIONS_ITEM_ID = 0;
    private static final int MENU_ALLOW_NOTIFICATIONS_ITEM_ID = 1;

    private final NotificationPermissions mNotificationPermissions;
    private final LargeIconBridge mLargeIconBridge;
    private MenuClickListener mMenuClickListener;
    private boolean mFaviconFetched;

    SafetyHubNotificationsPreference(
            Context context,
            @NonNull NotificationPermissions notificationPermissions,
            @NonNull LargeIconBridge largeIconBridge) {
        super(context);

        mNotificationPermissions = notificationPermissions;
        mLargeIconBridge = largeIconBridge;

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

        ImageView icon = (ImageView) holder.findViewById(android.R.id.icon);
        FaviconViewUtils.formatIconForFavicon(getContext().getResources(), icon);

        if (!mFaviconFetched) {
            FaviconLoader.loadFavicon(
                    getContext(),
                    mLargeIconBridge,
                    new GURL(mNotificationPermissions.getPrimaryPattern()),
                    this::onFaviconAvailable);
            mFaviconFetched = true;
        }
    }

    @Override
    public void onItemSelected(PropertyModel item) {
        if (mMenuClickListener == null) {
            return;
        }

        int itemId = item.get(ListMenuItemProperties.MENU_ITEM_ID);
        switch (itemId) {
            case MENU_ALLOW_NOTIFICATIONS_ITEM_ID:
                mMenuClickListener.onAllowClicked(this);
                break;
            case MENU_RESET_NOTIFICATIONS_ITEM_ID:
                mMenuClickListener.onResetClicked(this);
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
                        R.string.safety_hub_reset_notifications_menu_item,
                        MENU_RESET_NOTIFICATIONS_ITEM_ID,
                        0));
        listItems.add(
                buildMenuListItem(
                        R.string.safety_hub_allow_notifications_menu_item,
                        MENU_ALLOW_NOTIFICATIONS_ITEM_ID,
                        0));
        return BrowserUiListMenuUtils.getBasicListMenu(getContext(), listItems, this);
    }

    private void onFaviconAvailable(Drawable drawable) {
        if (drawable != null) {
            setIcon(drawable);
        }
    }
}
