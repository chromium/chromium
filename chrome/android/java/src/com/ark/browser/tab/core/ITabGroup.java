package com.ark.browser.tab.core;

import android.text.TextUtils;
import android.widget.Toast;

import com.ark.browser.tab.PageCacheManager;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.TabInfo;
import com.ark.browser.tab.TabInfoObserver;
import com.ark.browser.tab.TabSnapshotManager;
import com.ark.browser.utils.ThreadPool;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

public interface ITabGroup {


    void init(WindowAndroid nativeWindow);

    default String getTag() {
        return getClass().getSimpleName();
    }

    public boolean isIncognito();

    public int getIndex();

    public int getCount();

    public WindowAndroid getWindowAndroid();

    List<ITab> getTabInfoList();

    ObserverList<TabInfoObserver> getObservers();

    default int indexOf(Tab page) {
        for (int i = 0; i < getCount(); i++) {
            ITab tab = getTabAt(i);
            if (tab.hasPage(page)) {
                return i;
            }
        }
        return -1;
    }

    default int indexOf(ITab targetTab) {
        for (int i = 0; i < getCount(); i++) {
            ITab tab = getTabAt(i);
            if (TextUtils.equals(tab.getId(), targetTab.getId())) {
                return i;
            }
        }
        return -1;
    }

    ITab getTabAt(int i);

    default IPage getPageAt(int index) {
        ITab tab = getTabAt(index);
        if (tab == null) {
            return null;
        }
        return tab.getCurrentPage();
    }

    default TabInfo getTabInfoAt(int i) {
        ITab tab = getTabAt(i);
        if (tab == null) {
            return null;
        }
        return tab.getTabInfo();
    }

    default PageInfo getPageInfoById(int pageId) {
        for (int i = 0; i < getCount(); i++) {
            ITab tab = getTabAt(i);
            PageInfo pageInfo = tab.getPageInfoById(pageId);
            if (pageInfo != null) {
                return pageInfo;
            }
        }
        return null;
    }

    default IPage getPageById(int pageId) {
        for (int i = 0; i < getCount(); i++) {
            ITab tab = getTabAt(i);
            IPage page = tab.getPageById(pageId);
            if (page != null) {
                return page;
            }
        }
        return null;
    }

    default ITab getTabInfoById(String tabId) {
        ITab tabInfo = getCurrentTab();
        if (tabInfo != null && TextUtils.equals(tabId, tabInfo.getId())) {
            return tabInfo;
        }
        for (int i = 0; i < getCount(); i++) {
            ITab manager = getTabAt(i);
            if (TextUtils.equals(tabId, manager.getId())) {
                return manager;
            }
        }
        return null;
    }

    default ITab getTabInfoById(int pageId) {
        if (pageId == Tab.INVALID_PAGE_ID) {
            return null;
        }
        IPage page = getCurrentPage();
        if (page != null && page.getId() == pageId) {
            return getTabAt(getIndex());
        }

        for (int i = 0; i < getCount(); i++) {
            ITab tab = getTabAt(i);
            if (tab.hasPage(pageId)) {
                return tab;
            }
        }
        return null;
    }

    default ITab getTabById(int pageId) {
        if (pageId == Tab.INVALID_PAGE_ID) {
            return null;
        }
        IPage page = getCurrentPage();

        if (page != null && page.getId() == pageId) {
            return getTabAt(getIndex());
        }

        for (int i = 0; i < getCount(); i++) {
            ITab tab = getTabAt(i);
            if (tab.hasPage(pageId)) {
                return tab;
            }
        }
        return null;
    }

    default int getTabIndexById(int pageId) {
        IPage page = getCurrentPage();
        if (page != null && page.getId() == pageId) {
            return getIndex();
        }

        for (int i = 0; i < getCount(); i++) {
            ITab tab = getTabAt(i);
            if (tab.hasPage(pageId)) {
                return i;
            }
        }

        return ITab.INVALID_TAB_INDEX;
    }

    default IPage getCurrentPage() {
        ITab tab = getCurrentTab();
        Log.d(getClass().getSimpleName(), "getCurrentPage tab=" + tab);
        if (tab == null) {
            return null;
        }
        Log.d(getClass().getSimpleName(), "getCurrentPage tab.getCurrentPage=" + tab.getCurrentPage());
        return tab.getCurrentPage();
    }

    default IPage getPreviousPage() {
        ITab tab = getCurrentTab();
        if (tab == null) {
            return null;
        }
        return tab.getPreviousPage();
    }

