package com.ark.browser.tab;

import androidx.annotation.Keep;

import com.ark.browser.core.utils.ArkIdManager;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;

@Keep
public class TabInfo {

    private int tabId;

    // TODO
    private int mParentId;

    private long createTime;

    protected int childIndex = Tab.INVALID_PAGE_ID;

    protected int currentChildId = Tab.INVALID_PAGE_ID;

    protected int position = 0;

    protected boolean isLocked = false;

    public boolean incognito;

    protected long accessTime;

    protected int fromId = Tab.INVALID_TAB_ID;

    @TabLaunchType
    protected int mLaunchType;

    protected boolean mIsGroup;

    protected String title;

    private TabInfo() {

    }

    public int getId() {
        return tabId;
    }

    public void setId(int tabId) {
        this.tabId = tabId;
    }

    public int getParentId() {
        return mParentId;
    }

    public void setParentId(int parentId) {
        mParentId = parentId;
    }

    public void setCreateTime(long createTime) {
        this.createTime = createTime;
    }

    public int getChildIndex() {
        return childIndex;
    }

    public void setChildIndex(int pageIndex) {
        this.childIndex = pageIndex;
    }

    public int getCurrentPageId() {
        return currentChildId;
    }

    public TabInfo cloneTabInfo() {
        TabInfo newTabInfo = TabInfo.create(mParentId);
        newTabInfo.createTime = createTime;
        newTabInfo.childIndex = childIndex;
        newTabInfo.currentChildId = Tab.INVALID_PAGE_ID;
        newTabInfo.position = position;
        newTabInfo.isLocked = isLocked;
        newTabInfo.incognito = incognito;
        newTabInfo.accessTime = accessTime;
        newTabInfo.mLaunchType = mLaunchType;
        newTabInfo.fromId = fromId;
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
        return childIndex;
    }

    public void setIndex(int index) {
        this.childIndex = index;
    }

    public void setCurrentPageId(int currentPageId) {
        this.currentChildId = currentPageId;
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

    public int getFromId() {
        return fromId;
    }

    public void setFromId(int fromId) {
        this.fromId = fromId;
    }

    public int getLaunchType() {
        return mLaunchType;
    }

    public void setLaunchType(@TabLaunchType int launchType) {
        this.mLaunchType = launchType;
    }

    public boolean isGroup() {
        return mIsGroup;
    }

    public void setIsGroup(boolean isGroup) {
        mIsGroup = isGroup;
    }

//    public static TabInfo create(String groupId) {
//        return create(groupId, System.currentTimeMillis());
//    }

    public static TabInfo create(int id, int parentId, boolean isGroup) {
        TabInfo manager = new TabInfo();
        manager.createTime = System.currentTimeMillis();
        manager.tabId = id;
        manager.mParentId = parentId;
        manager.mIsGroup = isGroup;
        return manager;
    }

    public static TabInfo create(int parentId) {
        return create(parentId, System.currentTimeMillis());
    }

    public static TabInfo create(int parentId, long createTime) {
        return create(parentId, false, createTime);
    }

    public static TabInfo create(int parentId, boolean isGroup, long createTime) {
        TabInfo manager = new TabInfo();
        manager.createTime = createTime;
        manager.tabId = ArkIdManager.generateTabId();
        manager.mParentId = parentId;
        manager.mIsGroup = isGroup;
        return manager;
    }

    @Override
    public String toString() {
        return "TabInfo{" +
                "tabId=" + tabId +
                ", createTime=" + createTime +
                ", pageIndex=" + childIndex +
                ", currentPageId=" + currentChildId +
                ", position=" + position +
                ", isLocked=" + isLocked +
                ", incognito=" + incognito +
                ", accessTime=" + accessTime +
                ", parentId=" + mParentId +
                ", mLaunchType=" + mLaunchType +
                '}';
    }

}
