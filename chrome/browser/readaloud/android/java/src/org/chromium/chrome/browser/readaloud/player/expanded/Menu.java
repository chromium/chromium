// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.readaloud.player.R;

import java.util.HashMap;
import java.util.Map;

public class Menu extends LinearLayout {
    // Menu item constructor params.
    private final Context mContext;
    private final Map<Integer, Integer> mItemIdToIndex;
    private LinearLayout mItemsContainer;
    private int mFirstItemIndex;
    private int mLastItemIndex;

    private Callback<Integer> mItemClickHandler;
    private Callback<Integer> mRadioTrueHandler;
    private Callback<Integer> mPlayButtonClickHandler;

    public Menu(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mItemIdToIndex = new HashMap<>();
        mFirstItemIndex = -1;
        mLastItemIndex = -1;
    }

    MenuItem addItem(int itemId, int iconId, String label, @MenuItem.Action int action) {
        return addItem(itemId, iconId, label, action, "");
    }

    public MenuItem addItem(
            int itemId,
            int iconId,
            String label,
            @MenuItem.Action int action,
            String contentDescription) {
        if (mItemsContainer == null) {
            mItemsContainer = (LinearLayout) findViewById(R.id.items_container);
        }
        MenuItem item =
                new MenuItem(mContext, this, itemId, iconId, label, action, contentDescription);
        mItemsContainer.addView(
                item,
                /* width= */ LayoutParams.MATCH_PARENT,
                /* height= */ LayoutParams.WRAP_CONTENT);
        int index = mItemsContainer.indexOfChild(item);
        if (mFirstItemIndex < 0) {
            mFirstItemIndex = index;
        }
        mLastItemIndex = index;

        mItemIdToIndex.put(itemId, index);
        return item;
    }

    public MenuItem getItem(int itemId) {
        if (!mItemIdToIndex.containsKey(itemId)) {
            return null;
        }
        return (MenuItem) mItemsContainer.getChildAt(mItemIdToIndex.get(itemId));
    }

    void clearItems() {
        if (mFirstItemIndex >= 0) {
            mItemsContainer.removeViews(mFirstItemIndex, mLastItemIndex - mFirstItemIndex + 1);
        }
        mFirstItemIndex = -1;
        mLastItemIndex = -1;
        mItemIdToIndex.clear();
    }

    void setItemClickHandler(Callback<Integer> handler) {
        mItemClickHandler = handler;
    }

    void onItemClicked(int itemId) {
        if (mItemClickHandler != null) {
            mItemClickHandler.onResult(itemId);
        }
    }

    void setPlayButtonClickHandler(Callback<Integer> handler) {
        mPlayButtonClickHandler = handler;
    }

    void onPlayButtonClicked(int itemId) {
        if (mPlayButtonClickHandler != null) {
            mPlayButtonClickHandler.onResult(itemId);
        }
    }

    void setRadioTrueHandler(Callback<Integer> handler) {
        mRadioTrueHandler = handler;
    }

    void onRadioButtonSelected(int itemId) {
        for (Map.Entry<Integer, Integer> itemIndex : mItemIdToIndex.entrySet()) {
            if (itemIndex.getKey() != itemId) {
                MenuItem item = (MenuItem) mItemsContainer.getChildAt(itemIndex.getValue());
                item.setValue(false);
            }
        }
        if (mRadioTrueHandler != null) {
            mRadioTrueHandler.onResult(itemId);
        }
    }
}
