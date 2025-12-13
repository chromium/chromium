// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.settings.FaviconLoader;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.FaviconViewUtils;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

@NullMarked
public class SafetyHubNotificationsPreference extends ChromeBasePreference
        implements ListMenu.Delegate {
    interface MenuClickListener {
        void onAllowClicked(SafetyHubNotificationsPreference preference);

        void onResetClicked(SafetyHubNotificationsPreference preference);
    }

    private static final int MENU_RESET_NOTIFICATIONS_ITEM_ID = 0;
    private static final int MENU_ALLOW_NOTIFICATIONS_ITEM_ID = 1;

    private final NotificationPermissions mNotificationPermissions;
    private final LargeIconBridge mLargeIconBridge;
    private @Nullable MenuClickListener mMenuClickListener;
    private boolean mFaviconFetched;

    SafetyHubNotificationsPreference(
            Context context,
            NotificationPermissions notificationPermissions,
            LargeIconBridge largeIconBridge) {
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
    public void onItemSelected(PropertyModel item, View view) {
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
                new ListItemBuilder()
                        .withTitleRes(R.string.safety_hub_reset_notifications_menu_item)
                        .withMenuId(MENU_RESET_NOTIFICATIONS_ITEM_ID)
                        .build());
        listItems.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.safety_hub_allow_notifications_menu_item)
                        .withMenuId(MENU_ALLOW_NOTIFICATIONS_ITEM_ID)
                        .build());
        return BrowserUiListMenuUtils.getBasicListMenu(getContext(), listItems, this);
    }

    private void onFaviconAvailable(Drawable drawable) {
        if (drawable != null) {
            setIcon(drawable);
        }
    }
}
