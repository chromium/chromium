package com.ark.browser.tab.core;

import com.ark.browser.tab.PageCacheManager;
import com.ark.browser.utils.ArkLogger;

import org.chromium.base.Log;

import java.util.ArrayList;
import java.util.List;

public class PageGroupImpl implements IPageGroup {

    private static final String TAG = "PageGroupImpl";

    protected transient final List<IPage> mPageInfoList = new ArrayList<>();

    protected int index = 0;

//    public PageGroupImpl(List<PageInfo> pages) {
//        mPageInfoList.clear();
//
//        for (PageInfo info : pages) {
//            mPageInfoList.add(new PageImpl(info));
//        }
//
//        index = Math.min(index, mPageInfoList.size() - 1);
//        ArkLogger.d(TAG, "load pageSize=" + mPageInfoList.size() + " index=" + index);
//    }

    public PageGroupImpl(List<IPage> pages) {
        mPageInfoList.clear();
        this.mPageInfoList.addAll(pages);
        index = Math.min(index, mPageInfoList.size() - 1);
        ArkLogger.d(TAG, "load pageSize=" + mPageInfoList.size() + " index=" + index);
    }

//    @Override
//    public IPageGroup clone() {
//
//        PageGroupImpl pageGroup = new PageGroupImpl();
//
//        PageInfo parentInfo = null;
//        for (int i = 0; i < getCount(); i++) {
//            PageInfo tab = getPageInfoAt(i);
//            Log.d(getClass().getSimpleName(), "cloneTab id=" + tab.getPageId());
//            TabState state = TabState.restoreTabState(tabInfo.getTabListFolder(), tab.getPageId());
//            if (state == null) {
//                continue;
//            }
//
//            // 创建新的pageInfo
//            int newId = TabIdManager.getInstance().generateValidId(Tab.INVALID_PAGE_ID);
//            PageInfo newPageInfo = PageInfo.from(tab);
//            newPageInfo.setPageId(newId);
//            newPageInfo.setTabInfoId(pageGroup.getTabInfoId());
//            if (parentInfo == null) {
//                newPageInfo.setParentId(tab.getParentId());
//            } else {
//                newPageInfo.setParentId(pageGroup.mPageInfoList.get(i - 1).getPageInfo().getPageId());
//            }
//
//
//            // 复制TabState
//            state.parentId = newPageInfo.getParentId();
//            TabState.saveState(
//                    TabState.getTabStateFile(newTabInfo.getTabListFolder(), newId, newPageInfo.isIncognito()),
//                    state,
//                    newPageInfo.isIncognito()
//            );
//
//            parentInfo = newPageInfo;
//
//            newTab.getPageGroup().
//                    newTabInfo.mPageInfoList.add(newPageInfo);
//            if (tabInfo.getPageIndex() == i) {
//                newTabInfo.setCurrentTabId(newId);
//            }
//        }
//
//
//        return null;
//    }

    @Override
    public IPageGroup clone() {
        return null;
    }

    @Override
    public int getIndex() {
        return index;
    }

    @Override
    public int getCount() {
        return mPageInfoList.size();
    }

    @Override
    public IPage getPageAt(int pageIndex) {
        if (mPageInfoList.isEmpty() || pageIndex < 0 || pageIndex >= mPageInfoList.size()) {
            return null;
        }
        return mPageInfoList.get(pageIndex);
    }

    @Override
    public List<IPage> getPageInfoList() {
        return mPageInfoList;
    }

    @Override
    public void destroy() {
        for (IPage page : mPageInfoList) {
            PageCacheManager.getInstance().removePage(page);
        }
        mPageInfoList.clear();
    }
}
