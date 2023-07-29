package com.ark.browser.tab.core;

import android.graphics.Color;

import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.TabInfo;

public interface ITab {

    int INVALID_TAB_INDEX = -1;

    default int getId() {
        return getTabInfo().getId();
    }

    default String getTitle() {
        PageInfo pageInfo = getCurrentPageInfo();
        if (pageInfo == null) {
            return "";
        }
        return pageInfo.getTitle();
    }

    default int getThemeColor() {
        PageInfo pageInfo = getCurrentPageInfo();
        if (pageInfo == null) {
            return Color.WHITE;
        }
        return pageInfo.getThemeColor();
    }

    default boolean isLocked() {
        return getTabInfo().isLocked();
    }

    default void setLocked(boolean isLocked) {
        getTabInfo().setLocked(isLocked);
    }

    default int getParentId() {
        return getTabInfo().getParentId();
    }

    ITabGroup getParentTab();

    TabInfo getTabInfo();

    PageInfo getCurrentPageInfo();

    IPage getCurrentPage();

    IPage findPageById(int pageId);

    ITab cloneTab();

    void destroy();

    void remove();

    void saveTabInfo();

}
