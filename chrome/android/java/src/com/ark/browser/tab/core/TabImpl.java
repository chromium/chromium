package com.ark.browser.tab.core;

import android.content.Context;

import com.ark.browser.tab.PageCacheManager;
import com.ark.browser.tab.TabInfo;
import com.ark.browser.tab.TabListManager;
import com.ark.browser.utils.ArkLogger;

import org.chromium.base.ContextUtils;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class TabImpl implements ITab {

    private static final String TAG = "TabImpl";

    private final TabInfo tabInfo;
    private final IPageGroup mPageGroup;

    private transient ITab mFloatingTab;

//    protected transient File tabListFolder;

    public TabImpl() {
        this(TabInfo.create(), new ArrayList<>());
    }

    public TabImpl(TabInfo tabInfo) {
        this(tabInfo, new ArrayList<>());
    }

    public TabImpl(TabInfo tabInfo, List<IPage> pages) {
        this.tabInfo = tabInfo;
        this.mPageGroup = new PageGroupImpl(pages);
    }

    @Override
    public long getId() {
        return this.tabInfo.getTabInfoId();
    }

//    @Override
//    public File getTabListFolder() {
//        if (tabListFolder != null) {
//            return tabListFolder;
//        }
//        File root = ContextUtils.getApplicationContext()
//                .getDir("tab_list", Context.MODE_PRIVATE);
//        tabListFolder = new File(root.getAbsolutePath(), String.valueOf(getId()));
//        if (tabListFolder.exists()) {
//            return tabListFolder;
//        } else {
//            tabListFolder.mkdirs();
//        }
//        return tabListFolder;
//    }

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
    public IPageGroup getPageGroup() {
        return mPageGroup;
    }

    @Override
    public void exitFloatingTabInfo() {
        ArkLogger.d(TAG, "exitFloatingTabInfo mFloatingTabInfo=" + mFloatingTab + " this=" + this);
        if (mFloatingTab == null) {
            return;
        }

        List<IPage> temp = new ArrayList<>(mPageGroup.getPageInfoList());
        TabInfo mFloatingTabInfo = mFloatingTab.getTabInfo();

        tabInfo.setTabInfoId(mFloatingTabInfo.getTabInfoId());
        tabInfo.setCreateTime(mFloatingTabInfo.getCreateTime());

        tabInfo.setPageIndex(mFloatingTabInfo.getPageIndex());
        tabInfo.setCurrentTabId(mFloatingTabInfo.getCurrentTabId());
        tabInfo.setPosition(mFloatingTabInfo.getPosition());
        tabInfo.setLocked(mFloatingTabInfo.isLocked());
        tabInfo.setIncognito(mFloatingTabInfo.isIncognito());

//        tabListFolder = mFloatingTab.getTabListFolder();

        mPageGroup.getPageInfoList().clear();
        mPageGroup.getPageInfoList().addAll(mFloatingTab.getPageGroup().getPageInfoList());

        mFloatingTab = null;

//        getTabInfo().save();
        saveTabInfo();

        ITabGroup tabList = TabListManager.getInstance().getTabList(tabInfo.isIncognito());
        TabInfo currentTabInfo = tabList.getCurrentTabInfo();
        ArkLogger.d(TAG, "exitFloatingTabInfo currentTabInfo=" + currentTabInfo + " this=" + this);
        if (currentTabInfo != null
                && currentTabInfo.getTabInfoId() == getTabInfo().getTabInfoId()) {
            ArkLogger.d(TAG, "exitFloatingTabInfo tab=" + getCurrentPageInfo());
            TabListManager.getInstance().selectTab(this);
        }

        for (IPage pageInfo : temp) {
            PageCacheManager.getInstance().removePage(pageInfo);
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

}
