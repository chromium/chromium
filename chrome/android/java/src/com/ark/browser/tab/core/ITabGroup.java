package com.ark.browser.tab.core;

import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.core.util.AtomicFile;

import com.ark.browser.tab.ArkTabImpl;
import com.ark.browser.tab.TabCacheManager;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.TabInfo;
import com.ark.browser.tab.TabInfoObserver;
import com.ark.browser.tab.dao.ArkTabDao;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.content_public.browser.LoadUrlParams;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.List;

public interface ITabGroup {


    void init();

    @NonNull
    String getId();

    public boolean isIncognito();

    public int getIndex();

    default int getCount() {
        return getTabList().size();
    }

    List<ITab> getTabList();

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
            if (tab.getId() == targetTab.getId()) {
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

    default ITab getTabById(int tabId) {
        ITab tabInfo = getCurrentTab();
        if (tabInfo != null && tabId == tabInfo.getId()) {
            return tabInfo;
        }
        for (int i = 0; i < getCount(); i++) {
            ITab tab = getTabAt(i);
            if (tabId == tab.getId()) {
                return tab;
            }
        }
        return null;
    }

//    default int getTabIndexById(int pageId) {
//        IPage page = getCurrentPage();
//        if (page != null && page.getId() == pageId) {
//            return getIndex();
//        }
//
//        for (int i = 0; i < getCount(); i++) {
//            ITab tab = getTabAt(i);
//            if (tab.hasPage(pageId)) {
//                return i;
//            }
//        }
//
//        return ITab.INVALID_TAB_INDEX;
//    }

    default IPage getCurrentPage() {
        ITab tab = getCurrentTab();
        ArkLogger.d(getClass().getSimpleName(), "getCurrentPage tab=" + tab);
        if (tab == null) {
            return null;
        }
        ArkLogger.d(getClass().getSimpleName(), "getCurrentPage tab.getCurrentPage=" + tab.getCurrentPage());
        return tab.getCurrentPage();
    }

//    default IPage getPreviousPage() {
//        ITab tab = getCurrentTab();
//        if (tab == null) {
//            return null;
//        }
//        return tab.getPreviousPage();
//    }

//    default IPage getForwardPage() {
//        ITab tab = getCurrentTab();
//        if (tab == null) {
//            return null;
//        }
//        return tab.getNextPage();
//    }

    default PageInfo getCurrentPageInfo() {
        ITab tab = getCurrentTab();
        if (tab == null) {
            return null;
        }
        return tab.getCurrentPageInfo();
    }

//    default PageInfo getPreviousPageInfo() {
//        ITab tab = getCurrentTab();
//        if (tab == null) {
//            return null;
//        }
//        return tab.getPreviousPageInfo();
//    }

//    default PageInfo getForwardPageInfo() {
//        ITab tab = getCurrentTab();
//        if (tab == null) {
//            return null;
//        }
//        return tab.getNextPageInfo();
//    }

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
        selectTab(tab, tab.getCurrentPage());
    }

    default boolean selectPrePage(ITab tab) {
        return selectTab(tab, tab.getPreviousPage());
    }

    default boolean selectNextPage(ITab tab) {
        return selectTab(tab, tab.getNextPage());
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
            lastId = currentTab.getCurrentPageId();
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

        ArkTabImpl tab = (ArkTabImpl) TabCacheManager.getInstance().findTab(iTab.getId());

        if (tab == null) {
            tab = ArkTabImpl.create(iTab, null);
        }

        iTab.getTabInfo().setAccessTime(System.currentTimeMillis());
//        iTab.selectPage(page);

        onIndexChanged(indexOf(iTab));
        tab.selectPage(page);
//        if (state == null) {
//            tab.selectPage(page);
//        } else {
//            iTab.selectPage(page);
//        }

        for (TabInfoObserver obs : getObservers()) {
            ArkLogger.d(ITabGroup.this, "selectTabInfo obs=" + obs);
            obs.didSelectTab(iTab, TabSelectionType.FROM_USER, lastId);
        }




//        int finalLastId = lastId;
//        ThreadPool.executeIO(() -> {
//
//
//            TabState state = ArkTabDao.restorePageState(page.getId());
//
//            ArkLogger.e(ITabGroup.this, "selectTabInfo state=" + state);
//
//            TabState finalState = state;
//            ThreadPool.runOnUIThread(() -> {
//                long start = System.currentTimeMillis();
//
//
//                ArkTabImpl tab = (ArkTabImpl) PageCacheManager.getInstance().findTab(iTab.getId());
//
//                if (tab == null) {
//                    if (finalState == null) {
//                        tab = (ArkTabImpl) PageCacheManager.getInstance().createLivePage(iTab, page);
//                    } else {
//                        tab = (ArkTabImpl) PageCacheManager.getInstance().createFrozenPageFromState(
//                                iTab, finalState);
//                    }
//                } else {
//                    finalState = null;
//                }
////                innerTab.setImportance(ChildProcessImportance.IMPORTANT);
////                innerTab.show(TabSelectionType.FROM_USER);
//
//                onIndexChanged(indexOf(iTab));
//                iTab.getTabInfo().setAccessTime(System.currentTimeMillis());
//                iTab.selectPage(page);
//
//                tab.selectPage(page);
//
//
//                for (TabInfoObserver obs : getObservers()) {
//                    ArkLogger.d(ITabGroup.this, "selectTabInfo obs=" + obs);
//                    obs.didSelectTab(iTab, TabSelectionType.FROM_USER, finalLastId);
//                }
//                ArkLogger.e(ITabGroup.this, "selectTabInfo end create tab deltaTime=" + (System.currentTimeMillis() - start));
//            });
//        });


        return true;
    }



