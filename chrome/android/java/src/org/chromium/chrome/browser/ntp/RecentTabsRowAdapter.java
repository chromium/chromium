// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.ArrayMap;
import android.util.LruCache;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.MenuItem.OnMenuItemClickListener;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseExpandableListAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.IntDef;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.favicon.FaviconUtils;
import org.chromium.chrome.browser.ntp.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.ntp.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.ntp.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.signin.SyncPromoView;
import org.chromium.chrome.browser.ui.widget.RoundedIconGenerator;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Row adapter for presenting recently closed tabs, synced tabs from other devices, the sync or
 * sign in promo, and currently open tabs (only in document mode) in a grouped list view.
 */
public class RecentTabsRowAdapter extends BaseExpandableListAdapter {
    private static final int MAX_NUM_FAVICONS_TO_CACHE = 128;

    @IntDef({ChildType.NONE, ChildType.DEFAULT_CONTENT, ChildType.PERSONALIZED_SIGNIN_PROMO,
            ChildType.SYNC_PROMO})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ChildType {
        // Values should be enumerated from 0 and can't have gaps.
        int NONE = 0;
        int DEFAULT_CONTENT = 1;
        int PERSONALIZED_SIGNIN_PROMO = 2;
        int SYNC_PROMO = 3;
        /**
         * Number of entries.
         */
        int NUM_ENTRIES = 4;
    }

    @IntDef({GroupType.CONTENT, GroupType.VISIBLE_SEPARATOR, GroupType.INVISIBLE_SEPARATOR})
    @Retention(RetentionPolicy.SOURCE)
    private @interface GroupType {
        // Values should be enumerated from 0 and can't have gaps.
        int CONTENT = 0;
        int VISIBLE_SEPARATOR = 1;
        int INVISIBLE_SEPARATOR = 2;
        /**
         * Number of entries.
         */
        int NUM_ENTRIES = 3;
    }

    // Values from the OtherSessionsActions enum in histograms.xml; do not change these values or
    // histograms will be broken.
    @IntDef({OtherSessionsActions.MENU_INITIALIZED, OtherSessionsActions.LINK_CLICKED,
            OtherSessionsActions.COLLAPSE_SESSION, OtherSessionsActions.EXPAND_SESSION,
            OtherSessionsActions.OPEN_ALL, OtherSessionsActions.HAS_FOREIGN_DATA,
            OtherSessionsActions.HIDE_FOR_NOW})
    @Retention(RetentionPolicy.SOURCE)
    private @interface OtherSessionsActions {
        int MENU_INITIALIZED = 0;
        int LINK_CLICKED = 2;
        int COLLAPSE_SESSION = 6;
        int EXPAND_SESSION = 7;
        int OPEN_ALL = 8;
        int HAS_FOREIGN_DATA = 9;
        int HIDE_FOR_NOW = 10;

        int NUM_ENTRIES = 11;
    }

    @IntDef({FaviconLocality.LOCAL, FaviconLocality.FOREIGN})
    @Retention(RetentionPolicy.SOURCE)
    private @interface FaviconLocality {
        int LOCAL = 0;
        int FOREIGN = 1;

        int NUM_ENTRIES = 2;
    }

    private final Activity mActivity;
    private final List<Group> mGroups;
    private final DefaultFaviconHelper mDefaultFaviconHelper;
    private final RecentTabsManager mRecentTabsManager;
    private final RecentlyClosedTabsGroup mRecentlyClosedTabsGroup = new RecentlyClosedTabsGroup();
    private final SeparatorGroup mVisibleSeparatorGroup = new SeparatorGroup(true);
    private final SeparatorGroup mInvisibleSeparatorGroup = new SeparatorGroup(false);
    private final Map<Integer, FaviconCache> mFaviconCaches =
            new ArrayMap<>(FaviconLocality.NUM_ENTRIES);
    private final int mFaviconSize;
    private boolean mHasForeignDataRecorded;
    private RoundedIconGenerator mIconGenerator;

    /**
     * A generic group of objects to be shown in the RecentTabsRowAdapter, such as the list of
     * recently closed tabs.
     */
    abstract class Group {
        /**
         * @return The type of group: GroupType.CONTENT or GroupType.SEPARATOR.
         */
        abstract @GroupType int getGroupType();

