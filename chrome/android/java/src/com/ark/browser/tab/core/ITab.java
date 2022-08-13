package com.ark.browser.tab.core;

import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.TabInfo;

import org.chromium.base.Log;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tab.TabState;

import java.io.File;

public interface ITab {

    public static final int INVALID_TAB_INDEX = -1;

    String getId();

    TabInfo getTabInfo();

    File getTabListFolder();

    IPageGroup getPageGroup();

    default int getPageSize() {
        return getPageGroup().getCount();
    }

    default IPage getPageAt(int index) {
        return getPageGroup().getPageAt(index);
    }

    default PageInfo getPageInfoAt(int index) {
        IPage page = getPageAt(index);
        if (page == null) {
            return null;
        }
        return page.getPageInfo();
    }

    default PageInfo getCurrentPageInfo() {
        int pageIndex = getTabInfo().getIndex();
        return getPageGroup().getPageInfoAt(pageIndex);
    }

    default PageInfo getPreviousPageInfo() {
        int pageIndex = getTabInfo().getIndex();
        return getPageGroup().getPageInfoAt(pageIndex - 1);
    }

    default PageInfo getNextPageInfo() {
        int pageIndex = getTabInfo().getIndex();
        return getPageGroup().getPageInfoAt(pageIndex + 1);
    }

    default IPage getCurrentPage() {
        int pageIndex = getTabInfo().getIndex();
        return getPageGroup().getPageAt(pageIndex);
    }

    default IPage getPreviousPage() {
        int pageIndex = getTabInfo().getIndex();
        return getPageGroup().getPageAt(pageIndex - 1);
    }

    default IPage getNextPage() {
        int pageIndex = getTabInfo().getIndex();
        return getPageGroup().getPageAt(pageIndex + 1);
    }

//    default Tab getCurrentPage() {
//        PageInfo pageInfo = getCurrentPageInfo();
//        if (pageInfo == null) {
//            return null;
//        }
//        return PageCacheManager.getInstance().findPage(pageInfo);
//    }
//
//    default Tab getPreviousPage() {
//        PageInfo pageInfo = getPreviousPageInfo();
//        if (pageInfo == null) {
//            return null;
//        }
//        return PageCacheManager.getInstance().findPage(pageInfo);
//    }
//
//    default Tab getNextPage() {
//        PageInfo pageInfo = getNextPageInfo();
//        if (pageInfo == null) {
//            return null;
//        }
//        return PageCacheManager.getInstance().findPage(pageInfo);
//    }

    default int getCurrentPageId() {
        PageInfo pageInfo = getCurrentPageInfo();
        if (pageInfo == null) {
            return Tab.INVALID_PAGE_ID;
        } else {
            return pageInfo.getPageId();
        }
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
        return indexOfPage(pageInfo.getPageId());
    }

    default int indexOfPage(int pageId) {
        int id = getCurrentPageId();
        if (pageId == id) {
            Log.d("ITab", "getTabIndexById return index=" + getPageGroup().getIndex());
            return getTabInfo().getPageIndex();
        }
        for (int j = 0; j < getPageSize(); j++) {
            if (getPageInfoAt(j).getPageId() == pageId) {
                return j;
            }
        }
        return INVALID_TAB_INDEX;
    }

    default PageInfo getPageInfoById(int pageId) {
        int i = indexOfPage(pageId);
        return getPageInfoAt(i);
    }

    default IPage getPageById(int pageId) {
        int i = indexOfPage(pageId);
        return getPageAt(i);
    }



    default ITab cloneTab() {

        TabInfo tabInfo = getTabInfo();

        TabInfo newTabInfo = TabInfo.create();
        newTabInfo.setPageIndex(tabInfo.getPageIndex());
        newTabInfo.setCurrentTabId(Tab.INVALID_PAGE_ID);
        newTabInfo.setLocked(tabInfo.isLocked());
        newTabInfo.setIncognito(tabInfo.isIncognito());
        newTabInfo.setAccessTime(tabInfo.getAccessTime());
        ITab newTab = new TabImpl(newTabInfo);

//        Log.d(getClass().getSimpleName(), "cloneTab index=" + tabInfo.getPageIndex()
//                + " currentTabId=" + tabInfo.getCurrentTabId());
//
//        PageInfo parentInfo = null;
//        for (int i = 0; i < getPageGroup().getCount(); i++) {
//            PageInfo tab = getPageGroup().getPageInfoAt(i);
//            Log.d(getClass().getSimpleName(), "cloneTab id=" + tab.getPageId());
//            TabState state = TabState.restoreTabState(getTabListFolder(), tab.getPageId());
//            if (state == null) {
//                continue;
//            }
//
//            // 创建新的pageInfo
//            int newId = TabIdManager.getInstance().generateValidId(Tab.INVALID_PAGE_ID);
//            PageInfo newPageInfo = PageInfo.from(tab);
//            newPageInfo.setPageId(newId);
//            newPageInfo.setTabInfoId(newTabInfo.getTabInfoId());
//            if (parentInfo == null) {
//                newPageInfo.setParentId(tab.getParentId());
//            } else {
//                newPageInfo.setParentId(newTab.getPageInfoAt(i - 1).getPageId());
//            }
//
//
//            // 复制TabState
//            state.parentId = newPageInfo.getParentId();
////            TabState.saveState(
////                    TabState.getTabStateFile(newTab.getTabListFolder(), newId, newPageInfo.isIncognito()),
////                    state,
////                    newPageInfo.isIncognito()
////            );
//
//            parentInfo = newPageInfo;
//
//            newTab.getPageGroup().getPageInfoList().add(new PageImpl(newPageInfo));
//            if (tabInfo.getPageIndex() == i) {
//                newTabInfo.setCurrentTabId(newId);
//            }
//        }
        return newTab;
    }

    default void selectPage(int index) {
        if (index < 0) {
            return;
        }
        IPage page = getPageGroup().getPageAt(index);
        getTabInfo().setCurrentTabId(page.getId());
        getTabInfo().setPageIndex(index);
        getTabInfo().save();
    }

    default void selectPage(IPage page) {
        if (getTabInfo().getCurrentTabId() == page.getId()) {
            return;
        }
        selectPage(getPageGroup().indexOf(page));
    }

    default boolean removePage(IPage page) {
        int i = getPageGroup().indexOf(page);
        PageInfo pre = getPageInfoAt(i - 1);
        PageInfo next = getPageInfoAt(i + 1);
        // 移除当前page，优先显示前一个page
        if (pre != null) {
            getTabInfo().setCurrentTabId(pre.getPageId());
            getTabInfo().setPageIndex(getTabInfo().getPageIndex() - 1);
        } else if (next != null) {
            next.setParentId(page.getPageInfo().getParentId());
            getTabInfo().setCurrentTabId(next.getPageId());
        } else {
            return false;
        }
        getTabInfo().save();
        getPageGroup().removePage(page);
        page.getPageInfo().delete();
        return true;
    }


    default void destroy() {
        getPageGroup().destroy();
    }

    public void exitFloatingTabInfo();

    default void remove() {
        getPageGroup().remove();
        getTabInfo().delete();
    }

}