    default boolean canGoBack() {
        final TabInfo tabInfo = getCurrentTabInfo();
        if (tabInfo != null) {
            Tab currentTab = TabCacheManager.getInstance().findTab(tabInfo.getId());
            return currentTab != null && currentTab.canGoBack();
        }
        return false;
    }

    default boolean goBack() {
        ITab tab = getCurrentTab();
        if (tab != null) {
            Tab currentTab = TabCacheManager.getInstance().findTab(tab.getId());
            if (currentTab.canGoBack()) {
                currentTab.goBack();
                return true;
            }
        }
        return false;
    }

    default boolean canGoForward() {
        final TabInfo tabInfo = getCurrentTabInfo();
        if (tabInfo != null) {
            Tab currentTab = TabCacheManager.getInstance().findTab(tabInfo.getId());
            return currentTab != null && currentTab.canGoForward();
        }
        return false;
    }

    default boolean goForward() {
        ITab tabInfo = getCurrentTab();
        if (tabInfo != null) {
            Tab currentTab = TabCacheManager.getInstance().findTab(tabInfo.getId());
            if (currentTab.canGoForward()) {
                currentTab.goForward();
                return true;
            }
        }
        return false;
    }




    public ITab cloneTab(ITab tabInfo);

    public void openNewTab(ITab currentTab, LoadUrlParams loadUrlParams, @TabLaunchType int type);

//    default boolean openNewPage(Tab parent, @TabLaunchType int type, String url) {
//        LoadUrlParams params = new LoadUrlParams(UrlFormatter.fixupUrl(url));
//        params.setReferrer(new Referrer(parent.getUrl().toString(), org.chromium.network.mojom.ReferrerPolicy.DEFAULT));
//        return openNewPage(parent, params, type);
//    }
//
//    default boolean openNewPage(Tab parent, LoadUrlParams params) {
//        return openNewPage(parent, params, TabLaunchType.FROM_CHROME_UI);
//    }
//
//    public boolean openNewPage(Tab parent, LoadUrlParams loadUrlParams, @TabLaunchType int type);

    public boolean moveToNewTab(IPage page);

    default boolean removeTab(Tab tab) {
        ArkLogger.d(getClass().getSimpleName(), "closeTab tab=" + tab);
        if (tab == null) {
            return false;
        }
        ITab manager = getTabById(tab.getId());
        if (manager == null) {
            return false;
        }

        IPage nextPage;
        nextPage = manager.getPreviousPage();
        if (nextPage == null) {
            nextPage = manager.getNextPage();
        }

        boolean result;

        ArkLogger.d(getClass().getSimpleName(), "closeTab manager.getPageSize()=" + manager.getPageSize());
        if (nextPage == null) {
            removeTab(manager);
            result = getTabList().remove(manager);
            int index = getIndex();
            if (getTabList().isEmpty()) {
                index = ITab.INVALID_TAB_INDEX;
            } else if (index > getTabList().size() - 1) {
                index = getTabList().size() - 1;
            }
            onIndexChanged(index);
        } else {
            selectTab(manager, nextPage);

            int i = manager.indexOfPage(tab.getId());

            ArkLogger.d(getClass().getSimpleName(), "closeTabInStack i=" + i);
            if (i >= 0) {
                IPage page = manager.getPages().remove(i);
                page.remove();
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
        boolean result = getTabList().remove(manager);
        int index = getIndex();
        if (getTabList().isEmpty()) {
            index = ITab.INVALID_TAB_INDEX;
        } else if (getIndex() > getTabList().size() - 1) {
            index = getTabList().size() - 1;
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

    default void saveGroupFile() {
        ArkLogger.e(this, "saveGroupFile");
        try {
            ByteArrayOutputStream stream = new ByteArrayOutputStream();
            DataOutputStream os = new DataOutputStream(stream);
            int version = 1;
            os.writeInt(version);
            os.writeInt(getIndex());
            os.writeBoolean(isIncognito());
            os.writeInt(getCount());
            for (ITab tab : getTabList()) {
                os.writeInt(tab.getId());
            }
            os.close();

            ArkLogger.e(this, "saveGroupFile index=" + getIndex()
                    + " count=" + getCount() + " tabList=" + getTabList());

            byte[] bytes = stream.toByteArray();

            ThreadPool.executeIO(new Runnable() {
                @Override
                public void run() {
                    File groupFile = ArkTabDao.getGroupFile(getId());
                    AtomicFile file = new AtomicFile(groupFile);
                    FileOutputStream fos = null;
                    try {
                        fos = file.startWrite();
                        fos.write(bytes, 0, bytes.length);
                        file.finishWrite(fos);
                        ArkLogger.e(this, "saveGroupFile success!");
                    } catch (IOException e) {
                        if (fos != null) file.failWrite(fos);
                        ArkLogger.e(this, "saveGroupFile Failed to write file: " + file.getBaseFile().getAbsolutePath());
                    }
                }
            });

        } catch (IOException e) {
            e.printStackTrace();
        }
    }

}
