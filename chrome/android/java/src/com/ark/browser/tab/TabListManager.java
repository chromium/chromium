package com.ark.browser.tab;

import androidx.annotation.NonNull;

import com.ark.browser.ArkWindowAndroid;
import com.ark.browser.tab.core.IPage;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.tab.core.ITabGroup;
import com.ark.browser.tab.dao.ArkTabDao;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class TabListManager {

    private static final String TAG = "TabListManager";

    private static TabListManager sInstance;

    private final ObserverList<TabManagerObserver> mObservers = new ObserverList<>();
    private final ITabGroup[] tabLists = new ITabGroup[2];
    private final TabInfoObserver tabInfoObserver;
    private int currentIndex = 0;

    public static TabListManager getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) sInstance = new TabListManager();
        return sInstance;
    }

    private TabListManager() {
        tabInfoObserver = new EmptyTabInfoObserver() {
            @Override
            public void didAddTab(IPage page, @TabLaunchType int type) {
                ArkLogger.d(TAG, "didAddTab");
                notifyChanged();
            }

            @Override
            public void didSelectTab(IPage page, @TabSelectionType int type, int lastId) {
                ArkLogger.d(TAG, "didSelectTab");
                notifyChanged();
            }

        };
    }

    public void onDestroy() {
        this.mObservers.clear();
        for (ITabGroup tabList : tabLists) {
            tabList.destroy();
        }
        Arrays.fill(tabLists, null);
        currentIndex = 0;
        sInstance = null;
    }

    private boolean mLoaded = false;

    public boolean isLoaded() {
        return mLoaded;
    }

    public void restore(ArkWindowAndroid nativeWindow, Callback<Void> callback) {

//        tabLists[0] = new TabGroupImpl(nativeWindow, false);
//        tabLists[1] = new TabGroupImpl(nativeWindow, true);
//        ThreadPool.execute(() -> {
//            tabLists[0].init(nativeWindow);
//            ThreadPool.post(() -> {
//                tabLists[0].addObserver(tabInfoObserver);
//                mLoaded = true;
//                if (callback != null) {
//                    callback.onResult(null);
//                }
//            });
//        });


        tabLists[0] = ArkTabDao.loadTabGroup(nativeWindow, false);

        tabLists[1] = ArkTabDao.loadTabGroup(nativeWindow, true);

        ThreadPool.execute(() -> {
            tabLists[0].init(nativeWindow);
            ThreadPool.post(() -> {
                tabLists[0].addObserver(tabInfoObserver);
                mLoaded = true;
                if (callback != null) {
                    callback.onResult(null);
                }

                ThreadPool.postOnUIThread(() -> {

                    int count = tabLists[0].getCount();
                    ArkLogger.e(TAG, "restore count=" + count);

                    if (count > 0) {
                        ArkLogger.e(TAG, "restore selectTabAt " + tabLists[0].getIndex());
                        tabLists[0].selectTabAt(tabLists[0].getIndex());
                    } else {
                        ArkLogger.e(TAG, "restore openNewTab");
                        LoadUrlParams params = new LoadUrlParams("www.baidu.com", PageTransition.LINK);
                        TabListManager.getInstance().openNewTab(params, TabLaunchType.FROM_CHROME_UI);
                    }
                });

            });
        });

    }

