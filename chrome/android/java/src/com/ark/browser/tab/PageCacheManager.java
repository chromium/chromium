package com.ark.browser.tab;

import android.util.SparseArray;

import androidx.annotation.NonNull;
import androidx.annotation.UiThread;

import com.ark.browser.ArkWindowAndroid;
import com.ark.browser.tab.core.IPage;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.utils.ArkLogger;

import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;

public class PageCacheManager {

    private static final String TAG = "PageCacheManager";

    private final SparseArray<Tab> tabCache = new SparseArray<>();

    private static final class Holder {
        private static final PageCacheManager MANAGER = new PageCacheManager();
    }

    public static PageCacheManager getInstance() {
        return Holder.MANAGER;
    }

    private PageCacheManager() {

    }

    public void init(WindowAndroid nativeWindow, TabContentManager contentManager) {
        destroy();
    }

    public Tab findPageById(int id) {
        return tabCache.get(id);
    }

    public Tab findPage(int id) {
        return tabCache.get(id);
    }

    public Tab findPage(PageInfo pageInfo) {
        if (pageInfo == null) {
            return null;
        }
        return findPage(pageInfo.getPageId());
    }

//    public PageInfo findPageInfo(int id) {
//        Tab page = findPage(id);
//        if (page == null) {
//            return null;
//        }
//        return page.getPageInfo();
//    }

//    public Tab findPageById(PageInfo pageInfo) {
//        return findPageById(pageInfo.getPageId());
//    }

    private void putPage(@NonNull Tab tab) {
        tabCache.put(tab.getId(), tab);
    }

//    public void initTabIdle(WindowAndroid window) {
//        ThreadPool.postIdle(() -> Tab.initIdle(window));
//    }

//    @NonNull
//    public Tab createLivePageState(@NonNull TabList tabList, @NonNull TabInfo tabInfo, @NonNull PageInfo pageInfo) {
//        long start = System.currentTimeMillis();
//
//        TabState state = null;
//
//        SiteRedirectItem item = SiteRedirectManager.getRedirectItemByHost(pageInfo.getUrl());
//        if (item == null) {
//            state = TabState.restoreTabState(tabInfo.getTabListFolder(), pageInfo.getPageId());
//            if (state != null && !TextUtils.equals(pageInfo.getUrl(), state.getVirtualUrlFromState())) {
//                item = SiteRedirectManager.getRedirectItemByHost(state.getVirtualUrlFromState());
//                if (item != null) {
//                    state = null;
//                    pageInfo.setUrl(item.getRedirectUrl());
//                }
//            }
//        } else {
//            pageInfo.setUrl(item.getRedirectUrl());
//        }
//
//        Log.d("PageCacheManager", "createLivePageState state=" + state
//                + " pageInfo=" + pageInfo + " isRedirect=" + (item != null)
//                + " deltaTime=" + (System.currentTimeMillis() - start));
//        Tab tab;
//        if (state == null) {
//            tab = Tab.createLiveTab(pageInfo, tabList.getWindowAndroid(), TabLaunchType.FROM_CHROME_UI, false);
//            tab.initialize(null, false, false);
//            LoadUrlParams params = new LoadUrlParams(UrlFormatter.fixupUrl(pageInfo.getUrl()));
//            params.setTransitionType(TabLaunchType.FROM_CHROME_UI);
//            tab.loadUrl(params);
//        } else {
//            tab = Tab.createFrozenTabFromState(pageInfo, tabList.getWindowAndroid(), state);
//            tab.initialize(null, true, true);
//        }
//        putPage(tab);
//        return tab;
//    }

    @UiThread
    @NonNull
    public ArkTabImpl createLivePageByType(@NonNull ITab tabInfo, LoadUrlParams params, int index, @TabLaunchType int type) {
        long start = System.currentTimeMillis();

        PageInfo pageInfo = PageInfo.from(TabIdManager.getInstance().generateValidId(),
                Tab.INVALID_PAGE_ID, tabInfo.getId(),
                index, tabInfo.getTabInfo().isIncognito());

        ArkTabImpl tab = ArkTabBuilder.createLiveTab(pageInfo, false)
                .setLaunchType(type)
                .setLoadUrlParams(params)
                .build();
        putPage(tab);
        ArkLogger.d(TAG, "createLivePageByType create tab deltaTime=" + (System.currentTimeMillis() - start));
        return tab;
    }

    @UiThread
    @NonNull
    public Tab createLivePage(@NonNull PageInfo pageInfo) {
        long start = System.currentTimeMillis();

        ArkLogger.e(this, "createLivePage pageInfo=" + pageInfo);

        Tab tab = ArkTabBuilder.createLiveTab(pageInfo, false)
                .build();
        LoadUrlParams params = new LoadUrlParams(UrlFormatter.fixupUrl(pageInfo.getUrl()));
        params.setTransitionType(TabLaunchType.FROM_CHROME_UI);
        tab.loadUrl(params);
        putPage(tab);
        ArkLogger.d(TAG, "createLivePage create tab deltaTime="
                + (System.currentTimeMillis() - start));
        return tab;
    }

    @UiThread
    @NonNull
    public Tab createFrozenPageFromState(@NonNull PageInfo pageInfo,
                                         @NonNull TabState state) {
        long start = System.currentTimeMillis();
        Tab tab = ArkTabBuilder.createFromFrozenState(pageInfo)
                .setTabState(state)
                .build();
        putPage(tab);
        ArkLogger.d(TAG, "createFrozenPageFromState create tab deltaTime=" + (System.currentTimeMillis() - start));
        return tab;
    }

    public void removePage(Tab tab) {
        tabCache.remove(tab.getId());
        tab.destroy();
    }

    public void removePage(PageInfo pageInfo) {
        removePage(pageInfo.getPageId());
    }

    public void removePage(IPage page) {
        removePage(page.getId());
    }

    public void removePage(int id) {
        Tab tab = tabCache.get(id);
        if (tab != null) {
            tabCache.remove(id);
            tab.destroy();
//            ThreadPool.post(tab::remove);
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
