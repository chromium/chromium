package com.ark.browser.tab.core;

import android.util.AtomicFile;

import androidx.annotation.NonNull;

import com.ark.browser.core.ArkWebContents;
import com.ark.browser.tab.ArkTabImpl;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.TabCacheManager;
import com.ark.browser.tab.TabGroupManager;
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
import org.chromium.content_public.browser.WebContents;

import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class GroupTab implements ITabGroup {

    public interface TabStore {

        // TODO

    }

    private static final String TAG = "TabGroupImpl";

    private ITabGroup mParentTab;
    protected final TabInfo mTabInfo;
    private final List<ITab> mTabList;

    private final ObserverList<TabInfoObserver> mObservers;

    private final Runnable mSaveRunnable = this::saveGroupFile;

    private final TabGroupManager.Selector mSelector;

    private AsyncTask<DataInputStream> mPrefetchTabGroupTask;

    public GroupTab(TabGroupManager.Selector selector, ITabGroup parent) {
        this(selector, parent, TabInfo.create(parent.getId(), true));
    }

    public GroupTab(TabGroupManager.Selector selector, ITabGroup parent, TabInfo tabInfo) {
        this(selector, parent, tabInfo, new ArrayList<>(0));
        File groupFile = ArkTabDao.getTabFile(tabInfo.getId());
        if (groupFile.exists()) {
            mPrefetchTabGroupTask = ArkTabDao.readFileAsync(groupFile);
        }
    }

    private GroupTab(TabGroupManager.Selector selector, ITabGroup parent, TabInfo tabInfo, List<ITab> tabs) {
        this(selector, parent, tabInfo, tabs, new ObserverList<>());
    }

    private GroupTab(TabGroupManager.Selector selector, ITabGroup parent, TabInfo tabInfo, List<ITab> tabs, ObserverList<TabInfoObserver> observers) {
        mSelector = selector;
        mParentTab = parent;
        mObservers = observers;
        mObservers.disableThreadAsserts();
        mTabInfo = tabInfo;
        mTabList = tabs;
    }

    public TabGroupManager.Selector getSelector() {
        return mSelector;
    }

    @Override
    public ITabGroup getParentGroup() {
        return mParentTab;
    }

    @Override
    public void setParentGroup(ITabGroup group) {
        mParentTab = group;
    }

    @Override
    public ITab cloneByGroupTab(ITabGroup group) {
        GroupTab tab = new GroupTab(mSelector, group, mTabInfo, new ArrayList<>(mTabList), mObservers);
        mTabInfo.setParentId(group.getId());
        tab.mPrefetchTabGroupTask = mPrefetchTabGroupTask;
        return tab;
    }

    @Override
    public TabInfo getTabInfo() {
        return mTabInfo;
    }

    @Override
    public ITab cloneTab() {
        // TODO clone group
        return null;
    }

    private void readGroupFile(DataInputStream is) {
        try {
            mTabInfo.fromStream(is);
            int count = is.readInt();
            ArkLogger.e(TabInfo.class, "readGroupFile id="
                    + mTabInfo.getId() + " count=" + count);
            List<Integer> list = new ArrayList<>();
            for (int i = 0; i < count; i++) {
                int childId = is.readInt();
                list.add(childId);
                ITab newTab = restoreTab(this, childId);
                ArkLogger.e(TAG, "readGroupFile newTab=" + newTab);
                ArkLogger.e(TAG, "readGroupFile tabInfo=" + newTab.getTabInfo());
                mTabList.add(newTab);
            }


//            list.add(-100);
//            ArkLogger.e(TAG, "restoreLostTab list=" + list);
//            File dir = ArkTabDao.getTabDir();
//            File[] files = dir.listFiles(new FileFilter() {
//                @Override
//                public boolean accept(File file) {
//                    String id = file.getName().substring(3);
//                    boolean accept = !list.contains(Integer.parseInt(id));
//                    ArkLogger.e(GroupTab.TAG, "restoreLostTab accept=" + accept + " id=" + id);
//                    return accept;
//                }
//            });
//
//
//            ArkLogger.e(TAG, "restoreLostTab files size=" + files.length);
//            SparseArray<List<ITab>> lostTabsMap = new SparseArray<>();
//
//            for (File file : files) {
//                ArkLogger.e(TAG, "restoreLostTab file=" + file);
//                try {
//                    ITab tab = restoreTab(null, file);
//
//                    int parentId = tab.getTabInfo().getParentId();
//                    ArkLogger.e(TAG, "restoreLostTab parentId=" + parentId + " tab=" + tab);
//
//                    List<ITab> tabs = lostTabsMap.get(parentId, null);
//                    if (tabs == null) {
//                        tabs = new ArrayList<>();
//                        lostTabsMap.put(parentId, tabs);
//                    }
//                    tabs.add(tab);
//                } catch (Exception e) {
//                    e.printStackTrace();
//                }
//            }
//
//            ArkLogger.e(TAG, "restoreLostTab lostTabsMap size=" + lostTabsMap.size());
//
//
//            List<ITab> tabList = new ArrayList<>(mTabList);
//            if (lostTabsMap.get(-100, null) != null) {
//                tabList.addAll(lostTabsMap.get(-100));
//                lostTabsMap.remove(-100);
//            }
//
//            Collections.sort(tabList, new Comparator<ITab>() {
//                @Override
//                public int compare(ITab t0, ITab t1) {
//                    return Integer.compare(t0.getTabInfo().getPosition(), t1.getTabInfo().getPosition());
//                }
//            });
//
//            if (lostTabsMap.get(ITab.INVALID_TAB_INDEX, null) != null) {
//                int pos = tabList.get(tabList.size() - 1).getTabInfo().getPosition();
//                for (ITab tab : lostTabsMap.get(ITab.INVALID_TAB_INDEX)) {
//                    tab.getTabInfo().setParentId(-100);
//                    tab.getTabInfo().setPosition(++pos);
//                    tabList.add(tab);
//                }
//                lostTabsMap.remove(ITab.INVALID_TAB_INDEX);
//            }
//
//
//
//            for (ITab tab : tabList) {
//                ArkLogger.e(TAG, "restoreLostTab title=" + tab.getTitle());
//            }
//
//            for (int i = 0; i < lostTabsMap.size(); i++) {
//                int groupId = lostTabsMap.keyAt(i);
//                List<ITab> tabs = lostTabsMap.get(groupId);
//                ArkLogger.e(TAG, "restoreLostTab groupId=" + groupId
//                        + " size=" + tabs.size() + " tabs=" + tabs);
//                ITab groupTab = null;
//
//                for (ITab tab : tabList) {
//                    if (tab.getId() == groupId) {
//                        groupTab = tab;
//                        break;
//                    }
//                }
//
//                if (groupTab == null) {
//                    ArkLogger.e(TAG, "restoreLostTab group id=" + groupId + " null");
//                } else {
//                    ArkLogger.e(TAG, "restoreLostTab group id=" + groupId + " title=" + groupTab.getTitle());
//                }
//            }
//
//            mTabList.clear();
//            mTabList.addAll(tabList);
//            saveTabInfo();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void restoreLastTab() {

    }

    @Override
    public void init() {
        long start = System.currentTimeMillis();
        this.mTabList.clear();
//        this.mObservers.clear();

        if (mPrefetchTabGroupTask != null) {
            try (DataInputStream stream = mPrefetchTabGroupTask.get()) {
                readGroupFile(stream);
            } catch (Exception e) {
                ArkLogger.e(this, "init failed!", e);
                e.printStackTrace();
            }
            mPrefetchTabGroupTask = null;
        }
        ArkLogger.d(TAG, "load deltaTime=" + (System.currentTimeMillis() - start));
    }

    @Override
    public boolean isIncognito() {
        return mTabInfo.isIncognito();
    }

    @Override
    public int getIndex() {
        return mTabInfo.getChildIndex();
    }

    @Override
    public List<ITab> getTabList() {
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
    public boolean closeTab(ITab tab) {
        return removeTab(tab, true);
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
            cloneTab.saveTabInfo();
            selectTab(tab, tab.getCurrentPage());
            saveTabPosition(index, position);
            return cloneTab;
        }
        return null;
    }

    @Override
    public void openInNewTab(ITab currentTab, WebContents webContents, LoadUrlParams loadUrlParams) {
        ArkLogger.e(TAG, "openNewTab url=" + webContents.getVisibleUrl()
                + " params=" + loadUrlParams);
        ChildTab newTab = new ChildTab(this);
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

        newTab.getTabInfo().setLaunchType(TabLaunchType.FROM_LINK);
        ArkTabImpl nativeTab = ArkTabImpl.create(newTab, currentTab);

        for (TabInfoObserver obs : mObservers) {
            obs.didAddTab(newTab, TabSelectionType.FROM_NEW);
        }

        IPage page = nativeTab.loadInNewPage(webContents, loadUrlParams);
        selectTab(newTab, page);
    }

    @Override
    public void openInNewTab(ITab currentTab, LoadUrlParams loadUrlParams, @TabLaunchType int type) {
        ArkLogger.e(TAG, "openNewTab url=" + loadUrlParams.getUrl() + " type=" + type);


        ChildTab newTab = new ChildTab(this);

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
            saveTabInfo();
        }

        ArkLogger.d(TAG, "openNewTab newPos=" + newTab.getTabInfo().getPosition());


        loadUrlParams.setUrl(UrlFormatter.fixupUrl(loadUrlParams.getUrl()).getValidSpecOrEmpty());
//        loadUrlParams.setTransitionType(type);


//        PageInfo pageInfo = PageInfo.from(newTab.getTabInfo().getId(),
//                newTab.getPageSize(),
//                newTab.getTabInfo().isIncognito());
//        IPage newPage = new PageImpl(pageInfo);
//        newTab.getPageGroup().getPageInfoList().add(newPage);

        newTab.getTabInfo().setLaunchType(type);
//        nativeTab.loadInNewPage();

        // TODO optimise
//        mSelector.notifyChanged();
        for (TabInfoObserver obs : mObservers) {
            obs.didAddTab(newTab, type);
        }
        ArkLogger.d(TAG, "openNewTab loadUrlParams=" + loadUrlParams);

        if (type == TabLaunchType.FROM_LONGPRESS_BACKGROUND) {
            IPage page = newTab.openNewPage();
            page.getPageInfo().setUrl(loadUrlParams.getUrl());
            newTab.selectPage(page);
            ArkWebContents arkWeb = ArkWebContents.Builder.createLiveTab(page.getPageInfo(), true)
                    .setLoadUrlParams(loadUrlParams)
                    .build();
            arkWeb.loadUrlInternal(loadUrlParams);
        } else {
            ArkTabImpl nativeTab = ArkTabImpl.create(newTab, currentTab);
            IPage page = nativeTab.loadInNewPage(loadUrlParams);
            newTab.saveTabInfo();
            selectTab(newTab, page);
        }
    }

    @Override
    public void openInNewGroup(ITab currentTab, LoadUrlParams loadUrlParams, int type) {
        ArkLogger.e(TAG, "openInNewGroup url=" + loadUrlParams.getUrl() + " type=" + type);
        List<ITab> tabs = new ArrayList<>(0);
        TabInfo tabInfo = TabInfo.create(getId(), true, System.currentTimeMillis());
        tabInfo.setLaunchType(type);
        GroupTab newGroup = new GroupTab(mSelector, this, tabInfo, tabs);

        int index;
        int lastId;
        if (currentTab != null) {
            tabs.add(currentTab);

            lastId = currentTab.getId();
            index = indexOf(currentTab);
            mTabList.set(index, newGroup);
            currentTab.getTabInfo().setParentId(newGroup.getId());
            currentTab.getTabInfo().setPosition(0);
            currentTab.saveTabInfo();
        } else {
            lastId = ITab.INVALID_TAB_INDEX;
            index = getCount();
            mTabList.add(newGroup);
        }
        newGroup.getTabInfo().setPosition(index);
        newGroup.saveTabInfo();
        onIndexChanged(index);
        saveTabInfo();

        ArkLogger.d(TAG, "openInNewGroup group=" + newGroup.getTabInfo());

        // TODO optimise
        mSelector.selectGroup(newGroup);
        for (TabInfoObserver obs : mObservers) {
            ArkLogger.d(GroupTab.this, "selectTabInfo obs=" + obs);
            obs.didSelectTab(newGroup, TabSelectionType.FROM_USER, lastId);
        }

        ArkLogger.d(TAG, "openInNewGroup loadUrlParams=" + loadUrlParams);

        newGroup.openInNewTab(currentTab, loadUrlParams, type);
    }

    public boolean isClosurePending(int pageId) {
        IPage page = getCurrentPage();
        if (page == null) {
            return false;
        }
        return page.getId() != pageId;
    }

//    @Override
//    public boolean openNewPage(Tab parent, LoadUrlParams params, @TabLaunchType int type) {
//        ArkLogger.d(TAG, "openNewPage params=" + params + " type=" + type);
//
//        int parentId = parent.getId();
//        // The parent tab was already closed.  Do not open child tabs.
//        if (isClosurePending(parentId)) return false;
//
//        // If parent is in the same tab model, place the new tab next to it.
//        ITab iTab = getTabById(parentId);
//        if (iTab == null) {
//            return false;
//        }
//
//        IPage page = iTab.createPage(parent, params, type);
//
//        if (page == null) {
//            return false;
//        }
//
//        for (TabInfoObserver obs : mObservers) {
//            obs.didAddTab(iTab, TabSelectionType.FROM_NEW);
//        }
//
//        return selectTab(iTab, page);
//    }


    @Override
    public boolean moveToNewTab(ITab tab, IPage page) {
        ArkLogger.d(TAG, "moveToNewTab");
        // TODO remove instanceof
        if (tab instanceof ChildTab && ((ChildTab) tab).removePage(page)) {
            TabInfo newTabInfo = TabInfo.create(getId());
            List<IPage> pages = new ArrayList<>();
            page.getPageInfo().setTabId(newTabInfo.getId());
            pages.add(page);
            ITab newTab = new ChildTab(this, newTabInfo, pages);

            int index = indexOf(tab) + 1;
            int position = tab.getTabInfo().getPosition() + 1;
            getTabList().add(index, newTab);
            newTabInfo.setPosition(position);
            newTab.saveTabInfo();
            selectTab(newTab, page);
            saveTabPosition(index, position);
            page.savePageInfo();
            return true;
        }
        return false;
    }

    @Override
    public boolean moveToNewGroup(ITab tab, boolean selected) {
        if (tab.getParentGroup() == this) {
            return false;
        }
        ITabGroup oldParent = tab.getParentGroup();
        if (oldParent == null || oldParent.removeTab(tab, false)) {
            tab.setParentGroup(this);
            tab.getTabInfo().setParentId(getId());
            ITab lastTab = getTabAt(mTabList.size() - 1);
            if (lastTab == null) {
                tab.getTabInfo().setPosition(0);
            } else {
                tab.getTabInfo().setPosition(lastTab.getTabInfo().getPosition() + 1);
            }
            mTabList.add(tab);
            tab.saveTabInfo();
            saveTabInfo();

            Tab nativeTab = TabCacheManager.getInstance().findTab(tab);
            if (nativeTab != null && nativeTab.getWindowAndroid() != null) {
                ITab current = tab;
                ITabGroup parent = tab.getParentGroup();
                while (parent != null) {
                    parent.onIndexChanged(parent.indexOf(current));
                    current = parent;
                    parent = parent.getParentGroup();
                }
                mSelector.selectGroup(tab.getParentGroup());
            }

            mSelector.notifyTabMoved(tab, oldParent);
            for (TabInfoObserver observer : mObservers) {
                observer.didMoveTab(tab);
            }
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
        mObservers.addObserver(observer);
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
        if (mPrefetchTabGroupTask != null) {
            if (!mPrefetchTabGroupTask.isCancelled()) {
                mPrefetchTabGroupTask.cancel(true);
            }
            mPrefetchTabGroupTask = null;
        }
        ThreadPool.removeCallbacks(mSaveRunnable);
        this.mObservers.clear();
        if (!this.mTabList.isEmpty()) {
            for (ITab info : mTabList) {
                info.destroy();
            }
            this.mTabList.clear();
        }
    }

    @Override
    public void remove() {
        ArkTabDao.deleteTabFile(getId());
        for (ITab tab : mTabList) {
            tab.remove();
        }
        mTabList.clear();
        destroy();
    }

    @Override
    public void saveTabInfo() {
        ThreadPool.removeCallbacks(mSaveRunnable);
        ThreadPool.postOnUIThread(mSaveRunnable);
    }

    @Override
    public void onIndexChanged(int index) {
        ArkLogger.e(TAG, "onIndexChanged index=" + index);
        if (getIndex() == index) {
            return;
        }
        mTabInfo.setChildIndex(index);
        ITab current = getTabAt(index);
        if (current != null) {
            mTabInfo.setCurrentPageId(current.getId());
        }
        saveTabInfo();
    }

    @Override
    public String toString() {
        return "GroupTab{" +
                "info=" + mTabInfo +
                '}';
    }

    private static final int MAX_CHANGE_COUNT = 10;

    private void saveTabPosition(int index, int position) {
        if (mTabList.isEmpty()) {
            return;
        }
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
//                changes.clear();
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

        saveTabInfo();

        if (changes.isEmpty()) {
            return;
        }


        start = System.currentTimeMillis();
        for (ITab tab : changes) {
            tab.saveTabInfo();
        }
        ArkLogger.e(TAG, "saveTabPosition chagesSize=" + changes.size()
                + " saveTabInfo deltaTime=" + (System.currentTimeMillis() - start));


    }

//    static TabGroupImpl from(DataInputStream is) throws IOException {
//        TabInfo newTabInfo = new TabInfo();
//        List<ITab> tabs = new ArrayList<>();
//        int version = is.readInt();
//        newTabInfo.setId(is.readInt());
//        if (version >= 3) {
//            if (version >= 5) {
//                newTabInfo.setParentId(is.readInt());
//            } else {
//                String name = is.readUTF();
//                if ("group_incognito".equals(name)) {
//                    newTabInfo.setParentId(-101);
//                } else if ("group_default".equals(name)) {
//                    newTabInfo.setParentId(-100);
//                } else {
//                    newTabInfo.setParentId(-1);
//                }
//            }
//
//        }
//        if (version >= 4) {
//            newTabInfo.setIsGroup(is.readBoolean());
//        } else {
//            newTabInfo.setIsGroup(false);
//        }
//        if (version >= 2) {
//            newTabInfo.setLaunchType(is.readInt());
//        }
//        newTabInfo.setCreateTime(is.readLong());
//        newTabInfo.setIncognito(is.readBoolean());
//        newTabInfo.setLocked(is.readBoolean());
//
//        newTabInfo.setPageIndex(is.readInt());
//        newTabInfo.setCurrentPageId(is.readInt());
//        newTabInfo.setPosition(is.readInt());
//        newTabInfo.setAccessTime(is.readLong());
//
//        int count = is.readInt();
//        ArkLogger.e(TabInfo.class, "TabInfo.from id=" + newTabInfo.getId() + " count=" + count);
//        for (int i = 0; i < count; i++) {
//            int id = is.readInt();
//            File tabFile = ArkTabDao.getTabFile(id);
//            ArkLogger.e(TAG, "from tabInfo tabFile=" + tabFile);
//            ITab newTab = ITab.from(tabFile);
//            ArkLogger.e(TAG, "from tabInfo=" + newTab.getTabInfo());
//            tabs.add(newTab);
//        }
//
//        return new TabGroupImpl(newTabInfo, tabs);
//    }

    private void saveGroupFile() {
        try {
            TabInfo tabInfo = mTabInfo;
            long time = System.currentTimeMillis();
            ByteArrayOutputStream stream = new ByteArrayOutputStream();
            DataOutputStream os = new DataOutputStream(stream);
            tabInfo.wrapStream(os);
            os.writeInt(getCount());

            ArkLogger.e(this, "saveGroupFile info=" + tabInfo
                    + " getTabList=" + mTabList);
            for (ITab tab : mTabList) {
                os.writeInt(tab.getId());
            }
            os.close();

            ArkLogger.e(this, "saveGroupFile to byte deltaTime="
                    + (System.currentTimeMillis() - time));

            ThreadPool.executeIO(() -> {
                long time1 = System.currentTimeMillis();
                byte[] bytes = stream.toByteArray();
                File tabFile = ArkTabDao.getTabFile(getId());
                ArkLogger.e(GroupTab.this, "saveGroupFile file=" + tabFile);
                AtomicFile file = new AtomicFile(tabFile);
                FileOutputStream fos = null;
                try {
                    fos = file.startWrite();
                    fos.write(bytes, 0, bytes.length);
                    file.finishWrite(fos);
                } catch (IOException e) {
                    if (fos != null) file.failWrite(fos);
                    ArkLogger.e(GroupTab.this, "saveGroupFile Failed to write file: " + file.getBaseFile().getAbsolutePath());
                }
                ArkLogger.e(GroupTab.this, "saveGroupFile deltaTime="
                        + (System.currentTimeMillis() - time1));
            });
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public static ITab restoreTab(@NonNull ITabGroup parent, int tabId) {
        File tabFile = ArkTabDao.getTabFile(tabId);
        ArkLogger.e(ChildTab.class, "from id=" + tabId + " tabFile=" + tabFile);
        try {
            return restoreTab(parent, tabFile);
        } catch (IOException e) {
            e.printStackTrace();
            // TODO
            return new ChildTab(parent, TabInfo.create(tabId, -1, false));
        }
    }

    private static ITab restoreTab(@NonNull ITabGroup parent, File tabFile) throws IOException {
        try (DataInputStream stream = ArkTabDao.readFileAtomic(tabFile)) {
            return restoreTab(parent, stream);
        }
    }

    private static ITab restoreTab(@NonNull ITabGroup parent, DataInputStream is) throws IOException {
        TabInfo newTabInfo = TabInfo.create(is);

        int count = is.readInt();
        ArkLogger.e(TabInfo.class, "restoreTab newTabInfo=" + newTabInfo);

        boolean isGroup = newTabInfo.isGroup();

        if (isGroup) {
            List<ITab> tabs = new ArrayList<>();
            ITabGroup newGroupTab = new GroupTab(parent.getSelector(), parent, newTabInfo, tabs);
            for (int i = 0; i < count; i++) {
                int childId = is.readInt();
                ITab newTab = restoreTab(newGroupTab, childId);
                ArkLogger.e(TAG, "restoreTab tabInfo=" + newTab.getTabInfo());
                tabs.add(newTab);
            }
            return newGroupTab;
        } else {
            File pagesDir = ArkTabDao.getPagesDir(newTabInfo.getId());
            List<IPage> pages = new ArrayList<>();
            for (int i = 0; i < count; i++) {
                int pageId = is.readInt();
                File file = new File(pagesDir, String.valueOf(pageId));
                PageInfo pageInfo;
                try {
                    pageInfo = PageInfo.from(file);
                } catch (Exception e) {
                    e.printStackTrace();
                    ArkLogger.e(TabInfo.class, "restoreTab page restore failed! pageId=" + pageId);
                    pageInfo = PageInfo.createBreakPageInfo(pageId, newTabInfo.getId(), newTabInfo.isIncognito());
                }

                ArkLogger.e(TabInfo.class, "restoreTab pageId=" + pageId + " pageInfo=" + pageInfo);
                pages.add(new PageImpl(pageInfo));
            }
            return new ChildTab(parent, newTabInfo, pages);
        }
    }


}
