// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import androidx.annotation.StringDef;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator.TabStripLayoutType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Helper utility for logging UMA user action metrics across tab strip context menus. */
@NullMarked
public class TabStripMenuMetricsUtils {

    @Retention(RetentionPolicy.SOURCE)
    @StringDef({
        TabMenuAction.ADD_TO_TAB_GROUP,
        TabMenuAction.ADD_TO_NEW_TAB_GROUP,
        TabMenuAction.REMOVE_TAB_FROM_TAB_GROUP,
        TabMenuAction.MOVE_TAB_TO_NEW_WINDOW,
        TabMenuAction.MOVE_TABS_TO_OTHER_WINDOW,
        TabMenuAction.SHARE_TAB,
        TabMenuAction.PIN_TAB,
        TabMenuAction.UNPIN_TAB,
        TabMenuAction.CLOSE_TAB,
        TabMenuAction.NEW_GROUP,
        TabMenuAction.MOVE_TAB_TO_GROUP,
        TabMenuAction.MOVE_TAB_TO_INCOGNITO_GROUP,
        TabMenuAction.MOVE_TAB_TO_OTHER_WINDOW,
        TabMenuAction.MUTE_SITE,
        TabMenuAction.UNMUTE_SITE,
        TabMenuAction.DUPLICATE_TAB,
        TabMenuAction.CLOSE_ALL_TABS,
        TabMenuAction.CLOSE_ALL_INCOGNITO_TABS,
        TabMenuAction.CLOSE_OTHER_TABS,
        TabMenuAction.CLOSE_TABS_BELOW,
        TabMenuAction.CLOSE_TABS_TO_THE_RIGHT,
        TabMenuAction.NEW_TAB_BELOW,
        TabMenuAction.NEW_TAB_TO_THE_RIGHT,
        TabMenuAction.ADD_TAB_TO_READING_LIST,
        TabMenuAction.SEND_TO_YOUR_DEVICES,
        TabMenuAction.TOGGLE_TAB_LAYOUT,
        TabMenuAction.SHOWN,
    })
    public @interface TabMenuAction {
        String ADD_TO_TAB_GROUP = "AddToTabGroup";
        String ADD_TO_NEW_TAB_GROUP = "AddToNewTabGroup";
        String REMOVE_TAB_FROM_TAB_GROUP = "RemoveTabFromTabGroup";
        String MOVE_TAB_TO_NEW_WINDOW = "MoveTabToNewWindow";
        String MOVE_TABS_TO_OTHER_WINDOW = "MoveTabsToOtherWindow";
        String SHARE_TAB = "ShareTab";
        String PIN_TAB = "PinTab";
        String UNPIN_TAB = "UnpinTab";
        String CLOSE_TAB = "CloseTab";
        String NEW_GROUP = "NewGroup";
        String MOVE_TAB_TO_GROUP = "MoveTabToGroup";
        String MOVE_TAB_TO_INCOGNITO_GROUP = "MoveTabToIncognitoGroup";
        String MOVE_TAB_TO_OTHER_WINDOW = "MoveTabToOtherWindow";
        String MUTE_SITE = "MuteSite";
        String UNMUTE_SITE = "UnmuteSite";
        String DUPLICATE_TAB = "DuplicateTab";
        String CLOSE_ALL_TABS = "CloseAllTabs";
        String CLOSE_ALL_INCOGNITO_TABS = "CloseAllIncognitoTabs";
        String CLOSE_OTHER_TABS = "CloseOtherTabs";
        String CLOSE_TABS_BELOW = "CloseTabsBelow";
        String CLOSE_TABS_TO_THE_RIGHT = "CloseTabsToTheRight";
        String NEW_TAB_BELOW = "NewTabBelow";
        String NEW_TAB_TO_THE_RIGHT = "NewTabToTheRight";
        String ADD_TAB_TO_READING_LIST = "AddTabToReadingList";
        String SEND_TO_YOUR_DEVICES = "SendToYourDevices";
        String TOGGLE_TAB_LAYOUT = "ToggleTabLayout";
        String SHOWN = "Shown";
    }

