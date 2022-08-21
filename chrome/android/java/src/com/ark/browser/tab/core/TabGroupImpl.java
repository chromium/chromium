package com.ark.browser.tab.core;

import android.text.TextUtils;

import com.ark.browser.ArkWindowAndroid;
import com.ark.browser.tab.PageCacheManager;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.PageInfoManager;
import com.ark.browser.tab.TabInfo;
import com.ark.browser.tab.TabInfoManager;
import com.ark.browser.tab.TabInfoObserver;
import com.ark.browser.utils.ArkLogger;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class TabGroupImpl implements ITabGroup {

    private static final String TAG = "TabGroupImpl";

    private final List<ITab> mTabList = new ArrayList<>();

    private final ArkWindowAndroid nativeWindow;

    private final ObserverList<TabInfoObserver> mObservers;

    private final boolean incognito;

    protected int index = ITab.INVALID_TAB_INDEX;

    public TabGroupImpl(ArkWindowAndroid nativeWindow, boolean incognito) {
        this.nativeWindow = nativeWindow;
        this.mObservers = new ObserverList<>();
        this.incognito = incognito;
    }

    @Override
    public void init(ArkWindowAndroid nativeWindow) {
        long start = System.currentTimeMillis();
        this.index = ITab.INVALID_TAB_INDEX;
        this.mTabList.clear();
//        this.mObservers.clear();


        List<PageInfo> allPages = PageInfoManager.getAllPages();

        Map<String, List<PageInfo>> pageListMap = new HashMap<>();
        for (PageInfo pageInfo : allPages) {
            String tabInfoId = pageInfo.getTabInfoId();
            List<PageInfo> pages = pageListMap.get(tabInfoId);
            if (pages == null) {
                pages = new ArrayList<>();
                pageListMap.put(tabInfoId, pages);
            }
//            Log.d(TAG, "deltaTime pageInfo " + pageInfo.getTabInfoId() + "-" + pageInfo.getOriginalIndex());
            pages.add(pageInfo);
        }


        for (TabInfo tabInfo : TabInfoManager.getAllTabs()) {
//            long startTime = System.currentTimeMillis();
            String tabId = tabInfo.getTabInfoId();
            List<PageInfo> pages = pageListMap.get(tabId);
            if (pages == null || pages.isEmpty()) {
                continue;
            }
            List<IPage> pageList = new ArrayList<>();
            for (PageInfo info : pages) {
                pageList.add(new PageImpl(info));
            }
            mTabList.add(new TabImpl(tabInfo, pageList));
//            Log.d(TAG, "restore deltaTime=" + (System.currentTimeMillis() - startTime));
        }

        ArkLogger.d(TAG, "load deltaTime=" + (System.currentTimeMillis() - start));

        allPages.clear();
        pageListMap.clear();
    }

    @Override
    public boolean isIncognito() {
        return incognito;
    }

    @Override
    public int getIndex() {
        return index;
    }

    @Override
    public int getCount() {
        return mTabList.size();
    }

    @Override
    public ArkWindowAndroid getWindowAndroid() {
        return nativeWindow;
    }

    @Override
    public List<ITab> getTabInfoList() {
        return mTabList;
    }

    @Override
    public ObserverList<TabInfoObserver> getObservers() {
        return mObservers;
    }

    @Override
    public ITab getTabAt(int i) {
        if (i < 0 || i >= mTabList.size()) {
            return null;
        }
        return mTabList.get(i);
    }

    @Override
    public ITab cloneTab(ITab tab) {
        ArkLogger.d(TAG, "cloneTab");
        if (tab != null) {
            ITab cloneTab = tab.cloneTab();
            int index = indexOf(tab) + 1;
            int position = tab.getTabInfo().getPosition() + 1;
            ArkLogger.d(TAG, "cloneTab index=" + index);
            mTabList.add(index, cloneTab);
            cloneTab.getTabInfo().setPosition(position);
            cloneTab.getTabInfo().save();

            saveTabPosition(index, position);

            selectTabInfo(tab, tab.getCurrentPage());

            return cloneTab;
        }
        return null;
    }

    public int indexOf(ITab targetTab) {
        int index = 0;
        for (ITab tab : mTabList) {
            if (TextUtils.equals(tab.getId(), targetTab.getId())) {
                return index;
            }
            index++;
        }
        return -1;
    }

    @Override
    public void openNewTab(ITab currentTab, LoadUrlParams loadUrlParams, @TabLaunchType int type) {
        ArkLogger.e(TAG, "openNewTab url=" + loadUrlParams.getUrl() + " type=" + type);


        ITab newTab = new TabImpl();

        if (currentTab != null) {
            int index = indexOf(currentTab);
            int position = currentTab.getTabInfo().getPosition();
            ArkLogger.d(TAG, "openNewTab currentPosition=" + position + " index=" + index);

            mTabList.add(++index, newTab);
            newTab.getTabInfo().setPosition(++position);
            newTab.getTabInfo().save();

            saveTabPosition(index, position);
        } else {
            newTab.getTabInfo().setPosition(getCount());
            mTabList.add(newTab);
        }

        ArkLogger.d(TAG, "openNewTab newPos=" + newTab.getTabInfo().getPosition());


        loadUrlParams.setUrl(UrlFormatter.fixupUrl(loadUrlParams.getUrl()).getValidSpecOrEmpty());
        loadUrlParams.setTransitionType(type);

        Tab page = PageCacheManager.getInstance().createLivePageByType(
                newTab.getPageSize(), getWindowAndroid(), newTab, type);

//        PageInfo pageInfo = page.getPageInfo();
        PageInfo pageInfo = new PageInfo();
        pageInfo.setPageId(page.getId());
        pageInfo.setUrl(page.getUrl().toString());
        pageInfo.setTitle(page.getTitle());
        pageInfo.setIncognito(page.isIncognito());
        IPage newPage = new PageImpl(pageInfo);
        newTab.getPageGroup().getPageInfoList().add(newPage);

        for (TabInfoObserver obs : getObservers()) {
            obs.didAddTab(newPage, type);
        }
        ArkLogger.d(TAG, "openNewTab loadUrlParams=" + loadUrlParams);
        page.loadUrl(loadUrlParams);
        selectTabInfo(newTab, newPage);
    }

    public ITab getTabInfo(PageInfo pageInfo) {
        if (pageInfo == null) {
            return null;
        }
        return getTabInfoById(pageInfo.getTabInfoId());
    }

    public boolean isClosurePending(int pageId) {
        IPage page = getCurrentPage();
        if (page == null) {
            return false;
        }
        return page.getId() != pageId;
    }

    @Override
    public boolean openNewPage(Tab parent, @TabLaunchType int type, String url) {
        ArkLogger.d(TAG, "openNewPage url=" + url + " type=" + type);

        int parentId = parent.getId();
        // The parent tab was already closed.  Do not open child tabs.
        if (isClosurePending(parentId)) return false;

        // If parent is in the same tab model, place the new tab next to it.
        ITab manager = getTabById(parentId);
        if (manager == null) {
            return false;
        }

        int index = manager.indexOfPage(parentId);
        ArkLogger.d(TAG, "openNewPage index=" + index);
        if (index == ITab.INVALID_TAB_INDEX) {
            return false;
        }

        PageInfo parentPageInfo = manager.getPageInfoAt(index);
        ArkLogger.d(TAG, "openNewPage parentPageInfo=" + parentPageInfo);
        if (parentPageInfo == null) {
            return false;
        }

        Tab tab = PageCacheManager.getInstance().createLivePageByType(
                parentPageInfo.getOriginalIndex() + 1, getWindowAndroid(), manager, type);
//        PageInfo pageInfo = tab.getPageInfo();
        PageInfo pageInfo = new PageInfo();
        pageInfo.setUrl(tab.getUrl().toString());
        pageInfo.setTitle(tab.getTitle());
        pageInfo.setIncognito(tab.isIncognito());
        IPage page = new PageImpl(pageInfo);
        IPageGroup pageInfoList = manager.getPageGroup();

        pageInfoList.getPageInfoList().add(++index, page);

//        pageInfoList.add(pageInfo.getOriginalIndex(), pageInfo);

        LoadUrlParams params = new LoadUrlParams(UrlFormatter.fixupUrl(url));
        params.setTransitionType(type);
        params.setReferrer(new Referrer(parent.getUrl().toString(), org.chromium.network.mojom.ReferrerPolicy.DEFAULT));
        ArkLogger.d(TAG, "openNewPage params=" + params);
        tab.loadUrl(params);


        for (TabInfoObserver obs : mObservers) {
            obs.didAddTab(page, type);
        }
        selectTabInfo(manager, page);


//        if (++index < pageInfoList.getCount()) {
//            List<IPage> pageRemoved = pageInfoList.getPageInfoList()
//                    .subList(index, pageInfoList.getCount());
//
//            List<IPage> tempPages = new ArrayList<>(pageRemoved);
//            ThreadPool.executeIO(() -> {
//                long start = System.currentTimeMillis();
//                Log.d(TAG, "openNewPage pageRemovedCount=" + tempPages.size());
//
//                DatabaseWrapper database = FlowManager.getDatabase(PageInfoManager.class).getWritableDatabase();
//                try {
//                    database.beginTransaction();
//                    for (IPage info : tempPages) {
////                        info.removeInfoSync();
//                        int pageId = info.getId();
//                        String sql = String.format("delete from PageInfo where pageId=%s", pageId);
//                        database.execSQL(sql);
//
//                        PageCacheManager.getInstance().removePage(info);
//                        TabSnapshotManager.getInstance().removeSnapshot(pageId);
//                    }
//                    database.setTransactionSuccessful();
//                } finally {
//                    database.endTransaction();
//                }
//
////                for (PageInfo info : tempPages) {
////                    info.removeInfoSync();
////                }
//                Log.d(TAG, "openNewPage pageRemoved deltaTime=" + (System.currentTimeMillis() - start));
//            });
//            pageRemoved.clear();
//        }

        ArkLogger.d(TAG, "openNewPage end");
        return true;
    }

    @Override
    public boolean moveToNewTab(IPage page) {
        ArkLogger.d(TAG, "moveToNewTab");
        ITab tabInfo = getTabInfoById(page.getId());
        if (tabInfo != null && tabInfo.removePage(page)) {
            TabInfo newTabInfo = TabInfo.create();
            ITab newTab = new TabImpl(newTabInfo);
            page.getPageInfo().setTabInfoId(newTabInfo.getTabInfoId());
            newTab.getPageGroup().addPage(page);

            int index = indexOf(tabInfo) + 1;
            int position = tabInfo.getTabInfo().getPosition() + 1;
            getTabInfoList().add(index, newTab);
            newTabInfo.setPosition(position);
            newTabInfo.save();

            saveTabPosition(index, position);

            selectTabInfo(newTab, page);
            return true;
        }
        return false;
    }

    /**
     * Subscribes a {@link TabInfoObserver} to be notified about changes to this model.
     *
     * @param observer The observer to be subscribed.
     */
    public void addObserver(TabInfoObserver observer) {
        this.mObservers.addObserver(observer);
    }

    /**
     * Unsubscribes a previously subscribed {@link TabInfoObserver}.
     *
     * @param observer The observer to be unsubscribed.
     */
    public void removeObserver(TabInfoObserver observer) {
        this.mObservers.removeObserver(observer);
    }

    @Override
    public void destroy() {
        this.mObservers.clear();
        for (ITab info : mTabList) {
            info.destroy();
        }
        this.mTabList.clear();
        this.index = ITab.INVALID_TAB_INDEX;
    }

    @Override
    public void onIndexChanged(int index) {
        ArkLogger.e(TAG, "onIndexChanged index=" + index);
        this.index = index;
//        PrefsHelper.with().putInt("tab_index", index);
    }

    private static final int MAX_CHANGE_COUNT = 10;

    private void saveTabPosition(int index, int position) {
        long start = System.currentTimeMillis();
        int i = index;
        int pos = position;
        TabInfo next;
        List<TabInfo> changes = new ArrayList<>();

        ArkLogger.d(TAG, "saveTabPosition count=" + getCount() + " i=" + i + " pos=" + pos);
        while ((next = getTabInfoAt(++i)) != null && pos >= next.getPosition()) {
            next.setPosition(++pos);
//            next.update();

            changes.add(next);

            if (changes.size() > MAX_CHANGE_COUNT) {
                i = index;
                pos = pos + 1;
                changes.clear();
                while ((next = getTabInfoAt(++i)) != null && pos >= next.getPosition()) {
                    changes.add(next);
                    if (changes.size() > MAX_CHANGE_COUNT) {
                        pos++;
                    } else {
                        pos += 2;
                    }
                    next.setPosition(pos);
                }
                break;
            }
        }
        ArkLogger.d(TAG, "saveTabPosition dt=" + (System.currentTimeMillis() - start) + " size=" + changes.size());

        if (changes.isEmpty()) {
            return;
        }

//        ThreadPool.executeIO(() -> {
//            long t = System.currentTimeMillis();
//            DatabaseWrapper db = FlowManager.getDatabase(TabInfoManager.class)
//                    .getWritableDatabase();
//            try {
//                db.beginTransaction();
//                Log.d(TAG, "saveTabPosition beginTransaction");
//                int j = 0;
//                for (TabInfo tabInfo : changes) {
//                    db.execSQL(String.format(
//                            "update TabInfo set position=%s where tabInfoId='%s'",
//                            tabInfo.getPosition(), tabInfo.getTabInfoId())
//                    );
//                    Log.d(TAG, "saveTabPosition j=" + (j++));
//                }
//                db.setTransactionSuccessful();
//                Log.d(TAG, "saveTabPosition setTransactionSuccessful");
//            } finally {
//                db.endTransaction();
//                Log.d(TAG, "saveTabPosition endTransaction");
//            }
//            Log.d(TAG, "saveTabPosition deltaTime=" + (System.currentTimeMillis() - t) + " size=" + changes.size());
//        });
    }


}
