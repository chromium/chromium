package com.ark.browser.tab;

import androidx.annotation.Keep;

import org.chromium.chrome.browser.tab.Tab;

import java.util.ArrayList;
import java.util.List;

@Keep
//@Table(database = TabInfoManager.class)
public class TabInfo extends DbModel {

    private static final String TAG = "TabManager";

    protected transient final List<PageInfo> mPageInfoList = new ArrayList<>();

//    @PrimaryKey()
    protected String tabInfoId;

//    @Column
    protected long createTime;

//    @Column
    protected int pageIndex = 0;

//    @Column
    protected int currentTabId = Tab.INVALID_PAGE_ID;

//    @Column
    protected int position = 0;

//    @Column
    protected boolean isLocked = false;

//    @Column
    public boolean incognito;

//    @Column
    protected long accessTime;

    public void setTabInfoId(String tabInfoId) {
        this.tabInfoId = tabInfoId;
    }

    public void setCreateTime(long createTime) {
        this.createTime = createTime;
    }

    public int getPageIndex() {
        return pageIndex;
    }

    public void setPageIndex(int pageIndex) {
        this.pageIndex = pageIndex;
    }

    public int getCurrentTabId() {
        return currentTabId;
    }

    public TabInfo cloneTabInfo() {
        TabInfo newTabInfo = TabInfo.create();
        newTabInfo.tabInfoId = tabInfoId;
        newTabInfo.createTime = createTime;
        newTabInfo.pageIndex = pageIndex;
        newTabInfo.currentTabId = currentTabId;
        newTabInfo.position = position;
        newTabInfo.isLocked = isLocked;
        newTabInfo.incognito = incognito;
        newTabInfo.accessTime = accessTime;
        newTabInfo.mPageInfoList.addAll(mPageInfoList);
        return newTabInfo;
    }


    public boolean isIncognito() {
        return incognito;
    }

    public void setIncognito(boolean incognito) {
        this.incognito = incognito;
    }

    public long getCreateTime() {
        return createTime;
    }

    public int getIndex() {
        return pageIndex;
    }

    public void setIndex(int index) {
        this.pageIndex = index;
        save();
    }

    public void setCurrentTabId(int currentTabId) {
        this.currentTabId = currentTabId;
    }

    public String getTabInfoId() {
        return tabInfoId;
    }

    public void setPosition(int position) {
        this.position = position;
    }

    public int getPosition() {
        return position;
    }

    public void setLocked(boolean locked) {
        isLocked = locked;
    }

    public boolean isLocked() {
        return isLocked;
    }

    public long getAccessTime() {
        return accessTime;
    }

    public void setAccessTime(long accessTime) {
        this.accessTime = accessTime;
    }

    public static TabInfo create() {
        return create(System.currentTimeMillis());
    }

    public static TabInfo create(long createTime) {
        TabInfo manager = new TabInfo();
        manager.createTime = createTime;
        manager.tabInfoId = String.valueOf(manager.createTime);
        return manager;
    }

    public static TabInfo from(TabInfo tabInfo) {
        TabInfo newTabInfo = TabInfo.create();
        newTabInfo.setTabInfoId(tabInfo.getTabInfoId());
        newTabInfo.setCreateTime(tabInfo.getCreateTime());
        newTabInfo.setPageIndex(tabInfo.getPageIndex());
        newTabInfo.setCurrentTabId(tabInfo.getCurrentTabId());
        newTabInfo.setPosition(tabInfo.getPosition());
        newTabInfo.setLocked(tabInfo.isLocked());
        newTabInfo.setIncognito(tabInfo.isIncognito());
        return newTabInfo;
    }

    @Override
    public String toString() {
        return "TabInfo{" +
                "mPageInfoList=" + mPageInfoList +
                ", tabInfoId='" + tabInfoId + '\'' +
                ", createTime=" + createTime +
                ", pageIndex=" + pageIndex +
                ", currentTabId=" + currentTabId +
                ", position=" + position +
                ", isLocked=" + isLocked +
                ", incognito=" + incognito +
                '}';
    }
}
