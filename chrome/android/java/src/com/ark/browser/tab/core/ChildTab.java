package com.ark.browser.tab.core;

import android.util.AtomicFile;

import androidx.annotation.NonNull;

import com.ark.browser.core.ArkWebContents;
import com.ark.browser.core.ArkWebManager;
import com.ark.browser.tab.ArkTabImpl;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.PageSnapshotManager;
import com.ark.browser.tab.TabCacheManager;
import com.ark.browser.tab.TabInfo;
import com.ark.browser.tab.dao.ArkTabDao;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;
import com.zpj.utils.FileUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;

import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class ChildTab implements ITab, IPageGroup {

    private static final String TAG = "TabImpl";

    private final ITabGroup mParentTab;
    private final TabInfo mTabInfo;

    private transient ChildTab mFloatingTab;

    protected transient final List<IPage> mPages;

    public ChildTab(ITabGroup parent) {
        this(parent, TabInfo.create(parent.getId()));
    }

    public ChildTab(ITabGroup parent, TabInfo tabInfo) {
        this(parent, tabInfo, new ArrayList<>(0));
    }

    public ChildTab(ITabGroup parent, TabInfo tabInfo, List<IPage> pages) {
        mParentTab = parent;
        mTabInfo = tabInfo;
        mPages = pages;
    }

    @Override
    public List<IPage> getPages() {
        return mPages;
    }

    @Override
    public IPage getPageAt(int index) {
        if (index < 0 || index >= mPages.size()) {
            return null;
        }
        return mPages.get(index);
    }

    @Override
    public int getPageSize() {
        return mPages.size();
    }

    @Override
    public int indexOfPage(int pageId) {
        int id = mTabInfo.getCurrentPageId();
        if (pageId == id) {
            return mTabInfo.getChildIndex();
        }
        for (int j = 0; j < mPages.size(); j++) {
            if (mPages.get(j).getId() == pageId) {
                return j;
            }
        }
        return INVALID_TAB_INDEX;
    }

    @Override
    public ITabGroup getParentTab() {
        return mParentTab;
    }

    @Override
    public TabInfo getTabInfo() {
        return mTabInfo;
    }

    @Override
    public IPage findPageById(int pageId) {
        if (mTabInfo.getCurrentPageId() == pageId) {
            return getCurrentPage();
        }

        for (IPage page : mPages) {
            if (page.getId() == pageId) {
                return page;
            }
        }
        return null;
    }

    @Override
    public PageInfo getCurrentPageInfo() {
        return getPageInfoAt(mTabInfo.getChildIndex());
    }

    @Override
    public IPage getCurrentPage() {
        return getPageAt(mTabInfo.getChildIndex());
    }

//    @Override
//    public void exitFloatingTabInfo() {
//        ArkLogger.d(TAG, "exitFloatingTabInfo mFloatingTabInfo=" + mFloatingTab + " this=" + this);
//        if (mFloatingTab == null) {
//            return;
//        }
//
//        List<IPage> temp = new ArrayList<>(mPages);
//        TabInfo mFloatingTabInfo = mFloatingTab.getTabInfo();
//
//        mTabInfo.setId(mFloatingTabInfo.getId());
//        mTabInfo.setCreateTime(mFloatingTabInfo.getCreateTime());
//
//        mTabInfo.setChildIndex(mFloatingTabInfo.getChildIndex());
//        mTabInfo.setCurrentPageId(mFloatingTabInfo.getCurrentPageId());
//        mTabInfo.setPosition(mFloatingTabInfo.getPosition());
//        mTabInfo.setLocked(mFloatingTabInfo.isLocked());
//        mTabInfo.setIncognito(mFloatingTabInfo.isIncognito());
//
////        tabListFolder = mFloatingTab.getTabListFolder();
//
//        mPages.clear();
//        mPages.addAll(mFloatingTab.mPages);
//
//        mFloatingTab = null;
//
//        saveTabInfo();
//
//        ITabGroup tabGroup = TabGroupManager.getTabGroupById(mTabInfo.getParentId());
//        TabInfo currentTabInfo = tabGroup.getCurrentTabInfo();
//        ArkLogger.d(TAG, "exitFloatingTabInfo currentTabInfo=" + currentTabInfo + " this=" + this);
//        if (currentTabInfo != null
//                && currentTabInfo.getId() == mTabInfo.getId()) {
//            ArkLogger.d(TAG, "exitFloatingTabInfo tab=" + getCurrentPageInfo());
//            tabGroup.selectTab(this);
//        }
//
//        for (IPage page : temp) {
//            ArkWebManager.remove(page.getId());
//        }
//        temp.clear();
//    }

    @Override
    public ITab cloneTab() {
        if (mFloatingTab != null) {
            return mFloatingTab.cloneTab();
        }
        ChildTab newTab = new ChildTab(mParentTab, getTabInfo().cloneTabInfo());

        boolean encrypted = newTab.getTabInfo().isIncognito();
        for (int i = 0; i < mPages.size(); i++) {
            IPage page = mPages.get(i);
            IPage newPage = page.clone(newTab.getId());
            newTab.mPages.add(newPage);
            newPage.savePageInfo();

            ArkWebContents arkWeb = ArkWebManager.get(page.getId());
            TabState tabState = null;
            if (arkWeb != null && !arkWeb.isDestroyed()) {
                tabState = TabStateExtractor.from(arkWeb.getWebContents());
            }
            TabState finalTabState = tabState;
            ThreadPool.executeIO(() -> {
                File newPageFile = ArkTabDao.getTabStateFile(newPage.getId(), encrypted);
                if (finalTabState == null) {
                    File pageFile = ArkTabDao.getTabStateFile(page.getId(), encrypted);
                    if (pageFile.exists()) {
                        FileUtils.copyFileFast(pageFile, newPageFile);
                    }
                } else {
                    TabStateFileManager.saveState(newPageFile, finalTabState, encrypted);
                }
            });
            if (mTabInfo.getChildIndex() == i) {
                newTab.getTabInfo().setChildIndex(i);
                PageSnapshotManager.getInstance().copySnapshot(mTabInfo.getCurrentPageId(), newPage.getId());
            }
        }

        return newTab;
    }

    public IPage openNewPage() {
        int nextIndex;
        if (mPages.isEmpty()) {
            nextIndex = 0;
        } else {
            nextIndex = mTabInfo.getChildIndex() + 1;
        }
        PageInfo pageInfo = PageInfo.from(getId(), nextIndex, getTabInfo().isIncognito());

        IPage page = new PageImpl(pageInfo);

        mPages.add(nextIndex, page);

        if (++nextIndex < mPages.size()) {
            List<IPage> pageRemoved = mPages.subList(nextIndex, mPages.size());
            List<IPage> tempPages = new ArrayList<>(pageRemoved);
            ThreadPool.postOnUIThread(() -> {
                long start = System.currentTimeMillis();
                ArkLogger.d(this, "openNewPage pageRemovedCount=" + tempPages.size());
                for (IPage info : tempPages) {
                    info.remove();
                }
                ArkLogger.d(this, "openNewPage pageRemoved deltaTime=" + (System.currentTimeMillis() - start));
            });
            pageRemoved.clear();
        }
        return page;
    }

    @Override
    public void remove() {
        deleteTabInfo();
        for (IPage page : mPages) {
            PageSnapshotManager.getInstance().removeSnapshot(page.getPageInfo().getId());
            page.deletePageInfo();
        }
        destroy();
    }

    @Override
    public void saveTabInfo() {
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
            os.writeInt(tabInfo.getChildIndex());
            os.writeInt(tabInfo.getCurrentPageId());
            os.writeInt(tabInfo.getPosition());
            os.writeLong(tabInfo.getAccessTime());
            os.writeInt(mPages.size());

            ArkLogger.e(this, "saveTabInfo info=" + tabInfo
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
                File tabFile = ArkTabDao.getTabFile(getId());
                AtomicFile file = new AtomicFile(tabFile);
                FileOutputStream fos = null;
                try {
                    fos = file.startWrite();
                    fos.write(bytes, 0, bytes.length);
                    file.finishWrite(fos);
                } catch (IOException e) {
                    if (fos != null) file.failWrite(fos);
                    ArkLogger.e(ChildTab.this, "Failed to write file: " + file.getBaseFile().getAbsolutePath());
                }
                ArkLogger.e(ChildTab.this, "saveTabInfo deltaTime="
                        + (System.currentTimeMillis() - time1));
            });
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void destroy() {
        TabCacheManager.getInstance().removeTab(getId());
        for (IPage page : mPages) {
            ArkWebManager.remove(page.getId());
        }
        mPages.clear();
    }

    @Override
    public String toString() {
        return "TabImpl{" +
                "tabInfo=" + mTabInfo +
                '}';
    }

    /**
     * TODO TabRestore
     */
    public void deleteTabInfo() {
        File tabFile = ArkTabDao.getTabFile(getId());
        tabFile.delete();
    }

    public boolean removePage(IPage page) {
        int i = indexOfPage(page.getId());
        PageInfo pre = getPageInfoAt(i - 1);
        PageInfo next = getPageInfoAt(i + 1);
        // 移除当前page，优先显示前一个page
        if (pre != null) {
            getTabInfo().setChildIndex(getTabInfo().getChildIndex() - 1);
            getTabInfo().setCurrentPageId(pre.getId());
        } else if (next != null) {
            getTabInfo().setCurrentPageId(next.getId());
        } else {
            return false;
        }

        mPages.remove(i);

        ArkTabImpl tab = (ArkTabImpl) TabCacheManager.getInstance().findTab(getId());
        if (tab != null) {
            tab.removePage(page);
        }
        page.deletePageInfo();
        saveTabInfo();
        return true;
    }

    private PageInfo getPageInfoAt(int index) {
        IPage page = getPageAt(index);
        if (page == null) {
            return null;
        }
        return page.getPageInfo();
    }


    public static ChildTab from(ITabGroup parent, int tabId) {
        File tabFile = ArkTabDao.getTabFile(tabId);
        ArkLogger.e(ChildTab.class, "from id=" + tabId + " tabFile=" + tabFile);
        try {
            return from(parent, tabFile);
        } catch (IOException e) {
            e.printStackTrace();
            return new ChildTab(parent, TabInfo.create(tabId, -1));
        }
    }

    private static ChildTab from(ITabGroup parent, File tabFile) throws IOException {
        try (DataInputStream stream = ArkTabDao.readFile(tabFile)) {
            if (stream == null) {
                throw new IOException("tab file stream is null!");
            }
            return from(parent, stream);
        }
    }

    private static ChildTab from(ITabGroup parent, DataInputStream is) throws IOException {
        List<IPage> pages = new ArrayList<>();
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
        return new ChildTab(parent, newTabInfo, pages);
    }

    private static ITab restoreTab(@NonNull ITabGroup parent, DataInputStream is) throws IOException {
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
        ArkLogger.e(ChildTab.class, "restoreTab version=" + version + " parentId=" + newTabInfo.getParentId());
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

        int count = is.readInt();
        ArkLogger.e(TabInfo.class, "restoreTab id=" + newTabInfo.getId() + " count=" + count);

        boolean isGroup = newTabInfo.isGroup();

        if (isGroup) {
            List<ITab> tabs = new ArrayList<>();
            for (int i = 0; i < count; i++) {
                int childId = is.readInt();
                ITab newTab = ChildTab.from(parent, childId);
                ArkLogger.e(TAG, "restoreTab tabInfo=" + newTab.getTabInfo());
                tabs.add(newTab);
            }
            return new GroupTab(parent, newTabInfo, tabs);
        } else {
            File pagesDir = ArkTabDao.getPagesDir(newTabInfo.getId());
            List<IPage> pages = new ArrayList<>();
            for (int i = 0; i < count; i++) {
                int pageId = is.readInt();
                File file = new File(pagesDir, String.valueOf(pageId));
                PageInfo pageInfo = PageInfo.from(file);
                ArkLogger.e(TabInfo.class, "restoreTab pageId=" + pageId + " pageInfo=" + pageInfo);
                pages.add(new PageImpl(pageInfo));
            }
            return new ChildTab(parent, newTabInfo, pages);
        }
    }
}
