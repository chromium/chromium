package com.ark.browser.tab.core;

import com.ark.browser.tab.ArkTabImpl;
import com.ark.browser.tab.TabInfo;
import com.ark.browser.tab.TabInfoObserver;
import com.ark.browser.tab.dao.ArkTabDao;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;

import org.chromium.base.ObserverList;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.tab.TabLaunchType;
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

public class GroupTab implements ITabGroup {

    public interface TabStore {

        // TODO

    }

    private static final String TAG = "TabGroupImpl";

    private final ITabGroup mParentTab;
    protected final TabInfo mTabInfo;
    private final List<ITab> mTabList;

    private final ObserverList<TabInfoObserver> mObservers;

    private final Runnable mSaveRunnable = this::saveGroupFile;

    private AsyncTask<DataInputStream> mPrefetchTabGroupTask;

    public GroupTab(ITabGroup parent, TabInfo tabInfo) {
        mParentTab = parent;
        this.mObservers = new ObserverList<>();
        mTabInfo = tabInfo;
        mTabList = new ArrayList<>();
        File groupFile = ArkTabDao.getTabFile(tabInfo.getId());
        if (groupFile.exists()) {
            mPrefetchTabGroupTask = ArkTabDao.fetchFile(groupFile);
        }
    }

    public GroupTab(ITabGroup parent, TabInfo tabInfo, List<ITab> tabs) {
        mParentTab = parent;
        this.mObservers = new ObserverList<>();
        mTabInfo = tabInfo;
        mTabList = tabs;
    }

//    public TabGroupImpl(String id, TabInfo tabInfo) {
//        this.mObservers = new ObserverList<>();
//        mTabInfo = tabInfo;
//        File groupFile = ArkTabDao.getGroupFile(id);
//        if (groupFile.exists()) {
//            mPrefetchTabGroupTask = ArkTabDao.fetchGroupFile(groupFile);
//        }
//    }

//    public TabGroupImpl(String id, boolean incognito, File groupFile) {
//        this(id, incognito);
//        mPrefetchTabGroupTask = ArkTabDao.fetchGroupFile(groupFile);
//    }


    @Override
    public ITabGroup getParentTab() {
        return mParentTab;
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

    public void readGroupFile(DataInputStream is) {
        try {
            int version = is.readInt();
            ArkLogger.e(this, "readGroupFile5 version=" + version);
            mTabInfo.setId(is.readInt());
            mTabInfo.setParentId(is.readInt());
            mTabInfo.setIsGroup(is.readBoolean());
            mTabInfo.setLaunchType(is.readInt());
            mTabInfo.setCreateTime(is.readLong());
            mTabInfo.setIncognito(is.readBoolean());
            mTabInfo.setLocked(is.readBoolean());
            mTabInfo.setIndex(is.readInt());
            mTabInfo.setCurrentPageId(is.readInt());
            mTabInfo.setPosition(is.readInt());
            mTabInfo.setAccessTime(is.readLong());

            int count = is.readInt();
            ArkLogger.e(TabInfo.class, "readGroupFile5 id="
                    + mTabInfo.getId() + " count=" + count);
            for (int i = 0; i < count; i++) {
                int childId = is.readInt();
                ITab newTab = ChildTab.from(this, childId);
                ArkLogger.e(TAG, "readGroupFile5 tabInfo=" + newTab.getTabInfo());
                mTabList.add(newTab);
            }

        } catch (IOException e) {
            e.printStackTrace();
        }
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
        return mTabInfo.getIndex();
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
    public void openNewTab(ITab currentTab, LoadUrlParams loadUrlParams, @TabLaunchType int type) {
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
        ArkTabImpl nativeTab = ArkTabImpl.create(newTab, currentTab);
//        nativeTab.loadInNewPage();

        for (TabInfoObserver obs : getObservers()) {
            obs.didAddTab(newTab, type);
        }
        ArkLogger.d(TAG, "openNewTab loadUrlParams=" + loadUrlParams);

        IPage page = nativeTab.loadInNewPage(loadUrlParams);
        selectTab(newTab, page);
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
    public boolean moveToNewTab(IPage page) {
        ArkLogger.d(TAG, "moveToNewTab");
        ITab tab = findTabById(page.getPageInfo().getTabId());

        // TODO remove instanceof
        if (tab instanceof ChildTab && ((ChildTab) tab).removePage(page)) {
//            selectTab(tab, tab.getCurrentPage());
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
        // TODO
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
        mTabInfo.setIndex(index);
        mTabInfo.setCurrentPageId(getTabAt(index).getId());
        saveTabInfo();
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
            int version = 5;
            os.writeInt(version);
            os.writeInt(tabInfo.getId());
            os.writeInt(tabInfo.getParentId());
            os.writeBoolean(tabInfo.isGroup());
            os.writeInt(tabInfo.getLaunchType());
            os.writeLong(tabInfo.getCreateTime());
            os.writeBoolean(tabInfo.isIncognito());
            os.writeBoolean(tabInfo.isLocked());
            os.writeInt(tabInfo.getChildIndex());
            os.writeInt(tabInfo.getCurrentPageId());
            os.writeInt(tabInfo.getPosition());
            os.writeLong(tabInfo.getAccessTime());
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
                android.util.AtomicFile file = new android.util.AtomicFile(tabFile);
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


}
