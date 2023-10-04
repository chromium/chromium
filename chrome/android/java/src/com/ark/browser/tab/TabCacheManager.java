package com.ark.browser.tab;

import android.util.SparseArray;

import androidx.annotation.NonNull;
import androidx.annotation.UiThread;

import com.ark.browser.tab.core.IPage;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.utils.ArkLogger;

import org.chromium.chrome.browser.tab.Tab;

public class TabCacheManager {

    private static final String TAG = "TabCacheManager";

    private final SparseArray<Tab> tabCache = new SparseArray<>();

    private static final class Holder {
        private static final TabCacheManager MANAGER = new TabCacheManager();
    }

    public static TabCacheManager getInstance() {
        return Holder.MANAGER;
    }

    private TabCacheManager() {

    }

    public Tab findTab(int id) {
        return tabCache.get(id, null);
    }

    public Tab findTab(TabInfo tabInfo) {
        if (tabInfo == null) {
            return null;
        }
        return findTab(tabInfo.getId());
    }

    public Tab findTab(ITab tab) {
        if (tab == null) {
            return null;
        }
        return findTab(tab.getId());
    }

    public void putTab(@NonNull Tab tab) {
        tabCache.put(tab.getId(), tab);
    }

//    @UiThread
//    @NonNull
//    public ArkTabImpl createLivePageByType(@NonNull ITab iTab, @TabLaunchType int type) {
//        long start = System.currentTimeMillis();
//
////        PageInfo pageInfo = PageInfo.from(TabIdManager.getInstance().generateValidId(),
////                Tab.INVALID_PAGE_ID, iTab.getId(),
////                index, iTab.getTabInfo().isIncognito());
//
//        ArkTabImpl tab = ArkTabBuilder.createLiveTab(iTab, false)
//                .setLaunchType(type)
//                .build();
//        putTab(tab);
//
////        tab.loadUrl(params);
//
//        ArkLogger.d(TAG, "createLivePageByType create tab deltaTime=" + (System.currentTimeMillis() - start));
//        return tab;
//    }
//
//    @UiThread
//    @NonNull
//    public ArkTabImpl createLiveTab(@NonNull ITab iTab) {
//        long start = System.currentTimeMillis();
//        ArkLogger.e(this, "createLivePage tabInfo=" + iTab.getTabInfo());
////        ArkTabImpl tab = ArkTabBuilder.createLiveTab(iTab, false)
////                .build();
//
//        ArkTabImpl tab = new ArkTabImpl(iTab);
//
//        putTab(tab);
//
//        tab.initialize(null);
//
//        ArkLogger.d(TAG, "createLivePage create tab deltaTime="
//                + (System.currentTimeMillis() - start));
//        return tab;
//    }
//
//    @UiThread
//    @NonNull
//    public Tab createFrozenPageFromState(@NonNull ITab iTab,
//                                         @NonNull TabState state) {
//        long start = System.currentTimeMillis();
//        Tab tab = ArkTabBuilder.createFromFrozenState(iTab)
//                .setTabState(state)
//                .build();
//        putTab(tab);
//        ArkLogger.d(TAG, "createFrozenPageFromState create tab deltaTime=" + (System.currentTimeMillis() - start));
//        return tab;
//    }

    public void removeTab(Tab tab) {
        tabCache.remove(tab.getId());
        tab.destroy();
    }

    public void removeTab(int id) {
        Tab tab = tabCache.get(id);
        if (tab != null) {
            tabCache.remove(id);
            tab.destroy();
        }
    }

    public void destroy() {
        for (int i = 0; i < tabCache.size(); i++) {
            Tab tab = tabCache.valueAt(i);
            if (tab != null) {
//                tab.detachContentView(tab.getHolder());
                tab.destroy();
            }
        }
        tabCache.clear();
    }

}