        /**
         * @return The number of children in this group.
         */
        abstract int getChildrenCount();

        /**
         * @return The child type.
         */
        abstract @ChildType int getChildType();

        /**
         * @param childPosition The position for which to return the child.
         * @return The child at the position childPosition.
         */
        Object getChild(int childPosition) {
            return null;
        }

        /**
         * Returns the view corresponding to the child view at a given position.
         *
         * @param childPosition The position of the child.
         * @param isLastChild Whether this child is the last one.
         * @param convertView The re-usable child view (may be null).
         * @param parent The parent view group.
         *
         * @return The view corresponding to the child.
         */
        View getChildView(int childPosition, boolean isLastChild,
                View convertView, ViewGroup parent) {
            View childView = convertView;
            if (childView == null) {
                LayoutInflater inflater = LayoutInflater.from(mActivity);
                childView = inflater.inflate(R.layout.recent_tabs_list_item, parent, false);

                ViewHolder viewHolder = new ViewHolder();
                viewHolder.textView = (TextView) childView.findViewById(R.id.title_row);
                viewHolder.domainView = (TextView) childView.findViewById(R.id.domain_row);
                viewHolder.imageView = (ImageView) childView.findViewById(R.id.recent_tabs_favicon);
                viewHolder.imageView.setBackgroundResource(R.drawable.list_item_icon_modern_bg);
                viewHolder.itemLayout = childView.findViewById(R.id.recent_tabs_list_item_layout);
                childView.setTag(viewHolder);
            }

            ViewHolder viewHolder = (ViewHolder) childView.getTag();
            configureChildView(childPosition, viewHolder);

            return childView;
        }

        /**
         * Configures a view inflated from recent_tabs_list_item.xml to display information about
         * a child in this group.
         *
         * @param childPosition The position of the child within this group.
         * @param viewHolder The ViewHolder with references to pieces of the view.
         */
        void configureChildView(int childPosition, ViewHolder viewHolder) {}

        /**
         * Returns the view corresponding to this group.
         *
         * @param isExpanded Whether the group is expanded.
         * @param convertView The re-usable group view (may be null).
         * @param parent The parent view group.
         *
         * @return The view corresponding to the group.
         */
        public View getGroupView(boolean isExpanded, View convertView, ViewGroup parent) {
            RecentTabsGroupView groupView = (RecentTabsGroupView) convertView;
            if (groupView == null) {
                groupView = (RecentTabsGroupView) LayoutInflater.from(mActivity).inflate(
                        R.layout.recent_tabs_group_item, parent, false);
            }
            configureGroupView(groupView, isExpanded);
            return groupView;
        }

        /**
         * Configures an RecentTabsGroupView to display the header of this group.
         * @param groupView The RecentTabsGroupView to configure.
         * @param isExpanded Whether the view is currently expanded.
         */
        abstract void configureGroupView(RecentTabsGroupView groupView, boolean isExpanded);

        /**
         * Sets whether this group is collapsed (i.e. whether only the header is visible).
         */
        abstract void setCollapsed(boolean isCollapsed);

        /**
         * @return Whether this group is collapsed.
         */
        abstract boolean isCollapsed();

        /**
         * Called when a child item is clicked.
         * @param childPosition The position of the child in the group.
         * @return Whether the click was handled.
         */
        boolean onChildClick(int childPosition) {
            return false;
        }

        /**
         * Called when the context menu for the group view is being built.
         * @param menu The context menu being built.
         * @param activity The current activity.
         */
        void onCreateContextMenuForGroup(ContextMenu menu, Activity activity) {
        }

        /**
         * Called when a context menu for one of the child views is being built.
         * @param childPosition The position of the child in the group.
         * @param menu The context menu being built.
         * @param activity The current activity.
         */
        void onCreateContextMenuForChild(int childPosition, ContextMenu menu,
                Activity activity) {
        }
    }

    /**
     * A group containing all the tabs associated with a foreign session from a synced device.
     */
    class ForeignSessionGroup extends Group {
        private final ForeignSession mForeignSession;

        ForeignSessionGroup(ForeignSession foreignSession) {
            mForeignSession = foreignSession;
        }