    default IPage getForwardPage() {
        ITab tab = getCurrentTab();
        if (tab == null) {
            return null;
        }
        return tab.getNextPage();
    }

    default PageInfo getCurrentPageInfo() {
        ITab tab = getCurrentTab();
        if (tab == null) {
            return null;
        }
        return tab.getCurrentPageInfo();
    }

    default PageInfo getPreviousPageInfo() {
        ITab tab = getCurrentTab();
        if (tab == null) {
            return null;
        }
        return tab.getPreviousPageInfo();
    }

    default PageInfo getForwardPageInfo() {
        ITab tab = getCurrentTab();
        if (tab == null) {
            return null;
        }
        return tab.getNextPageInfo();
    }

    default TabInfo getCurrentTabInfo() {
        return getTabInfoAt(getIndex());
    }

    default ITab getCurrentTab() {
        return getTabAt(getIndex());
    }



    default void selectTabAt(int position) {
        ITab tab = getTabAt(position);
        if (tab == null) {
            return;
        }
        selectTabInfo(tab, tab.getCurrentPage());
    }

    default boolean selectPrePage(ITab tab) {
        return selectTabInfo(tab, tab.getPreviousPage());
    }

    default boolean selectNextPage(ITab tab) {
        return selectTabInfo(tab, tab.getNextPage());
    }

    default void selectTab(ITab tab) {
        selectTabInfo(tab, tab.getCurrentPage());
    }

    default boolean selectTabInfo(ITab iTab, IPage page) {
        Log.d(getTag(), "selectTabInfo tabInfo=" + iTab + " pageInfo=" + page);
        if (page == null) {
            return false;
        }
        int lastId = Tab.INVALID_PAGE_ID;

        ITab currentTab = getCurrentTab();
        if (currentTab != null) {
            lastId = currentTab.getCurrentPageId();
            Log.d(getTag(), "selectTabInfo lastId=" + lastId + " currId=" + page.getId());
            if (lastId != page.getId()) {
                Tab lastTab = PageCacheManager.getInstance().findPage(lastId);
                if (lastTab != null && !lastTab.needsReload()) {
                    if (lastTab.isInitialized() && !lastTab.isDestroyed()) {
                        if (!lastTab.isClosing()) {
//                            lastTab.saveState();
                            TabSnapshotManager.getInstance().cacheTab(lastTab);
                        }
                    }
//                    lastTab.setImportance(ChildProcessImportance.NORMAL);
                }
            }
        }


        int finalLastId = lastId;
        ThreadPool.executeIO(() -> {
            Tab tab = PageCacheManager.getInstance().findPage(page.getId());
            Log.d(getTag(), "selectTabInfo tab=" + tab);
            TabState state = null;


            TabState finalState = state;
            ThreadPool.post(() -> {
                long start = System.currentTimeMillis();
                Tab innerTab = tab;
                if (innerTab == null) {
                    if (finalState == null) {
                        innerTab = PageCacheManager.getInstance().createLivePage(
                                getWindowAndroid(), page.getPageInfo());
                    } else {
                        innerTab = PageCacheManager.getInstance().createFrozenPageFromState(
                                getWindowAndroid(), page.getPageInfo(), finalState);
                    }
                }
//                innerTab.setImportance(ChildProcessImportance.IMPORTANT);
                innerTab.show(TabSelectionType.FROM_USER);

                onIndexChanged(indexOf(iTab));
                iTab.getTabInfo().setAccessTime(System.currentTimeMillis());
                iTab.selectPage(page);
                iTab.getTabInfo().save();


                for (TabInfoObserver obs : getObservers()) {
                    Log.d(getTag(), "selectTabInfo obs=" + obs);
                    obs.didSelectTab(page, TabSelectionType.FROM_USER, finalLastId);
                }
                Log.d(getTag(), "selectTabInfo end create tab deltaTime=" + (System.currentTimeMillis() - start));
            });
        });


        return true;
    }



    default boolean canGoBack() {
        final PageInfo pageInfo = getCurrentPageInfo();
        if (pageInfo != null) {
            Tab currentTab = PageCacheManager.getInstance().findPage(pageInfo);
            if (currentTab != null && currentTab.canGoBack()) {
                return true;
            } else {
                return getPreviousPageInfo() != null;
            }

        }
        return false;
    }

    default boolean goBack() {
        ITab tabInfo = getCurrentTab();
        if (tabInfo != null) {
            IPage page = tabInfo.getCurrentPage();
            Tab currentTab;
            if (page != null && (currentTab = page.getNativePage()) != null && currentTab.canGoBack()) {
                currentTab.goBack();
                return true;
            } else {
                return selectPrePage(tabInfo);
            }
        }
        return false;
    }

