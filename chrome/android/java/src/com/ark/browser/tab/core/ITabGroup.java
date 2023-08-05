package com.ark.browser.tab.core;

import android.graphics.Color;
import android.text.TextUtils;
import android.widget.Toast;

import com.ark.browser.settings.AppConfig;
import com.ark.browser.tab.ArkTabImpl;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.TabCacheManager;
import com.ark.browser.tab.TabGroupManager;
import com.ark.browser.tab.TabInfo;
import com.ark.browser.tab.TabInfoObserver;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.List;

public interface ITabGroup extends ITab {

    void init();

    @Override
    default String getTitle() {
        if (TextUtils.isEmpty(getTabInfo().getTitle())) {
            return getCount() + "个标签页";
        }
        return getTabInfo().getTitle();
    }

    @Override
    default int getThemeColor() {
        // TODO white or black
        return AppConfig.isNightMode() ? Color.BLACK : Color.WHITE;
    }

    boolean isIncognito();

    int getIndex();

    default int getCount() {
        return getTabList().size();
    }

    List<ITab> getTabList();

    ObserverList<TabInfoObserver> getObservers();

    default int indexOf(ITab targetTab) {
        int id = targetTab.getId();
        for (int i = 0; i < getCount(); i++) {
            ITab tab = getTabAt(i);
            if (tab.getId() == id) {
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

    @Override
    default IPage findPageById(int pageId) {
        for (int i = 0; i < getCount(); i++) {
            ITab tab = getTabAt(i);
            IPage page = tab.findPageById(pageId);
            if (page != null) {
                return page;
            }
        }
        return null;
    }

    default ITab findTabById(int tabId) {
        ITab currentTab = getCurrentTab();
        if (currentTab != null && tabId == currentTab.getId()) {
            return currentTab;
        }
        for (int i = 0; i < getCount(); i++) {
            ITab tab = getTabAt(i);
            if (tabId == tab.getId()) {
                return tab;
            } else if (tab instanceof ITabGroup) {
                tab = ((ITabGroup) tab).findTabById(tabId);
                if (tab != null) {
                    return tab;
                }
            }
        }
        return null;
    }

    @Override
    default IPage getCurrentPage() {
        ITab tab = getCurrentTab();
        ArkLogger.d(getClass().getSimpleName(), "getCurrentPage tab=" + tab);
        if (tab == null) {
            return null;
        }
        ArkLogger.d(getClass().getSimpleName(), "getCurrentPage tab.getCurrentPage=" + tab.getCurrentPage());
        return tab.getCurrentPage();
    }

    @Override
    default PageInfo getCurrentPageInfo() {
        ITab tab = getCurrentTab();
        if (tab == null) {
            return null;
        }
        return tab.getCurrentPageInfo();
    }

    default TabInfo getCurrentTabInfo() {
        return getTabInfoAt(getIndex());
    }

    default ITab getCurrentTab() {
        return getTabAt(getIndex());
    }


    default void selectTabAt(int position) {
        ITab tab = getTabAt(position);
        if (tab instanceof ChildTab) {
            selectTab(tab, tab.getCurrentPage());
        } else {
            onIndexChanged(position);
        }
    }

    default void selectTab(ITab tab) {
        selectTab(tab, tab.getCurrentPage());
    }

    default boolean selectTab(ITab iTab, IPage page) {
        ArkLogger.e(this, "selectTabInfo tabInfo=" + iTab + " page=" + page);
        if (page == null) {
            return false;
        }
        int lastId = Tab.INVALID_PAGE_ID;

        ITab currentTab = getCurrentTab();
        if (currentTab != null) {
            lastId = currentTab.getTabInfo().getCurrentPageId();
            ArkLogger.e(this, "selectTabInfo lastId=" + lastId + " currId=" + page.getId());
            if (lastId != page.getId()) {
                Tab lastTab = TabCacheManager.getInstance().findTab(lastId);
                if (lastTab != null && !lastTab.needsReload()) {
                    if (lastTab.isInitialized() && !lastTab.isDestroyed()) {
                        if (!lastTab.isClosing()) {
//                            lastTab.saveState();
                            lastTab.cacheThumbnail();
                        }
                    }
//                    lastTab.setImportance(ChildProcessImportance.NORMAL);
                }
            }
        }

//        TabState state = null;
//        if (ArkWebContents.get(page.getId()) == null) {
//            state = ArkTabDao.restorePageState(page.getId());
//        }

        ArkTabImpl tab = (ArkTabImpl) TabCacheManager.getInstance().findTab(iTab);

        if (tab == null) {
            // TODO not cast to TabImpl
            tab = ArkTabImpl.create((ChildTab) iTab, null);
        }

        iTab.getTabInfo().setAccessTime(System.currentTimeMillis());
//        iTab.selectPage(page);

        onIndexChanged(indexOf(iTab));
        tab.selectPage(page);

        // TODO optimise
        TabGroupManager.global().notifyTabSelected(iTab);
        for (TabInfoObserver obs : getObservers()) {
            ArkLogger.d(ITabGroup.this, "selectTabInfo obs=" + obs);
            obs.didSelectTab(iTab, TabSelectionType.FROM_USER, lastId);
        }
        return true;
    }

    ITab cloneTab(ITab tabInfo);

    void openInNewTab(ITab currentTab, LoadUrlParams loadUrlParams, @TabLaunchType int type);

    default void openInNewTab(PageInfo pageInfo, LoadUrlParams loadUrlParams, @TabLaunchType int type) {
        ITab currentTab = pageInfo == null ? null : findTabById(pageInfo.getTabId());
        openInNewTab(currentTab, loadUrlParams, type);
    }

    default void openInNewTab(LoadUrlParams loadUrlParams, @TabLaunchType int type) {
        openInNewTab(getCurrentTab(), loadUrlParams, type);
    }

    void openInNewGroup(ITab currentTab, LoadUrlParams loadUrlParams, @TabLaunchType int type);

    boolean moveToNewTab(ITab tab, IPage page);

    boolean moveToNewGroup(ITab tab, boolean selected);

    boolean closeTab(ITab tab);

    default void closeAllTabs() {
        Toast.makeText(ContextUtils.getApplicationContext(), "TODO 关闭所有窗口", Toast.LENGTH_SHORT).show();
    }

    default boolean removeTab(ITab tab, boolean delete) {
        if (tab == null) {
            return false;
        }
        boolean result = getTabList().remove(tab);
        if (result) {
            int index = getIndex();
            if (getTabList().isEmpty()) {
                // TODO remove empty group
                index = ITab.INVALID_TAB_INDEX;
            } else if (index > getTabList().size() - 1) {
                index = getTabList().size() - 1;
            }
            onIndexChanged(index);
            saveTabInfo();

            if (delete) {
                int id = tab.getId();
                ThreadPool.postOnUIThread(() -> {
                    tab.remove();
                    // TODO optimise
//                    TabGroupManager.global().notifyChanged();
                    for (TabInfoObserver obs : getObservers()) {
                        obs.didCloseTab(id, isIncognito());
                    }
                });
            } else {
//                TabGroupManager.global().notifyChanged();
                for (TabInfoObserver obs : getObservers()) {
                    obs.didRemoveTab(tab);
                }
            }

        }
        return result;
    }


    /**
     * Subscribes a {@link TabInfoObserver} to be notified about changes to this model.
     *
     * @param observer The observer to be subscribed.
     */
    void addObserver(TabInfoObserver observer);

    /**
     * Unsubscribes a previously subscribed {@link TabInfoObserver}.
     *
     * @param observer The observer to be unsubscribed.
     */
    void removeObserver(TabInfoObserver observer);

    default Profile getProfile() {
        ITab iTab = getCurrentTab();
        if (iTab != null) {
            Tab tab = TabCacheManager.getInstance().findTab(iTab.getId());
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
