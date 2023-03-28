package com.ark.browser.tab;

import androidx.annotation.NonNull;

import com.ark.browser.core.ArkWebContents;
import com.ark.browser.core.ArkWebManager;
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

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CountDownLatch;

public class TabGroupManager {

    private static final String TAG = "TabGroupManager";

    private static final Map<String, ITabGroup> TAB_GROUP_MAP = new HashMap<>();

    private TabGroupManager() {

    }

    public static GlobalSelector global() {
        return GlobalSelector.INSTANCE;
    }

    public static List<ITabGroup> getTabGroups() {
        return new ArrayList<>(TAB_GROUP_MAP.values());
    }

    public static ITabGroup getTabGroup(String groupId) {
        return TAB_GROUP_MAP.get(groupId);
    }

    public static ITabGroup getTabGroup(int tabId) {
        for (ITabGroup group : TAB_GROUP_MAP.values()) {
            if (group.getTabById(tabId) != null) {
                return group;
            }
        }
        return null;
    }

    public static ITab getTabById(int tabId) {
        for (ITabGroup group : TAB_GROUP_MAP.values()) {
            ITab tab = group.getTabById(tabId);
            if (tab != null) {
                return tab;
            }
        }
        return null;
    }


    public static PageInfo findPageInfoById(int id) {
        if (id != Tab.INVALID_PAGE_ID) {
            for (ITabGroup group : TAB_GROUP_MAP.values()) {
                PageInfo pageInfo = group.getPageInfoById(id);
                if (pageInfo != null) {
                    return pageInfo;
                }
            }
        }
        return null;
    }

    public static IPage findPageById(int pageId) {
        if (pageId != Tab.INVALID_PAGE_ID) {
            for (ITabGroup group : TAB_GROUP_MAP.values()) {
                IPage page = group.getPageById(pageId);
                if (page != null) {
                    return page;
                }
            }
        }
        return null;
    }

    public static int getTotalTabCount() {
        int count = 0;
        for (ITabGroup tabList : getTabGroups()) {
            count += tabList.getCount();
        }
        return count;
    }

    public static ITab cloneTab(ITab tab) {
        return getTabGroup(tab.getGroupId())
                .cloneTab(tab);
    }

    public static boolean moveToNewTab(PageInfo page) {
        ITab tab = getTabById(page.getTabId());
        if (tab != null) {
            ITabGroup group = getTabGroup(tab.getGroupId());
            return group.moveToNewTab(group.getPageById(page.getId()));
        }
        return false;
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

        public BaseSelector(ITabGroup group, ITabGroup... groups) {
            tabGroups.add(group);
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

        private static final GlobalSelector INSTANCE = new GlobalSelector();

        public static final String GROUP_DEFAULT = "group_default";
        public static final String GROUP_INCOGNITO = "group_incognito";

        private int mCurrentIndex = 0;

        public GlobalSelector() {
            super(new TabGroupImpl(GROUP_DEFAULT, false),
                    new TabGroupImpl(GROUP_INCOGNITO, true));
        }

        public boolean isIncognitoSelected() {
            return mCurrentIndex == 1;
        }

        public ITabGroup getTabGroup(boolean incognito) {
            return TabGroupManager.getTabGroup(
                    incognito ? GROUP_INCOGNITO : GROUP_DEFAULT);
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
            mCurrentIndex = 0;
            super.destroy();
        }
    }


}