        @Override
        public @GroupType int getGroupType() {
            return GroupType.CONTENT;
        }

        @Override
        public int getChildrenCount() {
            int count = 0;
            for (ForeignSessionWindow window : mForeignSession.windows) {
                count += window.tabs.size();
            }
            return count;
        }

        @Override
        public @ChildType int getChildType() {
            return ChildType.DEFAULT_CONTENT;
        }

        @Override
        public ForeignSessionTab getChild(int childPosition) {
            for (ForeignSessionWindow window : mForeignSession.windows) {
                if (childPosition < window.tabs.size()) {
                    return window.tabs.get(childPosition);
                }
                childPosition -= window.tabs.size();
            }
            assert false;
            return null;
        }

        @Override
        public void configureChildView(int childPosition, ViewHolder viewHolder) {
            ForeignSessionTab sessionTab = getChild(childPosition);
            String text = TextUtils.isEmpty(sessionTab.title) ? sessionTab.url : sessionTab.title;
            viewHolder.textView.setText(text);
            String domain = UrlUtilities.getDomainAndRegistry(sessionTab.url, false);
            if (!TextUtils.isEmpty(domain)) {
                viewHolder.domainView.setText(domain);
                viewHolder.domainView.setVisibility(View.VISIBLE);
            } else {
                viewHolder.domainView.setText("");
                viewHolder.domainView.setVisibility(View.GONE);
            }
            loadFavicon(viewHolder, sessionTab.url, FaviconLocality.FOREIGN);
        }

        @Override
        public void configureGroupView(RecentTabsGroupView groupView, boolean isExpanded) {
            groupView.configureForForeignSession(mForeignSession, isExpanded);
        }

        @Override
        public void setCollapsed(boolean isCollapsed) {
            if (isCollapsed) {
                RecordHistogram.recordEnumeratedHistogram("HistoryPage.OtherDevicesMenu",
                        OtherSessionsActions.COLLAPSE_SESSION, OtherSessionsActions.NUM_ENTRIES);
            } else {
                RecordHistogram.recordEnumeratedHistogram("HistoryPage.OtherDevicesMenu",
                        OtherSessionsActions.EXPAND_SESSION, OtherSessionsActions.NUM_ENTRIES);
            }
            mRecentTabsManager.setForeignSessionCollapsed(mForeignSession, isCollapsed);
        }

        @Override
        public boolean isCollapsed() {
            return mRecentTabsManager.getForeignSessionCollapsed(mForeignSession);
        }

        @Override
        public boolean onChildClick(int childPosition) {
            RecordHistogram.recordEnumeratedHistogram("HistoryPage.OtherDevicesMenu",
                    OtherSessionsActions.LINK_CLICKED, OtherSessionsActions.NUM_ENTRIES);
            ForeignSessionTab foreignSessionTab = getChild(childPosition);
            mRecentTabsManager.openForeignSessionTab(mForeignSession, foreignSessionTab,
                    WindowOpenDisposition.CURRENT_TAB);
            return true;
        }

        @Override
        public void onCreateContextMenuForGroup(ContextMenu menu, Activity activity) {
            menu.add(R.string.recent_tabs_open_all_menu_option).setOnMenuItemClickListener(item -> {
                RecordHistogram.recordEnumeratedHistogram("HistoryPage.OtherDevicesMenu",
                        OtherSessionsActions.OPEN_ALL, OtherSessionsActions.NUM_ENTRIES);
                openAllTabs();
                return true;
            });
            menu.add(R.string.recent_tabs_hide_menu_option).setOnMenuItemClickListener(item -> {
                RecordHistogram.recordEnumeratedHistogram("HistoryPage.OtherDevicesMenu",
                        OtherSessionsActions.HIDE_FOR_NOW, OtherSessionsActions.NUM_ENTRIES);
                mRecentTabsManager.deleteForeignSession(mForeignSession);
                return true;
            });
        }

        @Override
        public void onCreateContextMenuForChild(int childPosition, ContextMenu menu,
                Activity activity) {
            final ForeignSessionTab foreignSessionTab = getChild(childPosition);
            OnMenuItemClickListener listener = item -> {
                mRecentTabsManager.openForeignSessionTab(mForeignSession, foreignSessionTab,
                        WindowOpenDisposition.NEW_BACKGROUND_TAB);
                return true;
            };
            menu.add(R.string.contextmenu_open_in_new_tab).setOnMenuItemClickListener(listener);
        }

