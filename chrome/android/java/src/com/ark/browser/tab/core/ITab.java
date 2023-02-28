package com.ark.browser.tab.core;

import com.ark.browser.core.ArkWebContents;
import com.ark.browser.core.ArkWebManager;
import com.ark.browser.tab.ArkTabImpl;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.PageSnapshotManager;
import com.ark.browser.tab.TabCacheManager;
import com.ark.browser.tab.TabInfo;
import com.ark.browser.tab.dao.ArkTabDao;
import com.ark.browser.utils.ThreadPool;
import com.zpj.utils.FileUtils;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;

import java.io.File;
import java.util.List;

public interface ITab {

    int INVALID_TAB_INDEX = -1;

    default int getId() {
        return getTabInfo().getId();
    }

    default String getGroupId() {
        return getTabInfo().getGroupId();
    }

    TabInfo getTabInfo();

    default List<IPage> getPages() {
        return getTabInfo().getPages();
    }

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

    default PageInfo getPreviousPageInfo() {
        return getPageInfoAt(getPageIndex() - 1);
    }

    default PageInfo getNextPageInfo() {
        return getPageInfoAt(getPageIndex() + 1);
    }

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

    default PageInfo getPageInfoById(int pageId) {
        IPage page = getPageById(pageId);
        if (page == null) {
            return null;
        }
        return page.getPageInfo();
    }

    default IPage getPageById(int pageId) {
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



    default ITab cloneTab() {

        TabInfo newTabInfo = getTabInfo().cloneTabInfo();

        boolean encrypted = newTabInfo.isIncognito();
        for (int i = 0; i < getPageSize(); i++) {
            IPage page = getPageAt(i);
            IPage newPage = page.clone(newTabInfo.getId());
            newTabInfo.getPages().add(newPage);
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
                newTabInfo.setPageIndex(i);
                PageSnapshotManager.getInstance().copySnapshot(getCurrentPageId(), newPage.getId());
            }
        }

        return new TabImpl(newTabInfo);
    }

//    default IPage createPage(Tab parent, LoadUrlParams params, @TabLaunchType int type) {
//        int parentId = parent.getId();
//        int index = indexOfPage(parentId);
//        ArkLogger.d(this, "openNewPage index=" + index);
//        if (index == ITab.INVALID_TAB_INDEX) {
//            return null;
//        }
//
//        PageInfo parentPageInfo = getPageInfoAt(index);
//        ArkLogger.d(this, "openNewPage parentPageInfo=" + parentPageInfo);
//        if (parentPageInfo == null) {
//            return null;
//        }
//
//        int pageIndex = parentPageInfo.getOriginalIndex() + 1;
////        ArkTabImpl tab = PageCacheManager.getInstance().createLivePageByType(this, params, type);
//
//        PageInfo pageInfo = PageInfo.from(getTabInfo().getId(), pageIndex,
//                getTabInfo().isIncognito());
//        IPage page = new PageImpl(pageInfo);
//        IPageGroup pageInfoList = getPageGroup();
//
//        pageInfoList.getPageInfoList().add(++index, page);
//
//        ArkLogger.d(this, "openNewPage params=" + params);
////        tab.loadUrl(params);
//
//        if (++index < pageInfoList.getCount()) {
//            List<IPage> pageRemoved = pageInfoList.getPageInfoList()
//                    .subList(index, pageInfoList.getCount());
//
//            List<IPage> tempPages = new ArrayList<>(pageRemoved);
//            ThreadPool.postOnUIThread(() -> {
//                long start = System.currentTimeMillis();
//                ArkLogger.d(ITab.this, "openNewPage pageRemovedCount=" + tempPages.size());
//
//                for (IPage info : tempPages) {
//                    info.remove();
//                }
//
//                ArkLogger.d(ITab.this, "openNewPage pageRemoved deltaTime=" + (System.currentTimeMillis() - start));
//            });
//            pageRemoved.clear();
//        }
//
//        return page;
//    }

    default void selectPage(int index) {
        IPage page = getPageAt(index);
        if (page == null) {
            return;
        }
        getTabInfo().setCurrentPageId(page.getId());
        getTabInfo().setPageIndex(index);
        saveTabInfo();
    }

    default void selectPage(IPage page) {
        if (getCurrentPageId() == page.getId()) {
            return;
        }
        selectPage(indexOfPage(page.getId()));
    }

    default boolean removePage(IPage page) {
        int i = indexOfPage(page);
        PageInfo pre = getPageInfoAt(i - 1);
        PageInfo next = getPageInfoAt(i + 1);
        // 移除当前page，优先显示前一个page
        if (pre != null) {
            getTabInfo().setCurrentPageId(pre.getId());
            getTabInfo().setPageIndex(getTabInfo().getPageIndex() - 1);
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


    default void destroy() {
        TabCacheManager.getInstance().removeTab(getId());
        for (IPage page : getPages()) {
            ArkWebManager.remove(page.getId());
        }
        getPages().clear();
    }

    public void exitFloatingTabInfo();

    default void remove() {
        for (IPage page : getPages()) {
            PageSnapshotManager.getInstance().removeSnapshot(page.getPageInfo().getId());
            page.deletePageInfo();
        }
        deleteTabInfo();
        destroy();
    }

    default void deleteTabInfo() {
        File tabFile = ArkTabDao.getTabFile(getId());
        tabFile.delete();
    }

    default void saveTabInfo() {
        getTabInfo().save();
    }

}
