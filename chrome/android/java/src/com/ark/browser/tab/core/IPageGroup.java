package com.ark.browser.tab.core;

import com.ark.browser.tab.PageSnapshotManager;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.utils.ArkLogger;

import java.util.List;

public interface IPageGroup {


    IPageGroup clone();

    int getIndex();

    default int getCount() {
        return getPageList().size();
    }


    List<IPage> getPageList();

    default IPage getPageAt(int i) {
        ArkLogger.e(this, "getPageAt i=" + i + " count=" + getCount());
        if (i < 0 || i >= getCount()) {
            return null;
        }
        return getPageList().get(i);
    }



    default int indexOf(IPage target) {
        for (int i = 0; i < getCount(); i++) {
            IPage page = getPageAt(i);
            if (page.getId() == target.getId()) {
                return i;
            }
        }
        return -1;
    }

    default void addPage(IPage page) {
        getPageList().add(page);
    }

    default boolean removePage(IPage page) {
        int index = indexOf(page);
        if (index < 0) {
            return false;
        }
        return getPageList().remove(index) != null;
    }

    default PageInfo getPageInfoAt(int i) {
        IPage page = getPageAt(i);
        if (page == null) {
            return null;
        }
        return page.getPageInfo();
    }

    default PageInfo getCurrentPageInfo() {
        return getPageInfoAt(getIndex());
    }

    default PageInfo getPreviousPageInfo() {
        return getPageInfoAt(getIndex() - 1);
    }

    default PageInfo getNextPageInfo() {
        return getPageInfoAt(getIndex() + 1);
    }

    default IPage getCurrentPage() {
        return getPageAt(getIndex());
    }

    default IPage getPreviousPage() {
        return getPageAt(getIndex() - 1);
    }

    default IPage getNextPage() {
        return getPageAt(getIndex() + 1);
    }

    default void remove() {
        for (IPage page : getPageList()) {
            PageSnapshotManager.getInstance().removeSnapshot(page.getPageInfo().getId());
//            pageInfo.getPageInfo().delete();
            page.deletePageInfo();
        }
        getPageList().clear();
    }

    void destroy();

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

}
