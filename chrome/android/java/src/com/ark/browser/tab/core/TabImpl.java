package com.ark.browser.tab.core;

import com.ark.browser.core.ArkWebContents;
import com.ark.browser.core.ArkWebManager;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.PageSnapshotManager;
import com.ark.browser.tab.TabCacheManager;
import com.ark.browser.tab.TabGroupManager;
import com.ark.browser.tab.TabInfo;
import com.ark.browser.tab.dao.ArkTabDao;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;
import com.zpj.utils.FileUtils;

import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class TabImpl implements ITab {

    private static final String TAG = "TabImpl";

    private final TabInfo tabInfo;

    private transient ITab mFloatingTab;

    protected transient final List<IPage> mPages;

    public TabImpl(int parentId) {
        this(TabInfo.create(parentId));
    }

    public TabImpl(TabInfo tabInfo) {
        this(tabInfo, new ArrayList<>(0));
    }

    public TabImpl(TabInfo tabInfo, List<IPage> pages) {
        this.tabInfo = tabInfo;
        mPages = pages;
    }

    @Override
    public TabInfo getTabInfo() {
        return tabInfo;
    }

    @Override
    public List<IPage> getPages() {
        return mPages;
    }

    @Override
    public boolean hasPage(int id) {
        if (mFloatingTab != null && mFloatingTab.hasPage(id)) {
            return true;
        }
        return ITab.super.hasPage(id);
    }

    @Override
    public PageInfo getPageInfoById(int pageId) {
        IPage page = getPageById(pageId);
        if (page == null) {
            return null;
        }
        return page.getPageInfo();
    }

    @Override
    public IPage getPageById(int pageId) {
        if (getCurrentPageId() == pageId) {
            return getCurrentPage();
        }

        for (IPage page : getPages()) {
            if (page.getId() == pageId) {
                return page;
            }
        }
        return null;
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

        ITabGroup tabGroup = TabGroupManager.getTabGroupById(tabInfo.getParentId());
        TabInfo currentTabInfo = tabGroup.getCurrentTabInfo();
        ArkLogger.d(TAG, "exitFloatingTabInfo currentTabInfo=" + currentTabInfo + " this=" + this);
        if (currentTabInfo != null
                && currentTabInfo.getId() == tabInfo.getId()) {
            ArkLogger.d(TAG, "exitFloatingTabInfo tab=" + getCurrentPageInfo());
            tabGroup.selectTab(this);
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
        ITab newTab = new TabImpl(getTabInfo().cloneTabInfo());

        boolean encrypted = newTab.getTabInfo().isIncognito();
        for (int i = 0; i < getPageSize(); i++) {
            IPage page = getPageAt(i);
            IPage newPage = page.clone(newTab.getId());
            newTab.getPages().add(newPage);
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
            if (getPageIndex() == i) {
                newTab.getTabInfo().setPageIndex(i);
                PageSnapshotManager.getInstance().copySnapshot(getCurrentPageId(), newPage.getId());
            }
        }

        return newTab;
    }

    @Override
    public IPage openNewPage() {
        int nextIndex;
        if (getPageSize() == 0) {
            nextIndex = 0;
        } else {
            nextIndex = getPageIndex() + 1;
        }
        PageInfo pageInfo = PageInfo.from(getId(), nextIndex, getTabInfo().isIncognito());

        IPage page = new PageImpl(pageInfo);

        getPages().add(nextIndex, page);

        if (++nextIndex < getPageSize()) {
            List<IPage> pageRemoved = getPages().subList(nextIndex, getPageSize());
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
    public void destroy() {
        TabCacheManager.getInstance().removeTab(getId());
        for (IPage page : getPages()) {
            ArkWebManager.remove(page.getId());
        }
        getPages().clear();
    }

    @Override
    public String toString() {
        return "TabImpl{" +
                "tabInfo=" + tabInfo +
                '}';
    }
}
