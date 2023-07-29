package com.ark.browser.tab;

import android.util.SparseArray;

import androidx.annotation.NonNull;

import com.ark.browser.core.ArkWebContents;
import com.ark.browser.core.ArkWebManager;
import com.ark.browser.tab.core.GroupTab;
import com.ark.browser.tab.core.IPage;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.tab.core.ITabGroup;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CountDownLatch;

public class TabGroupManager {

    private static final String TAG = "TabGroupManager";

    private static final SparseArray<ITabGroup> TAB_GROUP_MAP = new SparseArray<>();

    private TabGroupManager() {

    }

    public static GlobalSelector global() {
        return GlobalSelector.getInstance();
    }

    public static List<ITabGroup> getTabGroups() {
        List<ITabGroup> list = new ArrayList<>();
        for (int i = 0; i < TAB_GROUP_MAP.size(); i++) {
            list.add(TAB_GROUP_MAP.valueAt(i));
        }
        return list;
    }

    public static ITabGroup getTabGroupById(int groupId) {
        return TAB_GROUP_MAP.get(groupId);
    }

    public static ITabGroup getTabGroup(int tabId) {
        // TODO getTabGroups()
        for (ITabGroup group : getTabGroups()) {
            if (group.findTabById(tabId) != null) {
                return group;
            }
        }
        return null;
    }

    public static ITab findTabById(int tabId) {
        // TODO getTabGroups()
        for (ITabGroup group : getTabGroups()) {
            ITab tab = group.findTabById(tabId);
            if (tab != null) {
                return tab;
            }
        }
        return null;
    }

    public static IPage findPageById(int pageId) {
        if (pageId != Tab.INVALID_PAGE_ID) {
            // TODO getTabGroups()
            for (ITabGroup group : getTabGroups()) {
                IPage page = group.findPageById(pageId);
                if (page != null) {
                    return page;
                }
            }
        }
        return null;
    }

//    public static int getTotalTabCount() {
//        int count = 0;
//        for (ITabGroup tabList : getTabGroups()) {
//            count += tabList.getCount();
//        }
//        return count;
//    }

    public static ITab cloneTab(ITab tab) {
        return getTabGroupById(tab.getParentId()).cloneTab(tab);
    }

    public static boolean moveToNewTab(PageInfo page) {
        ITab tab = findTabById(page.getTabId());
        if (tab != null) {
            ITabGroup group = getTabGroupById(tab.getParentId());
            return group.moveToNewTab(group.findPageById(page.getId()));
        }
        return false;
    }

    public static boolean selectTab(ITab iTab, IPage page) {
        ITabGroup tabGroup = getTabGroupById(iTab.getParentId());
        if (tabGroup == null) {
            return false;
        }
        return tabGroup.selectTab(iTab, page);
    }


    public interface Selector {

        boolean isLoaded();

        void restore(Callback<Void> callback);

        @NonNull
        ITabGroup getCurrentTabGroup();

        default PageInfo getCurrentPageInfo() {
            return getCurrentTabGroup().getCurrentPageInfo();
        }

        default ITab getCurrentTab() {
            return getCurrentTabGroup().getCurrentTab();
        }

        default Tab getCurrentNativeTab() {
            ITab iTab = getCurrentTab();
            if (iTab == null) {
                return null;
            }
            return TabCacheManager.getInstance().findTab(iTab.getId());
        }

        default ArkWebContents getCurrentWeb() {
            PageInfo pageInfo = getCurrentPageInfo();
            if (pageInfo == null) {
                return null;
            }
            return ArkWebManager.get(pageInfo.getId());
        }

        default boolean canGoForward() {
            return getCurrentTabGroup().canGoForward();
        }

        default boolean goForward() {
            return getCurrentTabGroup().goForward();
        }

        default boolean canGoBack() {
            return getCurrentTabGroup().canGoBack();
        }

        default boolean goBack() {
            return getCurrentTabGroup().goBack();
        }

        void notifyChanged();

        void addObserver(TabManagerObserver observer);

