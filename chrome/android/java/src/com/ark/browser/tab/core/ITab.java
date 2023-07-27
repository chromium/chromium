package com.ark.browser.tab.core;

import android.util.AtomicFile;

import com.ark.browser.tab.ArkTabImpl;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.PageSnapshotManager;
import com.ark.browser.tab.TabCacheManager;
import com.ark.browser.tab.TabInfo;
import com.ark.browser.tab.TabGroupManager;
import com.ark.browser.tab.dao.ArkTabDao;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tab.Tab;

import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public interface ITab {

    int INVALID_TAB_INDEX = -1;

    IPage openNewPage();




    default int getId() {
        return getTabInfo().getId();
    }

    default int getParentId() {
        return getTabInfo().getParentId();
    }

    TabInfo getTabInfo();

    List<IPage> getPages();

    default int getPageSize() {
        return getPages().size();
    }

    default int getPageIndex() {
        return getTabInfo().getPageIndex();
    }

    default IPage getPageAt(int index) {
        if (index < 0 || index >= getPageSize()) {
            return null;
        }
        return getPages().get(index);
    }

    default PageInfo getPageInfoAt(int index) {
        IPage page = getPageAt(index);
        if (page == null) {
            return null;
        }
        return page.getPageInfo();
    }

    default PageInfo getCurrentPageInfo() {
        return getPageInfoAt(getPageIndex());
    }

//    default PageInfo getPreviousPageInfo() {
//        return getPageInfoAt(getPageIndex() - 1);
//    }
//
//    default PageInfo getNextPageInfo() {
//        return getPageInfoAt(getPageIndex() + 1);
//    }

    default IPage getCurrentPage() {
        return getPageAt(getPageIndex());
    }

    default IPage getPreviousPage() {
        return getPageAt(getPageIndex() - 1);
    }

    default IPage getNextPage() {
        return getPageAt(getPageIndex() + 1);
    }

    default int getCurrentPageId() {
        return getTabInfo().getCurrentPageId();
//        PageInfo pageInfo = getCurrentPageInfo();
//        if (pageInfo == null) {
//            return Tab.INVALID_PAGE_ID;
//        } else {
//            return pageInfo.getId();
//        }
    }

    default boolean hasPage(Tab tab) {
        return hasPage(tab.getId());
    }

    default boolean hasPage(int id) {
        return indexOfPage(id) >= 0;
    }

    default int indexOfPage(PageInfo pageInfo) {
        if (pageInfo == null) {
            return INVALID_TAB_INDEX;
        }
        return indexOfPage(pageInfo.getId());
    }

    default int indexOfPage(int pageId) {
        int id = getCurrentPageId();
        if (pageId == id) {
            return getPageIndex();
        }
        for (int j = 0; j < getPageSize(); j++) {
            if (getPageAt(j).getId() == pageId) {
                return j;
            }
        }
        return INVALID_TAB_INDEX;
    }

    default int indexOfPage(IPage page) {
        return indexOfPage(page.getId());
    }

    PageInfo getPageInfoById(int pageId);

    IPage getPageById(int pageId);



    ITab cloneTab();

    default ITabGroup getTabGroup() {
        return TabGroupManager.getTabGroupById(getParentId());
    }

    default void selectTab() {
        selectPage(getCurrentPage());
    }

    default void selectPage(IPage page) {
        getTabGroup().selectTab(this, page);
    }

    default boolean removePage(IPage page) {
        int i = indexOfPage(page);
        PageInfo pre = getPageInfoAt(i - 1);
        PageInfo next = getPageInfoAt(i + 1);
        // 移除当前page，优先显示前一个page
        if (pre != null) {
            getTabInfo().setPageIndex(getTabInfo().getPageIndex() - 1);
            getTabInfo().setCurrentPageId(pre.getId());
        } else if (next != null) {
            getTabInfo().setCurrentPageId(next.getId());
        } else {
            return false;
        }

        getPages().remove(i);

        ArkTabImpl tab = (ArkTabImpl) TabCacheManager.getInstance().findTab(getId());
        if (tab != null) {
            tab.removePage(page);
        }
        page.deletePageInfo();
        saveTabInfo();
        return true;
    }


    void destroy();

    void exitFloatingTabInfo();

    default void remove() {
        deleteTabInfo();
        for (IPage page : getPages()) {
            PageSnapshotManager.getInstance().removeSnapshot(page.getPageInfo().getId());
            page.deletePageInfo();
        }
        destroy();
    }

    default void deleteTabInfo() {
        File tabFile = ArkTabDao.getTabFile(getId());
        tabFile.delete();
    }

    default void saveTabInfo() {
        ThreadUtils.checkUiThread();
        try {
            TabInfo tabInfo = getTabInfo();
            long time = System.currentTimeMillis();
            ByteArrayOutputStream stream = new ByteArrayOutputStream();
            DataOutputStream os = new DataOutputStream(stream);
            int version = 5;
            os.writeInt(version);
            os.writeInt(tabInfo.getId());
            os.writeInt(tabInfo.getParentId());
            os.writeBoolean(tabInfo.isGroup());
            os.writeInt(tabInfo.getLaunchType());
            os.writeLong(tabInfo.getCreateTime());
            os.writeBoolean(tabInfo.isIncognito());
            os.writeBoolean(tabInfo.isLocked());
            os.writeInt(tabInfo.getPageIndex());
            os.writeInt(tabInfo.getCurrentPageId());
            os.writeInt(tabInfo.getPosition());
            os.writeLong(tabInfo.getAccessTime());
            os.writeInt(getPages().size());

            ArkLogger.e(this, "saveTabInfo info=" + tabInfo
                    + " mPages=" + getPages());
            for (IPage page : getPages()) {
                os.writeInt(page.getId());
            }
            os.close();

            byte[] bytes = stream.toByteArray();

            ArkLogger.e(this, "saveTabInfo to byte deltaTime="
                    + (System.currentTimeMillis() - time));

            ThreadPool.executeIO(() -> {
                long time1 = System.currentTimeMillis();
                File tabFile = ArkTabDao.getTabFile(getId());
                AtomicFile file = new AtomicFile(tabFile);
                FileOutputStream fos = null;
                try {
                    fos = file.startWrite();
                    fos.write(bytes, 0, bytes.length);
                    file.finishWrite(fos);
                } catch (IOException e) {
                    if (fos != null) file.failWrite(fos);
                    ArkLogger.e(ITab.this, "Failed to write file: " + file.getBaseFile().getAbsolutePath());
                }
                ArkLogger.e(ITab.this, "saveTabInfo deltaTime="
                        + (System.currentTimeMillis() - time1));
            });
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    static TabImpl from(File tabFile) {
        try (DataInputStream stream = ArkTabDao.readFile(tabFile)) {
            if (stream == null) {
                throw new IOException("tab file stream is null!");
            }
            return from(stream);
        } catch (Exception e) {
            ArkLogger.e(TabInfo.class, "read info failed!", e);
            return new TabImpl(new TabInfo());
        }
    }

    static TabImpl from(DataInputStream is) throws IOException {
        TabInfo newTabInfo = new TabInfo();
        List<IPage> pages = new ArrayList<>();
        int version = is.readInt();
        newTabInfo.setId(is.readInt());
        if (version >= 3) {
            if (version >= 5) {
                newTabInfo.setParentId(is.readInt());
            } else {
                String name = is.readUTF();
                if ("group_incognito".equals(name)) {
                    newTabInfo.setParentId(-101);
                } else {
                    newTabInfo.setParentId(-100);
                }
            }
        }
        ArkLogger.e(TabImpl.class, "from version=" + version + " parentId=" + newTabInfo.getParentId());
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
            pages.add(new PageImpl(pageInfo));
        }
        return new TabImpl(newTabInfo, pages);
    }

}
