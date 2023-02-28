package com.ark.browser.tab.core;

import com.ark.browser.core.ArkWebManager;
import com.ark.browser.tab.TabInfo;
import com.ark.browser.tab.TabListManager;
import com.ark.browser.utils.ArkLogger;

import java.util.ArrayList;
import java.util.List;

public class TabImpl implements ITab {

    private static final String TAG = "TabImpl";

    private final TabInfo tabInfo;

    private transient ITab mFloatingTab;

    public TabImpl(String groupId) {
        this(TabInfo.create(groupId));
    }

    public TabImpl(TabInfo tabInfo) {
        this.tabInfo = tabInfo;
    }

    @Override
    public boolean hasPage(int id) {
        if (mFloatingTab != null && mFloatingTab.hasPage(id)) {
            return true;
        }
        return ITab.super.hasPage(id);
    }

    @Override
    public TabInfo getTabInfo() {
        return tabInfo;
    }

    @Override
    public void exitFloatingTabInfo() {
        ArkLogger.d(TAG, "exitFloatingTabInfo mFloatingTabInfo=" + mFloatingTab + " this=" + this);
        if (mFloatingTab == null) {
            return;
        }

        List<IPage> temp = new ArrayList<>(getPages());
        TabInfo mFloatingTabInfo = mFloatingTab.getTabInfo();

        tabInfo.setId(mFloatingTabInfo.getId());
        tabInfo.setCreateTime(mFloatingTabInfo.getCreateTime());

        tabInfo.setPageIndex(mFloatingTabInfo.getPageIndex());
        tabInfo.setCurrentPageId(mFloatingTabInfo.getCurrentPageId());
        tabInfo.setPosition(mFloatingTabInfo.getPosition());
        tabInfo.setLocked(mFloatingTabInfo.isLocked());
        tabInfo.setIncognito(mFloatingTabInfo.isIncognito());

//        tabListFolder = mFloatingTab.getTabListFolder();

        getPages().clear();
        getPages().addAll(mFloatingTab.getPages());

        mFloatingTab = null;

        saveTabInfo();

        ITabGroup tabList = TabListManager.getInstance().getTabGroup(tabInfo.isIncognito());
        TabInfo currentTabInfo = tabList.getCurrentTabInfo();
        ArkLogger.d(TAG, "exitFloatingTabInfo currentTabInfo=" + currentTabInfo + " this=" + this);
        if (currentTabInfo != null
                && currentTabInfo.getId() == getTabInfo().getId()) {
            ArkLogger.d(TAG, "exitFloatingTabInfo tab=" + getCurrentPageInfo());
            TabListManager.getInstance().selectTab(this);
        }

        for (IPage page : temp) {
            ArkWebManager.remove(page.getId());
        }
        temp.clear();
    }

    @Override
    public ITab cloneTab() {
        if (mFloatingTab != null) {
            return mFloatingTab.cloneTab();
        }
        return ITab.super.cloneTab();
    }

    @Override
    public String toString() {
        return "TabImpl{" +
                "tabInfo=" + tabInfo +
                '}';
    }
}