//    public void saveState() {
//        tabLists[0].saveState();
//    }

    public ITabGroup getTabList(boolean incognito) {
        return tabLists[incognito ? 1 : 0];
    }

    public List<ITabGroup> getTabLists() {
        return new ArrayList<>(Arrays.asList(tabLists));
    }

    public ITabGroup getCurrentTabList() {
        return tabLists[currentIndex];
    }

    public boolean goBack() {
        return getCurrentTabList().goBack();
    }

    public ITabGroup getTabListByPageId(Tab tab) {
        if (tab == null) {
            return null;
        }
        return getTabListByPageId(tab.getId());
    }

    public ITabGroup getTabListByPageId(int id) {
        for (ITabGroup tabList : tabLists) {
            if (tabList.getTabInfoById(id) != null) {
                return tabList;
            }
        }
        return null;
    }

    public ITab getTabInfoById(long tabInfoId) {
        for (ITabGroup tabList : tabLists) {
            ITab info = tabList.getTabInfoById(tabInfoId);
            if (info != null) {
                return info;
            }
        }
        return null;
    }


    public PageInfo getPageInfoById(int id) {
        if (id != Tab.INVALID_PAGE_ID) {
            for (ITabGroup tabList : tabLists) {
                PageInfo pageInfo = tabList.getPageInfoById(id);
                if (pageInfo != null) {
                    return pageInfo;
                }
            }
        }
        return null;
    }

    public void selectTabList(boolean incognito) {
        currentIndex = incognito ? 1 : 0;
    }

    public IPage getCurrentPage() {
        return getCurrentTabList().getCurrentPage();
    }

    public PageInfo getCurrentPageInfo() {
        return getCurrentTabList().getCurrentPageInfo();
    }

    public ITab getTabInfo(Tab tab) {
        if (tab == null) {
            return null;
        }
        return getTabList(tab.isIncognito()).getTabInfoById(tab.getId());
    }

    public ITab getTabInfo(PageInfo pageInfo) {
        if (pageInfo == null) {
            return null;
        }
        return getTabList(pageInfo.isIncognito()).getTabInfoById(pageInfo.getPageId());
    }

    public ITab getCurrentTabInfo() {
        return getCurrentTabList().getCurrentTab();
    }

    public int getCurrentPageId() {
        PageInfo pageInfo = getCurrentPageInfo();
        return pageInfo == null ? Tab.INVALID_PAGE_ID : pageInfo.getPageId();
    }

    public boolean isIncognitoSelected() {
        return currentIndex == 1;
    }

    public void selectTab(int position, boolean incognito) {
        ITabGroup tabList = getTabList(incognito);
        ITab tabInfo = tabList.getTabAt(position);
        selectPageInfo(tabInfo, tabInfo.getCurrentPage());
    }

    public void selectTab(ITab tabInfo) {
        selectPageInfo(tabInfo, tabInfo.getCurrentPage());
    }

    public void selectTab(ITab tabInfo, IPage pageInfo) {
        selectPageInfo(tabInfo, pageInfo);
    }

    public void selectPrePage(ITab tabInfo) {
        selectPageInfo(tabInfo, tabInfo.getPreviousPage());
    }

    public void selectNextPage(ITab tabInfo) {
        selectPageInfo(tabInfo, tabInfo.getNextPage());
    }

    private void selectPageInfo(ITab tabInfo, IPage pageInfo) {
        ITabGroup tabList = getTabList(tabInfo.getTabInfo().isIncognito());
        tabList.selectTabInfo(tabInfo, pageInfo);
    }

    public int getTotalTabCount() {
        int count = 0;
        for (ITabGroup tabList : getTabLists()) {
            count += tabList.getCount();
        }
        return count;
    }

    public void notifyChanged() {
        ArkLogger.d(TAG, "notifyChanged size=" + mObservers.size());
        for (TabManagerObserver listener : mObservers) {
            ArkLogger.d(TAG, "notifyChanged listener=" + listener);
            listener.onChange();
        }
    }

    public ITab cloneTab(ITab tabInfo) {
        return getTabList(tabInfo.getTabInfo().isIncognito()).cloneTab(tabInfo);
    }

    public boolean moveToNewTab(PageInfo page) {
        if (page != null) {
            ITabGroup tabList = getTabList(page.isIncognito());
            return tabList.moveToNewTab(tabList.getPageById(page.getPageId()));
        }
        return false;
    }

    public void openNewTab(LoadUrlParams loadUrlParams, @TabLaunchType int type, boolean incognito) {
        openNewTab(null, loadUrlParams, type, incognito);
    }

    public void openNewTab(LoadUrlParams loadUrlParams, @TabLaunchType int type) {
        openNewTab(null, loadUrlParams, type, isIncognitoSelected());
    }

    public void openNewTab(PageInfo pageInfo, LoadUrlParams loadUrlParams, @TabLaunchType int type,
                           boolean incognito) {
        ITabGroup tabList = getTabList(incognito);
        ITab currentTab = pageInfo == null ? null : tabList.getTabById(pageInfo.getPageId());
        tabList.openNewTab(currentTab, loadUrlParams, type);
    }

    public boolean openNewPage(@NonNull Tab parent, @TabLaunchType int type, String url) {
        ArkLogger.d("TabListManager", "openNewPage url=" + url + " type=" + type);

        ITabGroup tabList = getTabList(parent.isIncognito());
        return tabList.openNewPage(parent, type, url);
    }

    public boolean openNewPage(@NonNull Tab parent, LoadUrlParams params) {
        ArkLogger.d("TabListManager", "openNewPage params=" + params);

        ITabGroup tabList = getTabList(parent.isIncognito());
        return tabList.openNewPage(parent, params);
    }

    public void addObserver(TabManagerObserver observer) {
        ArkLogger.e(TAG, "addObserver hasObserver=" + mObservers.hasObserver(observer));
        if (!mObservers.hasObserver(observer)) {
            mObservers.addObserver(observer);
        }
    }

    public void removeObserver(TabManagerObserver observer) {
        mObservers.removeObserver(observer);
    }


}
