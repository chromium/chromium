package com.ark.browser.tab;

import androidx.annotation.Keep;

import com.ark.browser.core.utils.ArkIdManager;
import com.ark.browser.tab.core.ChildTab;
import com.ark.browser.utils.ArkLogger;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

@Keep
public class TabInfo {

    private int mTabId;

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

    // TODO
    protected String mTitle;

    private TabInfo() {

    }

    public int getId() {
        return mTabId;
    }

    public void setId(int tabId) {
        this.mTabId = tabId;
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

    public String getTitle() {
        return mTitle;
    }

    public void setTitle(String title) {
        mTitle = title;
    }

    public static TabInfo create(int id, int parentId, boolean isGroup) {
        TabInfo manager = new TabInfo();
        manager.createTime = System.currentTimeMillis();
        manager.mTabId = id;
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

    public static TabInfo create(int parentId, boolean isGroup) {
        return create(parentId, isGroup, System.currentTimeMillis());
    }

    public static TabInfo create(int parentId, boolean isGroup, long createTime) {
        TabInfo manager = new TabInfo();
        manager.createTime = createTime;
        manager.accessTime = createTime;
        manager.mTabId = ArkIdManager.generateTabId();
        manager.mParentId = parentId;
        manager.mIsGroup = isGroup;
        return manager;
    }

    public void wrapStream(DataOutputStream os) throws IOException {
        int version = 6;
        os.writeInt(version);
        os.writeInt(mTabId);
        os.writeInt(mParentId);
        os.writeBoolean(mIsGroup);
        os.writeInt(mLaunchType);
        os.writeLong(createTime);
        os.writeBoolean(incognito);
        os.writeBoolean(isLocked);
        os.writeInt(childIndex);
        os.writeInt(currentChildId);
        os.writeInt(position);
        os.writeLong(accessTime);
        os.writeUTF(mTitle == null ? "" : mTitle);
    }

    public void fromStream(DataInputStream is) throws IOException {
        int version = is.readInt();
        ArkLogger.e(this, "fromStream version=" + version);
        mTabId = is.readInt();
        if (version >= 3) {
            if (version >= 5) {
                mParentId = is.readInt();
            } else {
                String name = is.readUTF();
                if ("group_incognito".equals(name)) {
                    mParentId = -101;
                } else {
                    mParentId = -100;
                }
            }
        } else {
            mParentId = -100;
        }
        if (version >= 4) {
            mIsGroup = is.readBoolean();
        } else {
            mIsGroup = false;
        }
        if (version >= 2) {
            mLaunchType = is.readInt();
        }
        createTime = is.readLong();
        incognito = is.readBoolean();
        isLocked = is.readBoolean();
        childIndex = is.readInt();
        currentChildId = is.readInt();
        position = is.readInt();
        accessTime = is.readLong();
        if (version >= 6) {
            mTitle = is.readUTF();
        }
    }

    public static TabInfo create(DataInputStream is) throws IOException {
        int version = is.readInt();
        int tabId = is.readInt();
        int parentId;
        if (version >= 3) {
            if (version >= 5) {
                parentId = is.readInt();
            } else {
                String name = is.readUTF();
                if ("group_incognito".equals(name)) {
                    parentId = -101;
                } else {
                    parentId = -100;
                }
            }
        } else {
            parentId = -100;
        }
        TabInfo newTabInfo = TabInfo.create(tabId, parentId, false);
        ArkLogger.e(ChildTab.class, "from version=" + version + " parentId=" + newTabInfo.getParentId());
        if (version >= 4) {
            newTabInfo.setIsGroup(is.readBoolean());
        } else {
            newTabInfo.setIsGroup(false);
        }
        if (version >= 2) {
            newTabInfo.setLaunchType(is.readInt());
        }
        newTabInfo.setCreateTime(is.readLong());
        newTabInfo.setIncognito(is.readBoolean());
        newTabInfo.setLocked(is.readBoolean());

        newTabInfo.setChildIndex(is.readInt());
        newTabInfo.setCurrentPageId(is.readInt());
        newTabInfo.setPosition(is.readInt());
        newTabInfo.setAccessTime(is.readLong());
        if (version >= 6) {
            newTabInfo.setTitle(is.readUTF());
        }
        return newTabInfo;
    }

    @Override
    public String toString() {
        return "TabInfo{" +
                "tabId=" + mTabId +
                ", parentId=" + mParentId +
                ", createTime=" + createTime +
                ", childIndex=" + childIndex +
                ", currentChildId=" + currentChildId +
                ", position=" + position +
                ", isLocked=" + isLocked +
                ", incognito=" + incognito +
                ", accessTime=" + accessTime +
                ", fromId=" + fromId +
                ", launchType=" + mLaunchType +
                ", isGroup=" + mIsGroup +
                ", title='" + mTitle + '\'' +
                '}';
    }

}