    @Retention(RetentionPolicy.SOURCE)
    @StringDef({
        GroupMenuAction.UNGROUP,
        GroupMenuAction.CLOSE_GROUP,
        GroupMenuAction.DELETE_GROUP,
        GroupMenuAction.NEW_TAB_IN_GROUP,
        GroupMenuAction.MOVE_GROUP_TO_NEW_WINDOW,
        GroupMenuAction.MOVE_GROUP_TO_ANOTHER_WINDOW,
        GroupMenuAction.SHARE_GROUP,
        GroupMenuAction.MANAGE_SHARING,
        GroupMenuAction.RECENT_ACTIVITY,
        GroupMenuAction.DELETE_SHARED_GROUP,
        GroupMenuAction.LEAVE_SHARED_GROUP,
        GroupMenuAction.SHOWN,
        GroupMenuAction.COLOR_CHANGED,
        GroupMenuAction.TITLE_RESET,
        GroupMenuAction.TITLE_CHANGED,
    })
    public @interface GroupMenuAction {
        String UNGROUP = "Ungroup";
        String CLOSE_GROUP = "CloseGroup";
        String DELETE_GROUP = "DeleteGroup";
        String NEW_TAB_IN_GROUP = "NewTabInGroup";
        String MOVE_GROUP_TO_NEW_WINDOW = "MoveGroupToNewWindow";
        String MOVE_GROUP_TO_ANOTHER_WINDOW = "MoveGroupToAnotherWindow";
        String SHARE_GROUP = "ShareGroup";
        String MANAGE_SHARING = "ManageSharing";
        String RECENT_ACTIVITY = "RecentActivity";
        String DELETE_SHARED_GROUP = "DeleteSharedGroup";
        String LEAVE_SHARED_GROUP = "LeaveSharedGroup";
        String SHOWN = "Shown";
        String COLOR_CHANGED = "ColorChanged";
        String TITLE_RESET = "TitleReset";
        String TITLE_CHANGED = "TitleChanged";
    }

    @Retention(RetentionPolicy.SOURCE)
    @StringDef({
        StripMenuAction.REOPEN_CLOSED_ENTRY,
        StripMenuAction.TOGGLE_TAB_LAYOUT,
        StripMenuAction.PIN_GLIC,
        StripMenuAction.UNPIN_GLIC,
        StripMenuAction.TASK_MANAGER,
        StripMenuAction.SEND_FEEDBACK,
    })
    public @interface StripMenuAction {
        String REOPEN_CLOSED_ENTRY = "ReopenClosedEntry";
        String TOGGLE_TAB_LAYOUT = "ToggleTabLayout";
        String PIN_GLIC = "PinGlic";
        String UNPIN_GLIC = "UnpinGlic";
        String TASK_MANAGER = "TaskManager";
        String SEND_FEEDBACK = "SendFeedback";
    }

    private TabStripMenuMetricsUtils() {}

