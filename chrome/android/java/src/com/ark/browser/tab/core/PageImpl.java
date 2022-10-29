package com.ark.browser.tab.core;

import com.ark.browser.tab.PageCacheManager;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.TabSnapshotManager;

import org.chromium.chrome.browser.tab.Tab;

public class PageImpl implements IPage {

    private final PageInfo pageInfo;

    public PageImpl(PageInfo pageInfo) {
        this.pageInfo = pageInfo;
    }

    @Override
    public int getId() {
        return pageInfo.getPageId();
    }

    @Override
    public PageInfo getPageInfo() {
        return pageInfo;
    }

//    @Override
//    public void remove() {
//        PageCacheManager.getInstance().removePage(pageInfo);
//        TabSnapshotManager.getInstance().removeSnapshot(pageInfo.getPageId());
//        pageInfo.deleteSync();
//    }

    @Override
    public Tab getNativePage() {
        return PageCacheManager.getInstance().findPage(getId());
    }

}