        private void openAllTabs() {
            ForeignSessionTab firstTab = null;
            for (ForeignSessionWindow window : mForeignSession.windows) {
                for (ForeignSessionTab tab : window.tabs) {
                    if (firstTab == null) {
                        firstTab = tab;
                    } else {
                        mRecentTabsManager.openForeignSessionTab(
                                mForeignSession, tab, WindowOpenDisposition.NEW_BACKGROUND_TAB);
                    }
                }
            }
            // Open the first tab last because calls to openForeignSessionTab after one for
            // CURRENT_TAB are ignored.
            if (firstTab != null) {
                mRecentTabsManager.openForeignSessionTab(
                        mForeignSession, firstTab, WindowOpenDisposition.CURRENT_TAB);
            }
        }
    }

    /**
     * A base group for promos.
     */
    private abstract class PromoGroup extends Group {
        @Override
        @GroupType
        int getGroupType() {
            return GroupType.CONTENT;
        }

        @Override
        int getChildrenCount() {
            return 1;
        }

        @Override
        void configureGroupView(RecentTabsGroupView groupView, boolean isExpanded) {
            groupView.configureForPromo(isExpanded);
        }

        @Override
        void setCollapsed(boolean isCollapsed) {
            mRecentTabsManager.setPromoCollapsed(isCollapsed);
        }

        @Override
        boolean isCollapsed() {
            return mRecentTabsManager.isPromoCollapsed();
        }
    }

    /**
     * A group containing the personalized signin promo.
     */
    class PersonalizedSigninPromoGroup extends PromoGroup {
        @Override
        @ChildType
        int getChildType() {
            return ChildType.PERSONALIZED_SIGNIN_PROMO;
        }

        @Override
        View getChildView(
                int childPosition, boolean isLastChild, View convertView, ViewGroup parent) {
            if (convertView == null) {
                LayoutInflater layoutInflater = LayoutInflater.from(parent.getContext());
                convertView = layoutInflater.inflate(
                        R.layout.personalized_signin_promo_view_recent_tabs, parent, false);
            }
            mRecentTabsManager.setupPersonalizedSigninPromo(
                    convertView.findViewById(R.id.signin_promo_view_container));
            return convertView;
        }
    }

    /**
     * A group containing the sync promo.
     */
    class SyncPromoGroup extends PromoGroup {
        @Override
        public @ChildType int getChildType() {
            return ChildType.SYNC_PROMO;
        }

        @Override
        View getChildView(
                int childPosition, boolean isLastChild, View convertView, ViewGroup parent) {
            if (convertView == null) {
                convertView = SyncPromoView.create(parent, SigninAccessPoint.RECENT_TABS);
            }
            return convertView;
        }
    }

    /**
     * A group containing tabs that were recently closed on this device and a link to the history
     * page.
     */
    class RecentlyClosedTabsGroup extends Group {
        static final int ID_OPEN_IN_NEW_TAB = 1;
        static final int ID_REMOVE_ALL = 2;

        @Override
        public @GroupType int getGroupType() {
            return GroupType.CONTENT;
        }

        @Override
        public int getChildrenCount() {
            // The number of children is the number of recently closed tabs, plus one for the "Show
            // full history" item.
            return 1 + mRecentTabsManager.getRecentlyClosedTabs().size();
        }

        @Override
        public @ChildType int getChildType() {
            return ChildType.DEFAULT_CONTENT;
        }

        /**
         * @param childPosition The index of an item in the recently closed list.
         * @return Whether the item at childPosition is the link to the history page.
         */
        private boolean isHistoryLink(int childPosition) {
            return childPosition == mRecentTabsManager.getRecentlyClosedTabs().size();
        }

        @Override
        public RecentlyClosedTab getChild(int childPosition) {
            if (isHistoryLink(childPosition)) return null;
            return mRecentTabsManager.getRecentlyClosedTabs().get(childPosition);
        }

