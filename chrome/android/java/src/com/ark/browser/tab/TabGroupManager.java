package com.ark.browser.tab;

import android.text.TextUtils;
import android.util.SparseArray;
import android.widget.Toast;

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
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;

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

    public static ITab cloneTab(ITab tab) {
        return tab.getParentGroup().cloneTab(tab);
    }

    public static boolean moveToNewTab(PageInfo page) {
        ITab tab = findTabById(page.getTabId());
        if (tab != null) {
            ITabGroup group = tab.getParentGroup();
            return group.moveToNewTab(tab, group.findPageById(page.getId()));
        }
        return false;
    }

    public static boolean moveToNewTab(ITab tab, IPage page) {
        ITabGroup group = tab.getParentGroup();
        return group.moveToNewTab(tab, page);
    }

    public static boolean selectTab(ITab iTab, IPage page) {
        ITabGroup tabGroup = iTab.getParentGroup();
        if (tabGroup == null) {
            return false;
        }
        return tabGroup.selectTab(iTab, page);
    }

    public static List<ITab> searchTabs(String keyword) {
        List<ITab> tabList = new ArrayList<>();
        if (TextUtils.isEmpty(keyword)) {
            tabList.addAll(GlobalSelector.getInstance().getTabGroup(false).getTabList());
            return tabList;
        }
        searchTabs(tabList, GlobalSelector.getInstance().getTabGroup(false), keyword.toLowerCase());
        return tabList;
    }

    private static void searchTabs(List<ITab> results, ITab tab, String keyword) {
        if (tab instanceof ITabGroup) {
            for (ITab child : ((ITabGroup) tab).getTabList()) {
                searchTabs(results, child, keyword);
            }
        } else {
            PageInfo info = tab.getCurrentPageInfo();
            if (info != null) {
                if (info.getTitle().toLowerCase().contains(keyword)
                        || info.getUrl().toLowerCase().contains(keyword)) {
                    results.add(tab);
                }
            }
        }
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

        void notifyChanged();

        void addObserver(TabManagerObserver observer);

        void removeObserver(TabManagerObserver observer);

        void destroy();

    }

    private static abstract class BaseSelector implements Selector {

        private final ObserverList<TabManagerObserver> mObservers = new ObserverList<>();

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

        public void notifyTabSelected(ITab tab) {
            ArkLogger.d(TAG, "notifyTabSelected size=" + mObservers.size());
            for (TabManagerObserver listener : mObservers) {
                ArkLogger.d(TAG, "notifyTabSelected listener=" + listener);
                listener.onTabSelected(tab);
            }
        }

        public void notifyGroupChanged(ITabGroup newGroup, ITabGroup oldGroup) {
            ArkLogger.d(TAG, "notifyGroupChanged size=" + mObservers.size());
            for (TabManagerObserver listener : mObservers) {
                ArkLogger.d(TAG, "notifyGroupChanged listener=" + listener);
                listener.onGroupChanged(newGroup, oldGroup);
            }
        }

        public void notifyTabMoved(ITab tab, ITabGroup oldGroup) {
            ArkLogger.d(TAG, "notifyTabMoved size=" + mObservers.size());
            for (TabManagerObserver listener : mObservers) {
                ArkLogger.d(TAG, "notifyTabMoved listener=" + listener);
                listener.onTabMoved(tab, oldGroup);
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

        public static final int ID_DEFAULT = -100;
        public static final int ID_INCOGNITO = -101;

        private int mCurrentIndex = 0;

        private ITabGroup mCurrentGroup;

        // TODO optimise
        protected final TabContentManager mTabContentManager = new TabContentManager();

        public TabContentManager getTabContentManager() {
            return mTabContentManager;
        }

        private GlobalSelector() {
            TabInfo info = TabInfo.create(ID_DEFAULT, -1, true);
            info.setLocked(true);
            info.setIncognito(false);
            info.setTitle("Root Default");
            tabGroups.add(new GroupTab(null, info));

            info = TabInfo.create(ID_INCOGNITO, -1, true);
            info.setLocked(true);
            info.setIncognito(true);
            info.setTitle("Root Incognito");
            tabGroups.add(new GroupTab(null, info));
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

        public void closeAllTabs() {
            Toast.makeText(ContextUtils.getApplicationContext(), "TODO 关闭所有窗口", Toast.LENGTH_SHORT).show();
        }

        public void selectGroup(ITabGroup group) {
            ArkLogger.e(TAG, "selectGroup group=" + group + " currentGroup=" + mCurrentGroup);
            if (mCurrentGroup == group) {
                return;
            }
            ITabGroup old = mCurrentGroup;
            mCurrentGroup = group;

            group.getTabInfo().setAccessTime(System.currentTimeMillis());
            group.saveTabInfo();

            ITab current = group;
            ITabGroup parent = group.getParentGroup();
            while (parent != null) {
                parent.onIndexChanged(parent.indexOf(current));
                current = parent;
                parent = parent.getParentGroup();
            }

            notifyGroupChanged(group, old);
        }

        @NonNull
        @Override
        public ITabGroup getCurrentTabGroup() {
            if (mCurrentGroup == null) {
                mCurrentGroup = getCurrentTabGroup(isIncognitoSelected());
            }
            return mCurrentGroup;
        }

        public ITabGroup getCurrentTabGroup(boolean incognito) {
            ITabGroup tabGroup = getTabGroup(incognito);
            return getTabGroup(tabGroup);
        }

        private ITabGroup getTabGroup(ITabGroup tabGroup) {
            if (tabGroup.getCurrentTab() instanceof ITabGroup) {
                return getTabGroup((ITabGroup) tabGroup.getCurrentTab());
            }
            return tabGroup;
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
