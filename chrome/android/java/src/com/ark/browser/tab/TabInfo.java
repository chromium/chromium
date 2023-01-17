package com.ark.browser.tab;

import android.util.AtomicFile;

import androidx.annotation.Keep;

import com.ark.browser.core.utils.ArkIdManager;
import com.ark.browser.tab.core.IPage;
import com.ark.browser.tab.core.PageImpl;
import com.ark.browser.tab.dao.ArkTabDao;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;

import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

@Keep
public class TabInfo {

    private int tabId;

    private long createTime;

    protected int pageIndex = Tab.INVALID_PAGE_ID;

    protected int currentPageId = Tab.INVALID_PAGE_ID;

    protected int position = 0;

    protected boolean isLocked = false;

    public boolean incognito;

    protected long accessTime;

    protected int parentId = Tab.INVALID_TAB_ID;

    @TabLaunchType
    protected int mLaunchType;

    protected transient final List<IPage> mPages = new ArrayList<>(0);

    public void setId(int tabId) {
        this.tabId = tabId;
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

    public int getCurrentPageId() {
        return currentPageId;
    }

    public TabInfo cloneTabInfo() {
        TabInfo newTabInfo = TabInfo.create();
        newTabInfo.tabId = ArkIdManager.generateTabId();
        newTabInfo.createTime = createTime;
        newTabInfo.pageIndex = pageIndex;
        newTabInfo.currentPageId = Tab.INVALID_PAGE_ID;
        newTabInfo.position = position;
        newTabInfo.isLocked = isLocked;
        newTabInfo.incognito = incognito;
        newTabInfo.accessTime = accessTime;
        newTabInfo.mLaunchType = mLaunchType;
        newTabInfo.parentId = parentId;
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
    }

    public void setCurrentPageId(int currentPageId) {
        this.currentPageId = currentPageId;
    }

    public int getId() {
        return tabId;
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

    public int getParentId() {
        return parentId;
    }

    public void setParentId(int parentId) {
        this.parentId = parentId;
    }

    public int getLaunchType() {
        return mLaunchType;
    }

    public void setLaunchType(@TabLaunchType int launchType) {
        this.mLaunchType = launchType;
    }

    public List<IPage> getPages() {
        return mPages;
    }

    public static TabInfo create() {
        return create(System.currentTimeMillis());
    }

    public static TabInfo create(long createTime) {
        TabInfo manager = new TabInfo();
        manager.createTime = createTime;
        manager.tabId = ArkIdManager.generateTabId();
        return manager;
    }

//    public static TabInfo from(TabInfo tabInfo) {
//        TabInfo newTabInfo = TabInfo.create();
//        newTabInfo.setTabId(tabInfo.getTabId());
//        newTabInfo.setCreateTime(tabInfo.getCreateTime());
//        newTabInfo.setPageIndex(tabInfo.getPageIndex());
//        newTabInfo.setCurrentTabId(tabInfo.getCurrentTabId());
//        newTabInfo.setPosition(tabInfo.getPosition());
//        newTabInfo.setLocked(tabInfo.isLocked());
//        newTabInfo.setIncognito(tabInfo.isIncognito());
//        return newTabInfo;
//    }

    public static TabInfo from(File tabFile) throws IOException {
        try (DataInputStream stream = ArkTabDao.readFile(tabFile)) {
            if (stream == null) {
                throw new IOException("tab file stream is null!");
            }
            return from(stream);
        }
    }

    public static TabInfo from(DataInputStream is) throws IOException {
        TabInfo newTabInfo = new TabInfo();
        int version = is.readInt();
        newTabInfo.setId(is.readInt());
        if (version == 2) {
            newTabInfo.setLaunchType(is.readInt());
        }
        newTabInfo.setCreateTime(is.readLong());
        newTabInfo.setIncognito(is.readBoolean());
        newTabInfo.setLocked(is.readBoolean());

        newTabInfo.setPageIndex(is.readInt());
        newTabInfo.setCurrentPageId(is.readInt());
        newTabInfo.setPosition(is.readInt());
        newTabInfo.setAccessTime(is.readLong());
        int count = is.readInt();
        ArkLogger.e(TabInfo.class, "TabInfo.from id=" + newTabInfo.getId() + " count=" + count);
        File pagesDir = ArkTabDao.getPagesDir(newTabInfo.getId());
        for (int i = 0; i < count; i++) {
            int pageId = is.readInt();
            File file = new File(pagesDir, String.valueOf(pageId));
            PageInfo pageInfo = PageInfo.from(file);
            ArkLogger.e(TabInfo.class, "TabInfo.from pageId=" + pageId + " pageInfo=" + pageInfo);
            newTabInfo.mPages.add(new PageImpl(pageInfo));
        }
        return newTabInfo;
    }

    @Override
    public String toString() {
        return "TabInfo{" +
                "tabId=" + tabId +
                ", createTime=" + createTime +
                ", pageIndex=" + pageIndex +
                ", currentPageId=" + currentPageId +
                ", position=" + position +
                ", isLocked=" + isLocked +
                ", incognito=" + incognito +
                ", accessTime=" + accessTime +
                ", parentId=" + parentId +
                ", mLaunchType=" + mLaunchType +
                ", mPages=" + mPages +
                '}';
    }

    public void save() {
        ThreadUtils.checkUiThread();
        try {
            long time = System.currentTimeMillis();
            ByteArrayOutputStream stream = new ByteArrayOutputStream();
            DataOutputStream os = new DataOutputStream(stream);
            int version = 2;
            os.writeInt(version);
            os.writeInt(tabId);
            os.writeInt(mLaunchType);
            os.writeLong(createTime);
            os.writeBoolean(incognito);
            os.writeBoolean(isLocked);
            os.writeInt(pageIndex);
            os.writeInt(currentPageId);
            os.writeInt(position);
            os.writeLong(accessTime);
            os.writeInt(mPages.size());
            ArkLogger.e(this, "saveTabInfo info=" + this
                    + " mPages=" + mPages);
            for (IPage page : mPages) {
                os.writeInt(page.getId());
            }
            os.close();

            byte[] bytes = stream.toByteArray();

            ArkLogger.e(this, "saveTabInfo to byte deltaTime="
                    + (System.currentTimeMillis() - time));

            ThreadPool.executeIO(() -> {
                long time1 = System.currentTimeMillis();
                File tabFile = ArkTabDao.getTabFile(tabId);
                AtomicFile file = new AtomicFile(tabFile);
                FileOutputStream fos = null;
                try {
                    fos = file.startWrite();
                    fos.write(bytes, 0, bytes.length);
                    file.finishWrite(fos);
                } catch (IOException e) {
                    if (fos != null) file.failWrite(fos);
                    ArkLogger.e(TabInfo.this, "Failed to write file: " + file.getBaseFile().getAbsolutePath());
                }
                ArkLogger.e(TabInfo.this, "saveTabInfo deltaTime="
                        + (System.currentTimeMillis() - time1));
            });
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

}