        @Override
        public void configureChildView(int childPosition, ViewHolder viewHolder) {
            // Reset the domain view text manually since it does not always reset itself, which can
            // lead to wrong pairings of domain & title texts.
            viewHolder.domainView.setText("");
            viewHolder.domainView.setVisibility(View.GONE);
            if (isHistoryLink(childPosition)) {
                viewHolder.textView.setText(R.string.show_full_history);
                Bitmap historyIcon = BitmapFactory.decodeResource(
                        mActivity.getResources(), R.drawable.ic_watch_later_24dp);
                int size = mActivity.getResources().getDimensionPixelSize(
                        R.dimen.tile_view_icon_size_modern);
                Drawable drawable =
                        FaviconUtils.createRoundedBitmapDrawable(mActivity.getResources(),
                                Bitmap.createScaledBitmap(historyIcon, size, size, true));
                drawable.setColorFilter(ApiCompatibilityUtils.getColor(mActivity.getResources(),
                                                R.color.default_icon_color),
                        PorterDuff.Mode.SRC_IN);
                viewHolder.imageView.setImageDrawable(drawable);
                viewHolder.itemLayout.getLayoutParams().height =
                        mActivity.getResources().getDimensionPixelSize(
                                R.dimen.recent_tabs_show_history_item_size);
                return;
            }
            viewHolder.itemLayout.getLayoutParams().height =
                    mActivity.getResources().getDimensionPixelSize(
                            R.dimen.recent_tabs_foreign_session_group_item_height);
            RecentlyClosedTab tab = getChild(childPosition);
            String title = TitleUtil.getTitleForDisplay(tab.title, tab.url);
            viewHolder.textView.setText(title);

            String domain = UrlUtilities.getDomainAndRegistry(tab.url, false);
            if (!TextUtils.isEmpty(domain)) {
                viewHolder.domainView.setText(domain);
                viewHolder.domainView.setVisibility(View.VISIBLE);
            }
            loadFavicon(viewHolder, tab.url, FaviconLocality.LOCAL);
        }

        @Override
        public void configureGroupView(RecentTabsGroupView groupView, boolean isExpanded) {
            groupView.configureForRecentlyClosedTabs(isExpanded);
        }

        @Override
        public void setCollapsed(boolean isCollapsed) {
            mRecentTabsManager.setRecentlyClosedTabsCollapsed(isCollapsed);
        }

        @Override
        public boolean isCollapsed() {
            return mRecentTabsManager.isRecentlyClosedTabsCollapsed();
        }

        @Override
        public boolean onChildClick(int childPosition) {
            if (isHistoryLink(childPosition)) {
                mRecentTabsManager.openHistoryPage();
            } else {
                mRecentTabsManager.openRecentlyClosedTab(getChild(childPosition),
                        WindowOpenDisposition.CURRENT_TAB);
            }
            return true;
        }

        @Override
        public void onCreateContextMenuForGroup(ContextMenu menu, Activity activity) {
        }

        @Override
        public void onCreateContextMenuForChild(final int childPosition, ContextMenu menu,
                Activity activity) {
            final RecentlyClosedTab recentlyClosedTab = getChild(childPosition);
            if (recentlyClosedTab == null) return;
            OnMenuItemClickListener listener = item -> {
                switch (item.getItemId()) {
                    case ID_REMOVE_ALL:
                        mRecentTabsManager.clearRecentlyClosedTabs();
                        break;
                    case ID_OPEN_IN_NEW_TAB:
                        mRecentTabsManager.openRecentlyClosedTab(
                                recentlyClosedTab, WindowOpenDisposition.NEW_BACKGROUND_TAB);
                        break;
                    default:
                        assert false;
                }
                return true;
            };
            menu.add(ContextMenu.NONE, ID_OPEN_IN_NEW_TAB, ContextMenu.NONE,
                    R.string.contextmenu_open_in_new_tab).setOnMenuItemClickListener(listener);
            menu.add(ContextMenu.NONE, ID_REMOVE_ALL, ContextMenu.NONE,
                    R.string.remove_all).setOnMenuItemClickListener(listener);
        }
    }

    /**
     * A group containing a blank separator.
     */
    class SeparatorGroup extends Group {
        private final boolean mIsVisible;

        public SeparatorGroup(boolean isVisible) {
            mIsVisible = isVisible;
        }

