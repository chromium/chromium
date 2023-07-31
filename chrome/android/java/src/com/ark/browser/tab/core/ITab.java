package com.ark.browser.tab.core;

import android.graphics.Color;

import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.TabInfo;

public interface ITab {

    int INVALID_TAB_INDEX = -1;

    default int getId() {
        return getTabInfo().getId();
    }

    String getTitle();

    default int getThemeColor() {
        PageInfo pageInfo = getCurrentPageInfo();
        if (pageInfo == null) {
            return getTabInfo().isIncognito() ? Color.BLACK : Color.WHITE;
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

    ITab cloneByGroupTab(ITabGroup group);

    /**
     * TODO modify to CompositorViewHolder
     * @return
     */
    default ITabGroup getRootGroupTab() {

        ITabGroup tabGroup = getParentTab();

        while (tabGroup != null) {
            ITabGroup parent = tabGroup.getParentTab();
            if (parent == null) {
                return tabGroup;
            } else {
                tabGroup = parent;
            }
        }
        if (this instanceof ITabGroup) {
            return (ITabGroup) this;
        }
        throw new RuntimeException("root TabGroup is null!");
    }

    TabInfo getTabInfo();

    PageInfo getCurrentPageInfo();

    IPage getCurrentPage();

    IPage findPageById(int pageId);

    ITab cloneTab();

    void destroy();

    void remove();

    void saveTabInfo();

}
