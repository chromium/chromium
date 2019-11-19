// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_tab_switcher;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.accessibility_tab_switcher.AccessibilityTabModelListItem.AccessibilityTabModelListItemListener;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

/**
 * An instance of a {@link BaseAdapter} that represents a {@link TabModel}.
 */
public class AccessibilityTabModelAdapter extends BaseAdapter {
    private final Context mContext;

    private TabList mUndoneTabModel;
    private TabModel mActualTabModel;
    private AccessibilityTabModelAdapterListener mListener;
    private final AccessibilityTabModelListView mCanScrollListener;

    /**
     * An interface used to notify that the {@link Tab} specified by {@code tabId} should be
     * shown.
     */
    public interface AccessibilityTabModelAdapterListener {
        /**
         * Show the {@link Tab} specified by {@code tabId}.
         * @param tabId The id of the {@link Tab} that should be shown.
         */
        void showTab(int tabId);
    }

    private final AccessibilityTabModelListItemListener mInternalListener =
            new AccessibilityTabModelListItemListener() {
                @Override
                public void tabSelected(int tab) {
                    if (mListener != null) mListener.showTab(tab);
                    TabModelUtils.setIndex(
                            mActualTabModel, TabModelUtils.getTabIndexById(mActualTabModel, tab));
                    notifyDataSetChanged();
                }

                @Override
                public void tabClosed(int tab) {
                    if (mActualTabModel.isClosurePending(tab)) {
                        mActualTabModel.commitTabClosure(tab);
                    } else {
                        TabModelUtils.closeTabById(mActualTabModel, tab);
                    }
                    notifyDataSetChanged();
                }

                @Override
                public boolean hasPendingClosure(int tab) {
                    return mUndoneTabModel.isClosurePending(tab);
                }

                @Override
                public void schedulePendingClosure(int tab) {
                    mActualTabModel.closeTab(
                            TabModelUtils.getTabById(mActualTabModel, tab), true, false, true);
                    notifyDataSetChanged();
                }

                @Override
                public void cancelPendingClosure(int tab) {
                    mActualTabModel.cancelTabClosure(tab);
                    notifyDataSetChanged();
                }

                @Override
                public void tabChanged(int tabId) {
                    notifyDataSetChanged();
                }
            };

    /**
     * @param context The Context to use to inflate {@link View}s in.
     */
    public AccessibilityTabModelAdapter(Context context, AccessibilityTabModelListView listener) {
        mContext = context;
        mCanScrollListener = listener;
    }

    /**
     * @param tabModel The TabModel that this adapter should represent.
     */
    public void setTabModel(TabModel tabModel) {
        mActualTabModel = tabModel;
        mUndoneTabModel = tabModel.getComprehensiveModel();
        notifyDataSetChanged();
    }

    /**
     * Registers a listener that will be notified when this adapter wants to show a tab.
     * @param listener The listener to be notified of show events.
     */
    public void setListener(AccessibilityTabModelAdapterListener listener) {
        mListener = listener;
    }

    @Override
    public int getCount() {
        return mUndoneTabModel != null ? mUndoneTabModel.getCount() : 0;
    }

    @Override
    public Object getItem(int position) {
        return new Object();
    }

    @Override
    public long getItemId(int position) {
        return mUndoneTabModel != null ? mUndoneTabModel.getTabAt(position).getId()
                                       : Tab.INVALID_TAB_ID;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        int tabId = (int) getItemId(position);
        assert tabId != Tab.INVALID_TAB_ID;

        AccessibilityTabModelListItem listItem;
        if (convertView instanceof AccessibilityTabModelListItem) {
            listItem = (AccessibilityTabModelListItem) convertView;
        } else {
            listItem = (AccessibilityTabModelListItem) LayoutInflater.from(mContext).inflate(
                    R.layout.accessibility_tab_switcher_list_item, null, false);
        }

        listItem.setTab(TabModelUtils.getTabById(mUndoneTabModel, tabId),
                mActualTabModel.supportsPendingClosures());
        listItem.setListeners(mInternalListener, mCanScrollListener);
        listItem.resetState();

        return listItem;
    }
}