        @Override
        public @GroupType int getGroupType() {
            return mIsVisible ? GroupType.VISIBLE_SEPARATOR : GroupType.INVISIBLE_SEPARATOR;
        }

        @Override
        public @ChildType int getChildType() {
            return ChildType.NONE;
        }

        @Override
        public int getChildrenCount() {
            return 0;
        }

        @Override
        public View getGroupView(boolean isExpanded, View convertView, ViewGroup parent) {
            if (convertView == null) {
                int layout = mIsVisible
                        ? R.layout.recent_tabs_group_separator_visible
                        : R.layout.recent_tabs_group_separator_invisible;
                convertView = LayoutInflater.from(mActivity).inflate(layout, parent, false);
            }
            return convertView;
        }

        @Override
        public void configureGroupView(RecentTabsGroupView groupView, boolean isExpanded) {
        }

        @Override
        public void setCollapsed(boolean isCollapsed) {
        }

        @Override
        public boolean isCollapsed() {
            return false;
        }
    }

    private static class FaviconCache {
        private final LruCache<String, Drawable> mMemoryCache;

        public FaviconCache(int size) {
            mMemoryCache = new LruCache<>(size);
        }

        Drawable getFaviconImage(String url) {
            return mMemoryCache.get(url);
        }

        public void putFaviconImage(String url, Drawable image) {
            mMemoryCache.put(url, image);
        }
    }

    /**
     * Creates a RecentTabsRowAdapter used to populate an ExpandableList with other
     * devices and foreign tab cells.
     *
     * @param activity The Android activity this adapter will work in.
     * @param recentTabsManager The RecentTabsManager that will act as the data source.
     */
    public RecentTabsRowAdapter(Activity activity, RecentTabsManager recentTabsManager) {
        mActivity = activity;
        mRecentTabsManager = recentTabsManager;
        mGroups = new ArrayList<>();
        mFaviconCaches.put(FaviconLocality.LOCAL, new FaviconCache(MAX_NUM_FAVICONS_TO_CACHE));
        mFaviconCaches.put(FaviconLocality.FOREIGN, new FaviconCache(MAX_NUM_FAVICONS_TO_CACHE));

        Resources resources = activity.getResources();
        mDefaultFaviconHelper = new DefaultFaviconHelper();
        mFaviconSize = resources.getDimensionPixelSize(R.dimen.default_favicon_size);

        mIconGenerator = FaviconUtils.createCircularIconGenerator(activity.getResources());

        RecordHistogram.recordEnumeratedHistogram("HistoryPage.OtherDevicesMenu",
                OtherSessionsActions.MENU_INITIALIZED, OtherSessionsActions.NUM_ENTRIES);
    }

    /**
     * ViewHolder class optimizes looking up table row fields. findViewById is only called once
     * per row view initialization, and the references are cached here. Also stores a reference to
     * the favicon image callback; so that we can make sure we load the correct favicon.
     */
    private static class ViewHolder {
        public TextView textView;
        public TextView domainView;
        public ImageView imageView;
        public View itemLayout;
        public FaviconImageCallback imageCallback;
    }

    private void loadFavicon(
            final ViewHolder viewHolder, final String url, @FaviconLocality int locality) {
        Drawable image;
        if (url == null) {
            // URL is null for print jobs, for example.
            image = mDefaultFaviconHelper.getDefaultFaviconDrawable(
                    mActivity.getResources(), url, true);
        } else {
            image = mFaviconCaches.get(locality).getFaviconImage(url);
            if (image == null) {
                FaviconImageCallback imageCallback = new FaviconImageCallback() {
                    @Override
                    public void onFaviconAvailable(Bitmap bitmap, String iconUrl) {
                        if (this != viewHolder.imageCallback) return;
                        Drawable faviconDrawable = FaviconUtils.getIconDrawableWithFilter(bitmap,
                                url, mIconGenerator, mDefaultFaviconHelper,
                                mActivity.getResources(), mFaviconSize);
                        mFaviconCaches.get(locality).putFaviconImage(url, faviconDrawable);
                        viewHolder.imageView.setImageDrawable(faviconDrawable);
                    }
                };
                viewHolder.imageCallback = imageCallback;
                switch (locality) {
                    case FaviconLocality.LOCAL:
                        mRecentTabsManager.getLocalFaviconForUrl(url, mFaviconSize, imageCallback);
                        break;
                    case FaviconLocality.FOREIGN:
                        mRecentTabsManager.getForeignFaviconForUrl(
                                url, mFaviconSize, imageCallback);
                        break;
                }

                image = mDefaultFaviconHelper.getDefaultFaviconDrawable(
                        mActivity.getResources(), url, true);
            }
        }
        viewHolder.imageView.setImageDrawable(image);
    }