    default boolean canGoForward() {
        final PageInfo pageInfo = getCurrentPageInfo();
        if (pageInfo != null) {
            Tab currentTab = PageCacheManager.getInstance().findPage(pageInfo);
            if (currentTab != null && currentTab.canGoForward()) {
                return true;
            } else {
                return getForwardPageInfo() != null;
            }

        }
        return false;
    }

    default boolean goForward() {
        ITab tabInfo = getCurrentTab();
        if (tabInfo != null) {
            IPage page = tabInfo.getCurrentPage();
            Tab currentTab;
            if (page != null && (currentTab = page.getNativePage()) != null && currentTab.canGoForward()) {
                currentTab.goForward();
                return true;
            } else {
                return selectNextPage(tabInfo);
            }
        }
        return false;
    }




    public ITab cloneTab(ITab tabInfo);

    public void openNewTab(ITab currentTab, LoadUrlParams loadUrlParams, @TabLaunchType int type);

    public boolean openNewPage(Tab parent, @TabLaunchType int type, String url);

    public boolean moveToNewTab(IPage page);

    default boolean removePage(Tab tab) {
        Log.d(getClass().getSimpleName(), "closeTab tab=" + tab);
        if (tab == null) {
            return false;
        }
        ITab manager = getTabInfoById(tab.getId());
        if (manager == null) {
            return false;
        }

        IPage nextPage;
        nextPage = manager.getPreviousPage();
        if (nextPage == null) {
            nextPage = manager.getNextPage();
        }

        boolean result;

        Log.d(getClass().getSimpleName(), "closeTab manager.getPageSize()=" + manager.getPageSize());
        if (nextPage == null) {
            removeTab(manager);
            result = getTabInfoList().remove(manager);
            int index = getIndex();
            if (getTabInfoList().isEmpty()) {
                index = ITab.INVALID_TAB_INDEX;
            } else if (index > getTabInfoList().size() - 1) {
                index = getTabInfoList().size() - 1;
            }
            onIndexChanged(index);
        } else {
            selectTabInfo(manager, nextPage);

            int i = manager.indexOfPage(tab.getId());

            Log.d(getClass().getSimpleName(), "closeTabInStack i=" + i);
            if (i >= 0) {
                IPage pageInfo = manager.getPageGroup().getPageInfoList().remove(i);
                pageInfo.remove();
                result = true;
            } else {
                result = false;
            }

        }
        return result;
    }

    default boolean closeTab(ITab manager) {
        if (manager == null) {
            return false;
        }
        removeTab(manager);
        boolean result = getTabInfoList().remove(manager);
        int index = getIndex();
        if (getTabInfoList().isEmpty()) {
            index = ITab.INVALID_TAB_INDEX;
        } else if (getIndex() > getTabInfoList().size() - 1) {
            index = getTabInfoList().size() - 1;
        }
        onIndexChanged(index);
        return result;
    }

    default void closeAllTabs() {
        Toast.makeText(ContextUtils.getApplicationContext(), "TODO 关闭所有窗口", Toast.LENGTH_SHORT).show();
    }

    default void removeTab(ITab tab) {
        tab.remove();
        for (TabInfoObserver obs : getObservers()) {
            obs.didCloseTab(tab.getCurrentPageId(), isIncognito());
        }
    }


    /**
     * Subscribes a {@link TabInfoObserver} to be notified about changes to this model.
     *
     * @param observer The observer to be subscribed.
     */
    public void addObserver(TabInfoObserver observer);

    /**
     * Unsubscribes a previously subscribed {@link TabInfoObserver}.
     *
     * @param observer The observer to be unsubscribed.
     */
    public void removeObserver(TabInfoObserver observer);

    default Profile getProfile() {
        IPage page = getCurrentPage();
        if (page != null) {
            Tab tab = page.getNativePage();
            if (tab != null) {
                return Profile.fromWebContents(tab.getWebContents());
            }
        }
        Profile lastUsedProfile = Profile.getLastUsedRegularProfile();
        if (isIncognito()) {
//            assert lastUsedProfile.hasOffTheRecordProfile();
            return lastUsedProfile.getOffTheRecordProfile(lastUsedProfile.getOTRProfileID(), true);
        }
        return lastUsedProfile.getOriginalProfile();
//        return null;
    }

    void destroy();

    void onIndexChanged(int index);

}