    /**
     * Records user actions for single or multiple tab context menu items.
     *
     * @param label The base metric action label.
     * @param isMultipleTabs Whether the action was performed on multiple tabs.
     * @param layout The active {@link TabStripLayoutType}.
     */
    public static void recordTabMenuUserAction(
            @TabMenuAction String label, boolean isMultipleTabs, @TabStripLayoutType int layout) {
        if (layout == TabStripLayoutType.VERTICAL) {
            switch (label) {
                case TabMenuAction.ADD_TO_TAB_GROUP:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.AddToTabGroup.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.AddToTabGroup");
                    break;
                case TabMenuAction.ADD_TO_NEW_TAB_GROUP:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.AddToNewTabGroup.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.AddToNewTabGroup");
                    break;
                case TabMenuAction.REMOVE_TAB_FROM_TAB_GROUP:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.RemoveTabFromTabGroup.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.RemoveTabFromTabGroup");
                    break;
                case TabMenuAction.MOVE_TAB_TO_NEW_WINDOW:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.MoveTabToNewWindow.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.MoveTabToNewWindow");
                    break;
                case TabMenuAction.MOVE_TABS_TO_OTHER_WINDOW:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.MoveTabsToOtherWindow.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.MoveTabsToOtherWindow");
                    break;
                case TabMenuAction.SHARE_TAB:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.ShareTab.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.ShareTab");
                    break;
                case TabMenuAction.PIN_TAB:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.PinTab.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.PinTab");
                    break;
                case TabMenuAction.UNPIN_TAB:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.UnpinTab.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.UnpinTab");
                    break;
                case TabMenuAction.CLOSE_TAB:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.CloseTab.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.CloseTab");
                    break;
                case TabMenuAction.NEW_GROUP:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.NewGroup.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.NewGroup");
                    break;
                case TabMenuAction.MOVE_TAB_TO_GROUP:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.MoveTabToGroup.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.MoveTabToGroup");
                    break;
                case TabMenuAction.MOVE_TAB_TO_INCOGNITO_GROUP:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.MoveTabToIncognitoGroup.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.MoveTabToIncognitoGroup");
                    break;
                case TabMenuAction.MOVE_TAB_TO_OTHER_WINDOW:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.MoveTabToOtherWindow.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.MoveTabToOtherWindow");
                    break;
                case TabMenuAction.MUTE_SITE:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.MuteSite.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.MuteSite");
                    break;
                case TabMenuAction.UNMUTE_SITE:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.UnmuteSite.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.UnmuteSite");
                    break;
                case TabMenuAction.DUPLICATE_TAB:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.DuplicateTab.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.DuplicateTab");
                    break;
                case TabMenuAction.CLOSE_ALL_TABS:
                    RecordUserAction.record("Android.VerticalTabs.TabMenu.CloseAllTabs");
                    break;
                case TabMenuAction.CLOSE_ALL_INCOGNITO_TABS:
                    RecordUserAction.record("Android.VerticalTabs.TabMenu.CloseAllIncognitoTabs");
                    break;
                case TabMenuAction.CLOSE_OTHER_TABS:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.CloseOtherTabs.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.CloseOtherTabs");
                    break;
                case TabMenuAction.CLOSE_TABS_BELOW:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.CloseTabsBelow.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.CloseTabsBelow");
                    break;
                case TabMenuAction.NEW_TAB_BELOW:
                    RecordUserAction.record("Android.VerticalTabs.TabMenu.NewTabBelow");
                    break;
                case TabMenuAction.ADD_TAB_TO_READING_LIST:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.AddTabToReadingList.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.AddTabToReadingList");
                    break;
                case TabMenuAction.SEND_TO_YOUR_DEVICES:
                    RecordUserAction.record("Android.VerticalTabs.TabMenu.SendToYourDevices");
                    break;
                case TabMenuAction.TOGGLE_TAB_LAYOUT:
                    RecordUserAction.record("Android.VerticalTabs.TabMenu.ToggleTabLayout");
                    break;
                case TabMenuAction.SHOWN:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "Android.VerticalTabs.TabMenu.Shown.MultiTab"
                                    : "Android.VerticalTabs.TabMenu.Shown");
                    break;
                default:
                    assert false : "Unknown vertical tab menu action: " + label;
            }
        } else {
            switch (label) {
                case TabMenuAction.ADD_TO_TAB_GROUP:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.AddToTabGroup.MultiTab"
                                    : "MobileToolbarTabMenu.AddToTabGroup");
                    break;
                case TabMenuAction.ADD_TO_NEW_TAB_GROUP:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.AddToNewTabGroup.MultiTab"
                                    : "MobileToolbarTabMenu.AddToNewTabGroup");
                    break;
                case TabMenuAction.REMOVE_TAB_FROM_TAB_GROUP:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.RemoveTabFromTabGroup.MultiTab"
                                    : "MobileToolbarTabMenu.RemoveTabFromTabGroup");
                    break;
                case TabMenuAction.MOVE_TAB_TO_NEW_WINDOW:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.MoveTabToNewWindow.MultiTab"
                                    : "MobileToolbarTabMenu.MoveTabToNewWindow");
                    break;
                case TabMenuAction.MOVE_TABS_TO_OTHER_WINDOW:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.MoveTabsToOtherWindow.MultiTab"
                                    : "MobileToolbarTabMenu.MoveTabsToOtherWindow");
                    break;
                case TabMenuAction.SHARE_TAB:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.ShareTab.MultiTab"
                                    : "MobileToolbarTabMenu.ShareTab");
                    break;
                case TabMenuAction.PIN_TAB:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.PinTab.MultiTab"
                                    : "MobileToolbarTabMenu.PinTab");
                    break;
                case TabMenuAction.UNPIN_TAB:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.UnpinTab.MultiTab"
                                    : "MobileToolbarTabMenu.UnpinTab");
                    break;
                case TabMenuAction.CLOSE_TAB:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.CloseTab.MultiTab"
                                    : "MobileToolbarTabMenu.CloseTab");
                    break;
                case TabMenuAction.NEW_GROUP:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.NewGroup.MultiTab"
                                    : "MobileToolbarTabMenu.NewGroup");
                    break;
                case TabMenuAction.MOVE_TAB_TO_GROUP:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.MoveTabToGroup.MultiTab"
                                    : "MobileToolbarTabMenu.MoveTabToGroup");
                    break;
                case TabMenuAction.MOVE_TAB_TO_INCOGNITO_GROUP:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.MoveTabToIncognitoGroup.MultiTab"
                                    : "MobileToolbarTabMenu.MoveTabToIncognitoGroup");
                    break;
                case TabMenuAction.MOVE_TAB_TO_OTHER_WINDOW:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.MoveTabToOtherWindow.MultiTab"
                                    : "MobileToolbarTabMenu.MoveTabToOtherWindow");
                    break;
                case TabMenuAction.MUTE_SITE:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.MuteSite.MultiTab"
                                    : "MobileToolbarTabMenu.MuteSite");
                    break;
                case TabMenuAction.UNMUTE_SITE:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.UnmuteSite.MultiTab"
                                    : "MobileToolbarTabMenu.UnmuteSite");
                    break;
                case TabMenuAction.DUPLICATE_TAB:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.DuplicateTab.MultiTab"
                                    : "MobileToolbarTabMenu.DuplicateTab");
                    break;
                case TabMenuAction.CLOSE_ALL_TABS:
                    RecordUserAction.record("MobileToolbarTabMenu.CloseAllTabs");
                    break;
                case TabMenuAction.CLOSE_ALL_INCOGNITO_TABS:
                    RecordUserAction.record("MobileToolbarTabMenu.CloseAllIncognitoTabs");
                    break;
                case TabMenuAction.CLOSE_OTHER_TABS:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.CloseOtherTabs.MultiTab"
                                    : "MobileToolbarTabMenu.CloseOtherTabs");
                    break;
                case TabMenuAction.CLOSE_TABS_TO_THE_RIGHT:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.CloseTabsToTheRight.MultiTab"
                                    : "MobileToolbarTabMenu.CloseTabsToTheRight");
                    break;
                case TabMenuAction.NEW_TAB_TO_THE_RIGHT:
                    RecordUserAction.record("MobileToolbarTabMenu.NewTabToTheRight");
                    break;
                case TabMenuAction.ADD_TAB_TO_READING_LIST:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.AddTabToReadingList.MultiTab"
                                    : "MobileToolbarTabMenu.AddTabToReadingList");
                    break;
                case TabMenuAction.SEND_TO_YOUR_DEVICES:
                    RecordUserAction.record("MobileToolbarTabMenu.SendToYourDevices");
                    break;
                case TabMenuAction.TOGGLE_TAB_LAYOUT:
                    RecordUserAction.record("MobileToolbarTabMenu.ToggleTabLayout");
                    break;
                case TabMenuAction.SHOWN:
                    RecordUserAction.record(
                            isMultipleTabs
                                    ? "MobileToolbarTabMenu.Shown.MultiTab"
                                    : "MobileToolbarTabMenu.Shown");
                    break;
                default:
                    assert false : "Unknown mobile toolbar tab menu action: " + label;
            }
        }
    }

    /**
     * Records user actions for tab group context menu items.
     *
     * @param action The action name label.
     * @param layout The active {@link TabStripLayoutType}.
     */
    public static void recordGroupMenuUserAction(
            @GroupMenuAction String action, @TabStripLayoutType int layout) {
        if (layout == TabStripLayoutType.VERTICAL) {
            switch (action) {
                case GroupMenuAction.UNGROUP:
                    RecordUserAction.record("Android.VerticalTabs.GroupMenu.Ungroup");
                    break;
                case GroupMenuAction.CLOSE_GROUP:
                    RecordUserAction.record("Android.VerticalTabs.GroupMenu.CloseGroup");
                    break;
                case GroupMenuAction.DELETE_GROUP:
                    RecordUserAction.record("Android.VerticalTabs.GroupMenu.DeleteGroup");
                    break;
                case GroupMenuAction.NEW_TAB_IN_GROUP:
                    RecordUserAction.record("Android.VerticalTabs.GroupMenu.NewTabInGroup");
                    break;
                case GroupMenuAction.MOVE_GROUP_TO_NEW_WINDOW:
                    RecordUserAction.record("Android.VerticalTabs.GroupMenu.MoveGroupToNewWindow");
                    break;
                case GroupMenuAction.MOVE_GROUP_TO_ANOTHER_WINDOW:
                    RecordUserAction.record(
                            "Android.VerticalTabs.GroupMenu.MoveGroupToAnotherWindow");
                    break;
                case GroupMenuAction.SHARE_GROUP:
                    RecordUserAction.record("Android.VerticalTabs.GroupMenu.ShareGroup");
                    break;
                case GroupMenuAction.MANAGE_SHARING:
                    RecordUserAction.record("Android.VerticalTabs.GroupMenu.ManageSharing");
                    break;
                case GroupMenuAction.RECENT_ACTIVITY:
                    RecordUserAction.record("Android.VerticalTabs.GroupMenu.RecentActivity");
                    break;
                case GroupMenuAction.DELETE_SHARED_GROUP:
                    RecordUserAction.record("Android.VerticalTabs.GroupMenu.DeleteSharedGroup");
                    break;
                case GroupMenuAction.LEAVE_SHARED_GROUP:
                    RecordUserAction.record("Android.VerticalTabs.GroupMenu.LeaveSharedGroup");
                    break;
                case GroupMenuAction.SHOWN:
                    RecordUserAction.record("Android.VerticalTabs.GroupMenu.Shown");
                    break;
                case GroupMenuAction.COLOR_CHANGED:
                    RecordUserAction.record("Android.VerticalTabs.GroupMenu.ColorChanged");
                    break;
                case GroupMenuAction.TITLE_RESET:
                    RecordUserAction.record("Android.VerticalTabs.GroupMenu.TitleReset");
                    break;
                case GroupMenuAction.TITLE_CHANGED:
                    RecordUserAction.record("Android.VerticalTabs.GroupMenu.TitleChanged");
                    break;
                default:
                    assert false : "Unknown group menu action: " + action;
            }
        } else {
            switch (action) {
                case GroupMenuAction.UNGROUP:
                    RecordUserAction.record("MobileToolbarTabGroupMenu.Ungroup");
                    break;
                case GroupMenuAction.CLOSE_GROUP:
                    RecordUserAction.record("MobileToolbarTabGroupMenu.CloseGroup");
                    break;
                case GroupMenuAction.DELETE_GROUP:
                    RecordUserAction.record("MobileToolbarTabGroupMenu.DeleteGroup");
                    break;
                case GroupMenuAction.NEW_TAB_IN_GROUP:
                    RecordUserAction.record("MobileToolbarTabGroupMenu.NewTabInGroup");
                    break;
                case GroupMenuAction.MOVE_GROUP_TO_NEW_WINDOW:
                    RecordUserAction.record("MobileToolbarTabGroupMenu.MoveGroupToNewWindow");
                    break;
                case GroupMenuAction.MOVE_GROUP_TO_ANOTHER_WINDOW:
                    RecordUserAction.record("MobileToolbarTabGroupMenu.MoveGroupToAnotherWindow");
                    break;
                case GroupMenuAction.SHARE_GROUP:
                    RecordUserAction.record("MobileToolbarTabGroupMenu.ShareGroup");
                    break;
                case GroupMenuAction.MANAGE_SHARING:
                    RecordUserAction.record("MobileToolbarTabGroupMenu.ManageSharing");
                    break;
                case GroupMenuAction.RECENT_ACTIVITY:
                    RecordUserAction.record("MobileToolbarTabGroupMenu.RecentActivity");
                    break;
                case GroupMenuAction.DELETE_SHARED_GROUP:
                    RecordUserAction.record("MobileToolbarTabGroupMenu.DeleteSharedGroup");
                    break;
                case GroupMenuAction.LEAVE_SHARED_GROUP:
                    RecordUserAction.record("MobileToolbarTabGroupMenu.LeaveSharedGroup");
                    break;
                case GroupMenuAction.SHOWN:
                    RecordUserAction.record("MobileToolbarTabGroupMenu.Shown");
                    break;
                case GroupMenuAction.COLOR_CHANGED:
                    RecordUserAction.record("MobileToolbarTabGroupMenu.ColorChanged");
                    break;
                case GroupMenuAction.TITLE_RESET:
                    RecordUserAction.record("MobileToolbarTabGroupMenu.TitleReset");
                    break;
                case GroupMenuAction.TITLE_CHANGED:
                    RecordUserAction.record("MobileToolbarTabGroupMenu.TitleChanged");
                    break;
                default:
                    assert false : "Unknown group menu action: " + action;
            }
        }
    }

    /**
     * Records user actions for tab strip empty space context menu items.
     *
     * @param action The action name label.
     * @param layout The active {@link TabStripLayoutType}.
     */
    public static void recordStripMenuUserAction(
            @StripMenuAction String action, @TabStripLayoutType int layout) {
        if (layout == TabStripLayoutType.VERTICAL) {
            switch (action) {
                case StripMenuAction.REOPEN_CLOSED_ENTRY:
                    RecordUserAction.record("Android.VerticalTabs.StripMenu.ReopenClosedEntry");
                    break;
                case StripMenuAction.TOGGLE_TAB_LAYOUT:
                    RecordUserAction.record("Android.VerticalTabs.StripMenu.ToggleTabLayout");
                    break;
                case StripMenuAction.PIN_GLIC:
                    RecordUserAction.record("Android.VerticalTabs.StripMenu.PinGlic");
                    break;
                case StripMenuAction.UNPIN_GLIC:
                    RecordUserAction.record("Android.VerticalTabs.StripMenu.UnpinGlic");
                    break;
                case StripMenuAction.TASK_MANAGER:
                    RecordUserAction.record("Android.VerticalTabs.StripMenu.TaskManager");
                    break;
                case StripMenuAction.SEND_FEEDBACK:
                    RecordUserAction.record("Android.VerticalTabs.StripMenu.SendFeedback");
                    break;
                default:
                    assert false : "Unknown strip menu action: " + action;
            }
        } else {
            switch (action) {
                case StripMenuAction.REOPEN_CLOSED_ENTRY:
                    RecordUserAction.record("Android.TabStripMenu.ReopenClosedEntry");
                    break;
                case StripMenuAction.TOGGLE_TAB_LAYOUT:
                    RecordUserAction.record("Android.TabStripMenu.ToggleTabLayout");
                    break;
                case StripMenuAction.PIN_GLIC:
                    RecordUserAction.record("Android.TabStripMenu.PinGlic");
                    break;
                case StripMenuAction.UNPIN_GLIC:
                    RecordUserAction.record("Android.TabStripMenu.UnpinGlic");
                    break;
                case StripMenuAction.TASK_MANAGER:
                    RecordUserAction.record("Android.TabStripMenu.TaskManager");
                    break;
                case StripMenuAction.SEND_FEEDBACK:
                    RecordUserAction.record("Android.TabStripMenu.SendFeedback");
                    break;
                default:
                    assert false : "Unknown strip menu action: " + action;
            }
        }
    }
}
