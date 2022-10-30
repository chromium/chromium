package com.ark.browser.tab.core;

import androidx.core.util.AtomicFile;

import com.ark.browser.ArkWindowAndroid;
import com.ark.browser.tab.PageCacheManager;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.TabInfo;
import com.ark.browser.tab.TabInfoObserver;
import com.ark.browser.tab.dao.ArkTabDao;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;

import org.chromium.base.ObserverList;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.LoadUrlParams;

import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class TabGroupImpl implements ITabGroup {

    private static final String TAG = "TabGroupImpl";

    private final List<ITab> mTabList = new ArrayList<>();

    private final ArkWindowAndroid nativeWindow;

    private final ObserverList<TabInfoObserver> mObservers;

    private final boolean incognito;

    protected int index = ITab.INVALID_TAB_INDEX;

    private AsyncTask<DataInputStream> mPrefetchTabGroupTask;

    public TabGroupImpl(ArkWindowAndroid nativeWindow, boolean incognito) {
        this.nativeWindow = nativeWindow;
        this.mObservers = new ObserverList<>();
        this.incognito = incognito;
    }

    public TabGroupImpl(ArkWindowAndroid nativeWindow, boolean incognito, File groupFile) {
        this.nativeWindow = nativeWindow;
        this.mObservers = new ObserverList<>();
        this.incognito = incognito;
        mPrefetchTabGroupTask = ArkTabDao.fetchGroupFile(groupFile);
    }

//    private void saveGroupFile() {
//
//
//        try {
//            ByteArrayOutputStream stream = new ByteArrayOutputStream();
//            DataOutputStream os = new DataOutputStream(stream);
//            int version = 1;
//            os.writeInt(version);
//            os.writeInt(TabGroupImpl.this.index);
//            os.writeBoolean(incognito);
//            os.writeInt(mTabList.size());
//            for (ITab tab : mTabList) {
//                os.writeLong(tab.getId());
//            }
//            os.close();
//
//            byte[] bytes = stream.toByteArray();
//
//            int id = incognito ? 1 : 0;
//
//            ThreadPool.executeIO(new Runnable() {
//                @Override
//                public void run() {
//                    File groupFile = new File(ArkTabDao.getGroupsDir(), "group_" + id);
//                    AtomicFile file = new AtomicFile(groupFile);
//                    FileOutputStream fos = null;
//                    try {
//                        fos = file.startWrite();
//                        fos.write(bytes, 0, bytes.length);
//                        file.finishWrite(fos);
//                    } catch (IOException e) {
//                        if (fos != null) file.failWrite(fos);
//                        ArkLogger.e(this, "Failed to write file: " + file.getBaseFile().getAbsolutePath());
//                    }
//                }
//            });
//
//        } catch (IOException e) {
//            e.printStackTrace();
//        }
//
//
//
//
//
////        int id = incognito ? 1 : 0;
////        File groupFile = new File(ArkTabDao.getGroupsDir(), "group_" + id);
////        try (DataOutputStream os = new DataOutputStream(
////                new BufferedOutputStream(new FileOutputStream(groupFile)))) {
////            int version = 1;
////            os.writeInt(version);
////            os.writeInt(TabGroupImpl.this.index);
////            os.writeBoolean(incognito);
////            os.writeInt(mTabList.size());
////            for (ITab tab : mTabList) {
////                os.writeLong(tab.getId());
////            }
////            os.flush();
////        } catch (IOException e) {
////            e.printStackTrace();
////        }
//    }

    public long[] readGroupFile(DataInputStream stream) {
        try {
            final int version = stream.readInt();

            index = stream.readInt();
            final boolean isIncognito = stream.readBoolean();

            final int count = stream.readInt();

            long[] tabIds = new long[count];
            for (int i = 0; i < count; i++) {
                long tabId = stream.readLong();
                tabIds[i] = tabId;
//            if (tabIds != null) tabIds.append(tabId, true);
            }
            return tabIds;
        } catch (IOException e) {
            e.printStackTrace();
        }
        return new long[0];

    }

    @Override
    public void init(ArkWindowAndroid nativeWindow) {
        long start = System.currentTimeMillis();
        this.index = ITab.INVALID_TAB_INDEX;
        this.mTabList.clear();
//        this.mObservers.clear();


        if (mPrefetchTabGroupTask != null) {
            try (DataInputStream stream = mPrefetchTabGroupTask.get()) {
                long[] tabIds = readGroupFile(stream);

                for (long id : tabIds) {
                    File tabFile = ArkTabDao.getTabFile(id);
                    List<Integer> pageIds = new ArrayList<>();
                    TabInfo tabInfo = TabInfo.from(tabFile, pageIds);
                    ArkLogger.e(TAG, "from tabInfo=" + tabInfo + " pageIds=" + pageIds);

                    File pagesDir = ArkTabDao.getPagesDir(tabInfo.getTabInfoId());
                    List<IPage> pageList = new ArrayList<>();
                    for (int pageId : pageIds) {
                        File file = new File(pagesDir, String.valueOf(pageId));
                        pageList.add(new PageImpl(PageInfo.from(file)));
                    }

//                    List<IPage> pageList = new ArrayList<>();
//                    for (PageInfo info : tabInfo.getPageInfoList()) {
//                        pageList.add(new PageImpl(info));
//                    }
                    mTabList.add(new TabImpl(tabInfo, pageList));
                }


            } catch (Exception e) {
                e.printStackTrace();
            }
        }

        ArkLogger.d(TAG, "load deltaTime=" + (System.currentTimeMillis() - start));


//        List<PageInfo> allPages = PageInfoManager.getAllPages();
//
//        Map<String, List<PageInfo>> pageListMap = new HashMap<>();
//        for (PageInfo pageInfo : allPages) {
//            String tabInfoId = pageInfo.getTabInfoId();
//            List<PageInfo> pages = pageListMap.get(tabInfoId);
//            if (pages == null) {
//                pages = new ArrayList<>();
//                pageListMap.put(tabInfoId, pages);
//            }
////            Log.d(TAG, "deltaTime pageInfo " + pageInfo.getTabInfoId() + "-" + pageInfo.getOriginalIndex());
//            pages.add(pageInfo);
//        }
//
//
//        for (TabInfo tabInfo : TabInfoManager.getAllTabs()) {
////            long startTime = System.currentTimeMillis();
//            String tabId = tabInfo.getTabInfoId();
//            List<PageInfo> pages = pageListMap.get(tabId);
//            if (pages == null || pages.isEmpty()) {
//                continue;
//            }
//            List<IPage> pageList = new ArrayList<>();
//            for (PageInfo info : pages) {
//                pageList.add(new PageImpl(info));
//            }
//            mTabList.add(new TabImpl(tabInfo, pageList));
////            Log.d(TAG, "restore deltaTime=" + (System.currentTimeMillis() - startTime));
//        }
//
//        ArkLogger.d(TAG, "load deltaTime=" + (System.currentTimeMillis() - start));
//
//        allPages.clear();
//        pageListMap.clear();
    }

    @Override
    public int getId() {
        return incognito ? 1 : 0;
    }

    @Override
    public boolean isIncognito() {
        return incognito;
    }

    @Override
    public int getIndex() {
        return index;
    }

    @Override
    public ArkWindowAndroid getWindowAndroid() {
        return nativeWindow;
    }

    @Override
    public List<ITab> getTabInfoList() {
        return mTabList;
    }

    @Override
    public ObserverList<TabInfoObserver> getObservers() {
        return mObservers;
    }

    @Override
    public ITab getTabAt(int i) {
        if (i < 0 || i >= mTabList.size()) {
            return null;
        }
        return mTabList.get(i);
    }

    @Override
    public ITab cloneTab(ITab tab) {
        ArkLogger.d(TAG, "cloneTab");
        if (tab != null) {
            ITab cloneTab = tab.cloneTab();
            int index = indexOf(tab) + 1;
            int position = tab.getTabInfo().getPosition() + 1;
            ArkLogger.d(TAG, "cloneTab index=" + index);
            mTabList.add(index, cloneTab);
            cloneTab.getTabInfo().setPosition(position);
//            cloneTab.getTabInfo().save();
            cloneTab.saveTabInfo();

            saveTabPosition(index, position);

            selectTab(tab, tab.getCurrentPage());

            return cloneTab;
        }
        return null;
    }

    public int indexOf(ITab targetTab) {
        int index = 0;
        for (ITab tab : mTabList) {
            if (tab.getId() == targetTab.getId()) {
                return index;
            }
            index++;
        }
        return -1;
    }

    @Override
    public void openNewTab(ITab currentTab, LoadUrlParams loadUrlParams, @TabLaunchType int type) {
        ArkLogger.e(TAG, "openNewTab url=" + loadUrlParams.getUrl() + " type=" + type);


        ITab newTab = new TabImpl();

        if (currentTab != null) {
            int index = indexOf(currentTab);
            int position = currentTab.getTabInfo().getPosition();
            ArkLogger.d(TAG, "openNewTab currentPosition=" + position + " index=" + index);

            mTabList.add(++index, newTab);
            newTab.getTabInfo().setPosition(++position);
            newTab.saveTabInfo();

            saveTabPosition(index, position);
        } else {
            newTab.getTabInfo().setPosition(getCount());
            mTabList.add(newTab);
        }

        ArkLogger.d(TAG, "openNewTab newPos=" + newTab.getTabInfo().getPosition());


        loadUrlParams.setUrl(UrlFormatter.fixupUrl(loadUrlParams.getUrl()).getValidSpecOrEmpty());
//        loadUrlParams.setTransitionType(type);

        Tab page = PageCacheManager.getInstance().createLivePageByType(
                newTab.getPageSize(), getWindowAndroid(), newTab, type);

//        PageInfo pageInfo = page.getPageInfo();

        PageInfo pageInfo = PageInfo.from(page.getId(), Tab.INVALID_PAGE_ID, newTab.getId(),
                newTab.getPageSize(), page.isIncognito());

        pageInfo.setUrl(page.getUrl().toString());
        pageInfo.setTitle(page.getTitle());

        IPage newPage = new PageImpl(pageInfo);
        newPage.savePageInfo();
        newTab.getPageGroup().getPageInfoList().add(newPage);

        for (TabInfoObserver obs : getObservers()) {
            obs.didAddTab(newPage, type);
        }
        ArkLogger.d(TAG, "openNewTab loadUrlParams=" + loadUrlParams);
        page.loadUrl(loadUrlParams);
        selectTab(newTab, newPage);
    }

    public ITab getTabInfo(PageInfo pageInfo) {
        if (pageInfo == null) {
            return null;
        }
        return getTabInfoById(pageInfo.getTabInfoId());
    }

    public boolean isClosurePending(int pageId) {
        IPage page = getCurrentPage();
        if (page == null) {
            return false;
        }
        return page.getId() != pageId;
    }

    @Override
    public boolean openNewPage(Tab parent, LoadUrlParams params, @TabLaunchType int type) {
        ArkLogger.d(TAG, "openNewPage params=" + params + " type=" + type);

        int parentId = parent.getId();
        // The parent tab was already closed.  Do not open child tabs.
        if (isClosurePending(parentId)) return false;

        // If parent is in the same tab model, place the new tab next to it.
        ITab iTab = getTabById(parentId);
        if (iTab == null) {
            return false;
        }

//        int index = iTab.indexOfPage(parentId);
//        ArkLogger.d(TAG, "openNewPage index=" + index);
//        if (index == ITab.INVALID_TAB_INDEX) {
//            return false;
//        }
//
//        PageInfo parentPageInfo = iTab.getPageInfoAt(index);
//        ArkLogger.d(TAG, "openNewPage parentPageInfo=" + parentPageInfo);
//        if (parentPageInfo == null) {
//            return false;
//        }
//
//        int pageIndex = parentPageInfo.getOriginalIndex() + 1;
//        Tab tab = PageCacheManager.getInstance().createLivePageByType(
//                pageIndex, getWindowAndroid(), iTab, type);
////        PageInfo pageInfo = tab.getPageInfo();
//
//        PageInfo pageInfo = PageInfo.from(tab.getId(), parentId, iTab.getId(),
//                pageIndex, tab.isIncognito());
//        pageInfo.setUrl(tab.getUrl().toString());
//        pageInfo.setTitle(tab.getTitle());
////        pageInfo.save();
//
//        IPage page = new PageImpl(pageInfo);
//        page.savePageInfo();
//        IPageGroup pageInfoList = iTab.getPageGroup();
//
//        pageInfoList.getPageInfoList().add(++index, page);
//
//        for (TabInfoObserver obs : mObservers) {
//            obs.didAddTab(page, TabSelectionType.FROM_NEW);
//        }
//
//
//        ArkLogger.d(TAG, "openNewPage params=" + params);
//        tab.loadUrl(params);

        IPage page = iTab.createPage(parent, params, type);

        if (page == null) {
            return false;
        }

        for (TabInfoObserver obs : mObservers) {
            obs.didAddTab(page, TabSelectionType.FROM_NEW);
        }

        return selectTab(iTab, page);


//        if (++index < pageInfoList.getCount()) {
//            List<IPage> pageRemoved = pageInfoList.getPageInfoList()
//                    .subList(index, pageInfoList.getCount());
//
//            List<IPage> tempPages = new ArrayList<>(pageRemoved);
//            ThreadPool.executeIO(() -> {
//                long start = System.currentTimeMillis();
//                ArkLogger.d(TAG, "openNewPage pageRemovedCount=" + tempPages.size());
//
//                for (IPage info : tempPages) {
//                    info.remove();
//                }
//
//                ArkLogger.d(TAG, "openNewPage pageRemoved deltaTime=" + (System.currentTimeMillis() - start));
//            });
//            pageRemoved.clear();
//        }
//
//        ArkLogger.d(TAG, "openNewPage end");
//        return true;
    }

    @Override
    public boolean moveToNewTab(IPage page) {
        ArkLogger.d(TAG, "moveToNewTab");
        ITab tabInfo = getTabInfoById(page.getId());
        if (tabInfo != null && tabInfo.removePage(page)) {
            TabInfo newTabInfo = TabInfo.create();
            ITab newTab = new TabImpl(newTabInfo);
            page.getPageInfo().setTabInfoId(newTabInfo.getTabInfoId());
            newTab.getPageGroup().addPage(page);

            int index = indexOf(tabInfo) + 1;
            int position = tabInfo.getTabInfo().getPosition() + 1;
            getTabInfoList().add(index, newTab);
            newTabInfo.setPosition(position);
//            newTabInfo.save();
            newTab.saveTabInfo();

            saveTabPosition(index, position);

            selectTab(newTab, page);
            return true;
        }
        return false;
    }

    /**
     * Subscribes a {@link TabInfoObserver} to be notified about changes to this model.
     *
     * @param observer The observer to be subscribed.
     */
    public void addObserver(TabInfoObserver observer) {
        this.mObservers.addObserver(observer);
    }

    /**
     * Unsubscribes a previously subscribed {@link TabInfoObserver}.
     *
     * @param observer The observer to be unsubscribed.
     */
    public void removeObserver(TabInfoObserver observer) {
        this.mObservers.removeObserver(observer);
    }

    @Override
    public void destroy() {
        this.mObservers.clear();
        for (ITab info : mTabList) {
            info.destroy();
        }
        this.mTabList.clear();
        this.index = ITab.INVALID_TAB_INDEX;
    }

    @Override
    public void onIndexChanged(int index) {
        ArkLogger.e(TAG, "onIndexChanged index=" + index);
        this.index = index;
//        PrefsHelper.with().putInt("tab_index", index);

        ThreadPool.executeIO(this::saveGroupFile);

    }

    private static final int MAX_CHANGE_COUNT = 10;

    private void saveTabPosition(int index, int position) {
        long start = System.currentTimeMillis();
        int i = index;
        int pos = position;
        ITab next;
        List<ITab> changes = new ArrayList<>();

        ArkLogger.d(TAG, "saveTabPosition count=" + getCount() + " i=" + i + " pos=" + pos);
        while ((next = getTabAt(++i)) != null && pos >= next.getTabInfo().getPosition()) {
            next.getTabInfo().setPosition(++pos);
//            next.update();

            changes.add(next);

            if (changes.size() > MAX_CHANGE_COUNT) {
                i = index;
                pos = pos + 1;
                changes.clear();
                while ((next = getTabAt(++i)) != null && pos >= next.getTabInfo().getPosition()) {
                    changes.add(next);
                    if (changes.size() > MAX_CHANGE_COUNT) {
                        pos++;
                    } else {
                        pos += 2;
                    }
                    next.getTabInfo().setPosition(pos);
                }
                break;
            }
        }
        ArkLogger.d(TAG, "saveTabPosition dt=" + (System.currentTimeMillis() - start) + " size=" + changes.size());

        if (changes.isEmpty()) {
            return;
        }


        ThreadPool.executeIO(() -> {
            saveGroupFile();

            for (ITab tab : changes) {
                tab.saveTabInfo();
            }
        });

//        ThreadPool.executeIO(() -> {
//            long t = System.currentTimeMillis();
//            DatabaseWrapper db = FlowManager.getDatabase(TabInfoManager.class)
//                    .getWritableDatabase();
//            try {
//                db.beginTransaction();
//                Log.d(TAG, "saveTabPosition beginTransaction");
//                int j = 0;
//                for (TabInfo tabInfo : changes) {
//                    db.execSQL(String.format(
//                            "update TabInfo set position=%s where tabInfoId='%s'",
//                            tabInfo.getPosition(), tabInfo.getTabInfoId())
//                    );
//                    Log.d(TAG, "saveTabPosition j=" + (j++));
//                }
//                db.setTransactionSuccessful();
//                Log.d(TAG, "saveTabPosition setTransactionSuccessful");
//            } finally {
//                db.endTransaction();
//                Log.d(TAG, "saveTabPosition endTransaction");
//            }
//            Log.d(TAG, "saveTabPosition deltaTime=" + (System.currentTimeMillis() - t) + " size=" + changes.size());
//        });
    }


}
