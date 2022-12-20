package com.ark.browser.tab;

import androidx.annotation.Keep;

import com.ark.browser.core.utils.ArkIdManager;
import com.ark.browser.tab.dao.ArkTabDao;

import org.chromium.chrome.browser.tab.Tab;

import java.io.DataInputStream;
import java.io.File;
import java.io.IOException;
import java.util.List;

@Keep
//@Table(database = TabInfoManager.class)
public class TabInfo {

    private static final String TAG = "TabManager";

//    protected transient final List<PageInfo> mPageInfoList = new ArrayList<>();

//    @PrimaryKey()
    protected int tabId;

//    @Column
    protected long createTime;

//    @Column
    protected int pageIndex = 0;

//    @Column
    protected int currentPageId = Tab.INVALID_PAGE_ID;

//    @Column
    protected int position = 0;

//    @Column
    protected boolean isLocked = false;

//    @Column
    public boolean incognito;

//    @Column
    protected long accessTime;

    protected int parentId;

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
//        newTabInfo.tabId = tabId;
        newTabInfo.createTime = createTime;
        newTabInfo.pageIndex = pageIndex;
        newTabInfo.currentPageId = currentPageId;
        newTabInfo.position = position;
        newTabInfo.isLocked = isLocked;
        newTabInfo.incognito = incognito;
        newTabInfo.accessTime = accessTime;
//        newTabInfo.mPageInfoList.addAll(mPageInfoList);
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
//        save();
    }

    public void setCurrentTabId(int currentPageId) {
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

    //    public List<PageInfo> getPageInfoList() {
//        return mPageInfoList;
//    }

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

    public static TabInfo from(File tabFile, List<Integer> pageIds) throws IOException {
        try (DataInputStream stream = ArkTabDao.readFile(tabFile)) {
            if (stream == null) {
                throw new IOException("tab file stream is null!");
            }
            return from(stream, pageIds);
        }
    }

    public static TabInfo from(DataInputStream is, List<Integer> pageIds) throws IOException {
        TabInfo newTabInfo = new TabInfo();
        int version = is.readInt();
        newTabInfo.setId(is.readInt());
        newTabInfo.setCreateTime(is.readLong());
        newTabInfo.setIncognito(is.readBoolean());
        newTabInfo.setLocked(is.readBoolean());

        newTabInfo.setPageIndex(is.readInt());
        newTabInfo.setCurrentTabId(is.readInt());
        newTabInfo.setPosition(is.readInt());
        newTabInfo.setAccessTime(is.readLong());
        int count = is.readInt();
//        File pagesDir = ArkTabDao.getPagesDir(newTabInfo.getTabInfoId());
        for (int i = 0; i < count; i++) {
            int pageId = is.readInt();
            pageIds.add(pageId);
//            File file = new File(pagesDir, String.valueOf(pageId));
//            newTabInfo.mPageInfoList.add(PageInfo.from(file));
        }
        return newTabInfo;
    }

//    @Override
//    public void save() {
//        ThreadPool.executeIO(() -> {
//            File tabFile = ArkTabDao.getTabFile(tabInfoId);
//            try (DataOutputStream os = new DataOutputStream(
//                    new BufferedOutputStream(new FileOutputStream(tabFile)))) {
//                int version = 1;
//                os.writeInt(version);
//                os.writeInt(tabInfoId);
//                os.writeLong(createTime);
//                os.writeBoolean(incognito);
//                os.writeBoolean(isLocked);
//                os.writeInt(pageIndex);
//                os.writeInt(currentTabId);
//                os.writeInt(position);
//                os.writeLong(accessTime);
//                os.writeInt(mPageInfoList.size());
//                for (PageInfo info : mPageInfoList) {
//                    os.writeInt(info.getPageId());
//                }
//                os.flush();
//            } catch (IOException e) {
//                e.printStackTrace();
//            }
//        });
//    }
//
//    @Override
//    public void deleteSync() {
//        File tabFile = ArkTabDao.getTabFile(tabInfoId);
//        tabFile.delete();
//    }

    @Override
    public String toString() {
        return "TabInfo{" +
                ", tabId=" + tabId +
                ", createTime=" + createTime +
                ", pageIndex=" + pageIndex +
                ", currentPageId=" + currentPageId +
                ", position=" + position +
                ", isLocked=" + isLocked +
                ", incognito=" + incognito +
                '}';
    }

//    public void saveTabInfo() {
//        try {
//            long time = System.currentTimeMillis();
//            ByteArrayOutputStream stream = new ByteArrayOutputStream();
//            DataOutputStream os = new DataOutputStream(stream);
//            int version = 1;
//            os.writeInt(version);
//            os.writeInt(getTabInfoId());
//            os.writeLong(getCreateTime());
//            os.writeBoolean(isIncognito());
//            os.writeBoolean(isLocked());
//            os.writeInt(getPageIndex());
//            os.writeInt(getCurrentTabId());
//            os.writeInt(getPosition());
//            os.writeLong(getAccessTime());
//            os.writeInt(getPageSize());
//            ArkLogger.e(this, "saveTabInfo info=" + this
//                    + " pageSize=" + getPageSize());
//            for (IPage page : getPageGroup().getPageInfoList()) {
//                os.writeInt(page.getId());
//            }
//            os.close();
//
//            byte[] bytes = stream.toByteArray();
//
//            ArkLogger.e(this, "saveTabInfo to byte deltaTime="
//                    + (System.currentTimeMillis() - time));
//
//            ThreadPool.executeIO(() -> {
//                long time1 = System.currentTimeMillis();
//                File tabFile = ArkTabDao.getTabFile(getTabInfoId());
//                AtomicFile file = new AtomicFile(tabFile);
//                FileOutputStream fos = null;
//                try {
//                    fos = file.startWrite();
//                    fos.write(bytes, 0, bytes.length);
//                    file.finishWrite(fos);
//                } catch (IOException e) {
//                    if (fos != null) file.failWrite(fos);
//                    ArkLogger.e(TabInfo.this, "Failed to write file: " + file.getBaseFile().getAbsolutePath());
//                }
//                ArkLogger.e(TabInfo.this, "saveTabInfo deltaTime="
//                        + (System.currentTimeMillis() - time1));
//            });
//        } catch (IOException e) {
//            e.printStackTrace();
//        }
//    }

}