        void removeObserver(TabManagerObserver observer);

        void destroy();

    }

    private static abstract class BaseSelector implements Selector {

        private final ObserverList<TabManagerObserver> mObservers = new ObserverList<>();

        private final TabInfoObserver tabInfoObserver = new EmptyTabInfoObserver() {
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

        protected final List<ITabGroup> tabGroups = new ArrayList<>(0);

        private boolean mLoaded = false;

        public BaseSelector(ITabGroup... groups) {
            if (groups != null) {
                tabGroups.addAll(Arrays.asList(groups));
            }
        }

        @Override
        public boolean isLoaded() {
            return mLoaded;
        }

        @Override
        public void restore(Callback<Void> callback) {
            CountDownLatch latch = new CountDownLatch(tabGroups.size());
            for (ITabGroup group : tabGroups) {
                TAB_GROUP_MAP.put(group.getId(), group);
                ThreadPool.execute(() -> {
                    group.init();
                    latch.countDown();
                    ThreadPool.runOnUIThread(() -> {
                        group.addObserver(tabInfoObserver);
                        if (!mLoaded && latch.getCount() == 0) {
                            mLoaded = true;
                            if (callback != null) {
                                callback.onResult(null);
                            }
                        }
                    });
                });
            }
        }

        @Override
        public void notifyChanged() {
            ArkLogger.d(TAG, "notifyChanged size=" + mObservers.size());
            for (TabManagerObserver listener : mObservers) {
                ArkLogger.d(TAG, "notifyChanged listener=" + listener);
                listener.onChange();
            }
        }

        @Override
        public void addObserver(TabManagerObserver observer) {
            ArkLogger.e(TAG, "addObserver hasObserver=" + mObservers.hasObserver(observer));
            if (!mObservers.hasObserver(observer)) {
                mObservers.addObserver(observer);
            }
        }

        @Override
        public void removeObserver(TabManagerObserver observer) {
            mObservers.removeObserver(observer);
        }

        @Override
        public void destroy() {
            for (ITabGroup group : tabGroups) {
                TAB_GROUP_MAP.remove(group.getId());
                group.destroy();
            }
        }
    }


    public static class GlobalSelector extends BaseSelector {

        private static volatile GlobalSelector sInstance;

        public static final String GROUP_DEFAULT = "group_default";
        public static final String GROUP_INCOGNITO = "group_incognito";

        public static final int ID_DEFAULT = -100;
        public static final int ID_INCOGNITO = -101;

        private int mCurrentIndex = 0;

        private GlobalSelector() {
            TabInfo info = TabInfo.create(ID_DEFAULT, -1, true);
            info.setLocked(true);
            info.setIncognito(false);
            tabGroups.add(new GroupTab(GROUP_DEFAULT, info));

            info = TabInfo.create(ID_INCOGNITO, -1, true);
            info.setLocked(true);
            info.setIncognito(true);
            tabGroups.add(new GroupTab(GROUP_INCOGNITO, info));
        }

        public static GlobalSelector getInstance() {
            if (sInstance == null) {
                synchronized (GlobalSelector.class) {
                    if (sInstance == null) {
                        sInstance = new GlobalSelector();
                    }
                }
            }
            return sInstance;
        }

        public boolean isIncognitoSelected() {
            return mCurrentIndex == 1;
        }

        public ITabGroup getTabGroup(boolean incognito) {
            return TabGroupManager.getTabGroupById(incognito ? ID_INCOGNITO : ID_DEFAULT);
        }

        public void selectTabGroup(boolean incognito) {
            mCurrentIndex = incognito ? 1 : 0;
        }

        @NonNull
        @Override
        public ITabGroup getCurrentTabGroup() {
            return getTabGroup(isIncognitoSelected());
        }

        @Override
        public void destroy() {
            if (sInstance != null) {
                synchronized (GlobalSelector.class) {
                    if (sInstance != null) {
                        sInstance = null;
                    }
                }
            }
            mCurrentIndex = 0;
            super.destroy();
        }
    }


}