    @Override
    public View getChildView(int groupPosition, int childPosition, boolean isLastChild,
            View convertView, ViewGroup parent) {
        return getGroup(groupPosition)
                .getChildView(childPosition, isLastChild, convertView, parent);
    }

    // BaseExpandableListAdapter group related implementations
    @Override
    public int getGroupCount() {
        return mGroups.size();
    }

    @Override
    public long getGroupId(int groupPosition) {
        return groupPosition;
    }

    @Override
    public Group getGroup(int groupPosition) {
        return mGroups.get(groupPosition);
    }

    @Override
    public View getGroupView(int groupPosition, boolean isExpanded, View convertView,
            ViewGroup parent) {
        return getGroup(groupPosition).getGroupView(isExpanded, convertView, parent);
    }

    // BaseExpandableListAdapter child related implementations
    @Override
    public int getChildrenCount(int groupPosition) {
        return getGroup(groupPosition).getChildrenCount();
    }

    @Override
    public long getChildId(int groupPosition, int childPosition) {
        return childPosition;
    }

    @Override
    public Object getChild(int groupPosition, int childPosition) {
        return getGroup(groupPosition).getChild(childPosition);
    }

    @Override
    public boolean isChildSelectable(int groupPosition, int childPosition) {
        return true;
    }

    // BaseExpandableListAdapter misc. implementation
    @Override
    public boolean hasStableIds() {
        return false;
    }

    @Override
    public int getGroupType(int groupPosition) {
        return getGroup(groupPosition).getGroupType();
    }

    @Override
    public int getGroupTypeCount() {
        return GroupType.NUM_ENTRIES;
    }

    private void addGroup(Group group) {
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            mGroups.add(group);
        } else {
            if (mGroups.size() == 0) {
                mGroups.add(mInvisibleSeparatorGroup);
            }
            mGroups.add(group);
            mGroups.add(mInvisibleSeparatorGroup);
        }
    }

    @Override
    public void notifyDataSetChanged() {
        mGroups.clear();
        addGroup(mRecentlyClosedTabsGroup);
        for (ForeignSession session : mRecentTabsManager.getForeignSessions()) {
            if (!mHasForeignDataRecorded) {
                RecordHistogram.recordEnumeratedHistogram("HistoryPage.OtherDevicesMenu",
                        OtherSessionsActions.HAS_FOREIGN_DATA, OtherSessionsActions.NUM_ENTRIES);
                mHasForeignDataRecorded = true;
            }
            addGroup(new ForeignSessionGroup(session));
        }

        switch (mRecentTabsManager.getPromoType()) {
            case RecentTabsManager.PromoState.PROMO_NONE:
                break;
            case RecentTabsManager.PromoState.PROMO_SIGNIN_PERSONALIZED:
                addGroup(new PersonalizedSigninPromoGroup());
                break;
            case RecentTabsManager.PromoState.PROMO_SYNC:
                addGroup(new SyncPromoGroup());
                break;
            default:
                assert false : "Unexpected value for promo type!";
        }

        // Add separator line after the recently closed tabs group.
        int recentlyClosedIndex = mGroups.indexOf(mRecentlyClosedTabsGroup);
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            if (recentlyClosedIndex != mGroups.size() - 2) {
                mGroups.set(recentlyClosedIndex + 1, mVisibleSeparatorGroup);
            }
        }

        super.notifyDataSetChanged();
    }

    @Override
    public int getChildType(int groupPosition, int childPosition) {
        return mGroups.get(groupPosition).getChildType();
    }

    @Override
    public int getChildTypeCount() {
        return ChildType.NUM_ENTRIES;
    }
}
