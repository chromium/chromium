package com.ark.browser.tab;

import com.ark.browser.tab.core.IPage;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.tab.core.ITabGroup;
import com.ark.browser.tab.core.TabGroupImpl;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class TabListManager {

    private static final String TAG = "TabListManager";

    public static final String GROUP_DEFAULT = "group_default";
    public static final String GROUP_INCOGNITO = "group_incognito";

    private static volatile TabListManager sInstance;

    private final ObserverList<TabManagerObserver> mObservers = new ObserverList<>();
    private final Map<String, ITabGroup> mTabGroups = new HashMap<>();
    private final TabInfoObserver tabInfoObserver;
    private int currentIndex = 0;

    public static TabListManager getInstance() {
        if (sInstance == null) {
            synchronized (TabListManager.class) {
                if (sInstance == null) {
                    sInstance = new TabListManager();
                }
            }
        }
        return sInstance;
    }

    private TabListManager() {
        tabInfoObserver = new EmptyTabInfoObserver() {
            @Override
            public void didAddTab(ITab tab, @TabLaunchType int type) {
                ArkLogger.d(TAG, "didAddTab");
                notifyChanged();
            }

            @Override
            public void didSelectTab(ITab tab, @TabSelectionType int type, int lastId) {
                ArkLogger.d(TAG, "didSelectTab");
                notifyChanged();
            }

        };
    }

    public void onDestroy() {
        mObservers.clear();
        for (ITabGroup tabGroup : mTabGroups.values()) {
            tabGroup.destroy();
        }
        mTabGroups.clear();
        currentIndex = 0;
        sInstance = null;
    }

    private boolean mLoaded = false;

    public boolean isLoaded() {
        return mLoaded;
    }

    public void restore(Callback<Void> callback) {
        ITabGroup defaultTabGroup = new TabGroupImpl(GROUP_DEFAULT, false);
        mTabGroups.put(defaultTabGroup.getId(), defaultTabGroup);

        ITabGroup incognitoTabGroup = new TabGroupImpl(GROUP_INCOGNITO, true);
        mTabGroups.put(incognitoTabGroup.getId(), incognitoTabGroup);

        ThreadPool.execute(() -> {
            defaultTabGroup.init();
            ThreadPool.runOnUIThread(() -> {
                defaultTabGroup.addObserver(tabInfoObserver);
                mLoaded = true;
                if (callback != null) {
                    callback.onResult(null);
                }
            });
        });
    }

    public ITabGroup getTabGroup(boolean incognito) {
        return mTabGroups.get(incognito ? GROUP_INCOGNITO : GROUP_DEFAULT);
    }

    public List<ITabGroup> getTabGroups() {
        return new ArrayList<>(mTabGroups.values());
    }

    public ITabGroup getCurrentTabList() {
        return getTabGroup(isIncognitoSelected());
    }

    public boolean goBack() {
        return getCurrentTabList().goBack();
    }

    public ITabGroup getTabListByTabId(Tab tab) {
        if (tab == null) {
            return null;
        }
        return getTabListByTabId(tab.getId());
    }

    public ITabGroup getTabListByTabId(int tabId) {
        for (ITabGroup tabList : mTabGroups.values()) {
            if (tabList.getTabById(tabId) != null) {
                return tabList;
            }
        }
        return null;
    }

    public ITab getTabById(int tabId) {
        for (ITabGroup tabList : mTabGroups.values()) {
            ITab info = tabList.getTabById(tabId);
            if (info != null) {
                return info;
            }
        }
        return null;
    }


    public PageInfo findPageInfoById(int id) {
        if (id != Tab.INVALID_PAGE_ID) {
            for (ITabGroup tabList : mTabGroups.values()) {
                PageInfo pageInfo = tabList.getPageInfoById(id);
                if (pageInfo != null) {
                    return pageInfo;
                }
            }
        }
        return null;
    }

    public IPage findPageById(int pageId) {
        if (pageId != Tab.INVALID_PAGE_ID) {
            for (ITabGroup tabList : mTabGroups.values()) {
                IPage page = tabList.getPageById(pageId);
                if (page != null) {
                    return page;
                }
            }
        }
        return null;
    }

    public void selectTabGroup(boolean incognito) {
        currentIndex = incognito ? 1 : 0;
    }

//    public IPage getCurrentPage() {
//        return getCurrentTabList().getCurrentPage();
//    }

    public PageInfo getCurrentPageInfo() {
        return getCurrentTabList().getCurrentPageInfo();
    }

    public ITab getTabInfo(Tab tab) {
        if (tab == null) {
            return null;
        }
        return getTabGroup(tab.isIncognito()).getTabById(tab.getId());
    }

    public ITab getTabInfo(PageInfo pageInfo) {
        if (pageInfo == null) {
            return null;
        }
        return getTabGroup(pageInfo.isIncognito()).getTabById(pageInfo.getTabId());
    }

    public ITab getCurrentTab() {
        return getCurrentTabList().getCurrentTab();
    }

    public Tab getCurrentNativeTab() {
        ITab iTab = getCurrentTab();
        if (iTab == null) {
            return null;
        }
        return TabCacheManager.getInstance().findTab(iTab.getId());
    }

    public int getCurrentPageId() {
        PageInfo pageInfo = getCurrentPageInfo();
        return pageInfo == null ? Tab.INVALID_PAGE_ID : pageInfo.getId();
    }

    public boolean isIncognitoSelected() {
        return currentIndex == 1;
    }

    public void selectTab(int position, boolean incognito) {
        ITabGroup tabList = getTabGroup(incognito);
        ITab tabInfo = tabList.getTabAt(position);
        selectPageInfo(tabInfo, tabInfo.getCurrentPage());
    }

    public void selectTab(ITab tabInfo) {
        selectPageInfo(tabInfo, tabInfo.getCurrentPage());
    }

    public void selectTab(ITab tabInfo, IPage page) {
        selectPageInfo(tabInfo, page);
    }

    public void selectPrePage(ITab tabInfo) {
        selectPageInfo(tabInfo, tabInfo.getPreviousPage());
    }

    public void selectNextPage(ITab tabInfo) {
        selectPageInfo(tabInfo, tabInfo.getNextPage());
    }

    private void selectPageInfo(ITab tabInfo, IPage page) {
        ITabGroup tabList = getTabGroup(tabInfo.getTabInfo().isIncognito());
        tabList.selectTab(tabInfo, page);
    }

    public int getTotalTabCount() {
        int count = 0;
        for (ITabGroup tabList : getTabGroups()) {
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
        return getTabGroup(tabInfo.getTabInfo().isIncognito()).cloneTab(tabInfo);
    }

    public boolean moveToNewTab(PageInfo page) {
        if (page != null) {
            ITabGroup tabList = getTabGroup(page.isIncognito());
            return tabList.moveToNewTab(tabList.getPageById(page.getId()));
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
        ITabGroup tabList = getTabGroup(incognito);
        ITab currentTab = pageInfo == null ? null : tabList.getTabById(pageInfo.getTabId());
        tabList.openNewTab(currentTab, loadUrlParams, type);
    }

//    public boolean openNewPage(@NonNull Tab parent, @TabLaunchType int type, String url) {
//        ArkLogger.d("TabListManager", "openNewPage url=" + url + " type=" + type);
//
//        ITabGroup tabList = getTabList(parent.isIncognito());
//        return tabList.openNewPage(parent, type, url);
//    }
//
//    public boolean openNewPage(@NonNull Tab parent, LoadUrlParams params) {
//        ArkLogger.d("TabListManager", "openNewPage params=" + params);
//
//        ITabGroup tabList = getTabList(parent.isIncognito());
//        return tabList.openNewPage(parent, params);
//    }

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
