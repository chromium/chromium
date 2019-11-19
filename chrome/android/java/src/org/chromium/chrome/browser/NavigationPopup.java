// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.support.v7.content.res.AppCompatResources;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.ListPopupWindow;
import android.widget.PopupWindow;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashSet;
import java.util.Set;

/**
 * A popup that handles displaying the navigation history for a given tab.
 */
public class NavigationPopup implements AdapterView.OnItemClickListener {
    private static final int MAXIMUM_HISTORY_ITEMS = 8;
    private static final int FULL_HISTORY_ENTRY_INDEX = -1;

    /** Specifies the type of navigation popup being shown */
    @IntDef({Type.ANDROID_SYSTEM_BACK, Type.TABLET_BACK, Type.TABLET_FORWARD})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        int ANDROID_SYSTEM_BACK = 0;
        int TABLET_BACK = 1;
        int TABLET_FORWARD = 2;
    }

    private final Profile mProfile;
    private final Context mContext;
    private final ListPopupWindow mPopup;
    private final NavigationController mNavigationController;
    private NavigationHistory mHistory;
    private final NavigationAdapter mAdapter;
    private final @Type int mType;
    private final int mFaviconSize;
    @Nullable
    private final OnLayoutChangeListener mAnchorViewLayoutChangeListener;

    private DefaultFaviconHelper mDefaultFaviconHelper;

    /**
     * Loads the favicons asynchronously.
     */
    private FaviconHelper mFaviconHelper;
    private Runnable mOnDismissCallback;

    private boolean mInitialized;

    /**
     * Constructs a new popup with the given history information.
     *
     * @param profile The profile used for fetching favicons.
     * @param context The context used for building the popup.
     * @param navigationController The controller which takes care of page navigations.
     * @param type The type of navigation popup being triggered.
     */
    public NavigationPopup(Profile profile, Context context,
            NavigationController navigationController, @Type int type) {
        mProfile = profile;
        mContext = context;
        Resources resources = mContext.getResources();
        mNavigationController = navigationController;
        mType = type;

        boolean isForward = type == Type.TABLET_FORWARD;
        boolean anchorToBottom = type == Type.ANDROID_SYSTEM_BACK;

        mHistory = mNavigationController.getDirectedNavigationHistory(
                isForward, MAXIMUM_HISTORY_ITEMS);
        mHistory.addEntry(new NavigationEntry(FULL_HISTORY_ENTRY_INDEX, UrlConstants.HISTORY_URL,
                null, null, null, resources.getString(R.string.show_full_history), null, 0, 0));

        mAdapter = new NavigationAdapter();

        mPopup = new ListPopupWindow(context, null, 0, R.style.NavigationPopupDialog);
        mPopup.setOnDismissListener(this::onDismiss);
        mPopup.setBackgroundDrawable(ApiCompatibilityUtils.getDrawable(resources,
                anchorToBottom ? R.drawable.popup_bg_bottom_tinted : R.drawable.popup_bg_tinted));
        mPopup.setModal(true);
        mPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
        mPopup.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        mPopup.setOnItemClickListener(this);
        mPopup.setAdapter(mAdapter);
        mPopup.setWidth(resources.getDimensionPixelSize(
                anchorToBottom ? R.dimen.navigation_popup_width : R.dimen.menu_width));

        if (anchorToBottom) {
            // By default ListPopupWindow uses the top & bottom padding of the background to
            // determine the vertical offset applied to the window.  This causes the popup to be
            // shifted up by the top padding, and thus we forcibly need to specify a vertical offset
            // of 0 to prevent that.
            mPopup.setVerticalOffset(0);
            mAnchorViewLayoutChangeListener = new OnLayoutChangeListener() {
                @Override
                public void onLayoutChange(View v, int left, int top, int right, int bottom,
                        int oldLeft, int oldTop, int oldRight, int oldBottom) {
                    centerPopupOverAnchorViewAndShow();
                }
            };
        } else {
            mAnchorViewLayoutChangeListener = null;
        }

        mFaviconSize = resources.getDimensionPixelSize(R.dimen.default_favicon_size);
    }

    @VisibleForTesting
    ListPopupWindow getPopupForTesting() {
        return mPopup;
    }

    private String buildComputedAction(String action) {
        return (mType == Type.TABLET_FORWARD ? "ForwardMenu_" : "BackMenu_") + action;
    }

    /**
     * Shows the popup attached to the specified anchor view.
     */
    public void show(View anchorView) {
        if (!mInitialized) initialize();
        if (!mPopup.isShowing()) RecordUserAction.record(buildComputedAction("Popup"));
        if (mPopup.getAnchorView() != null && mAnchorViewLayoutChangeListener != null) {
            mPopup.getAnchorView().removeOnLayoutChangeListener(mAnchorViewLayoutChangeListener);
        }
        mPopup.setAnchorView(anchorView);
        if (mType == Type.ANDROID_SYSTEM_BACK) {
            anchorView.addOnLayoutChangeListener(mAnchorViewLayoutChangeListener);
            centerPopupOverAnchorViewAndShow();
        } else {
            mPopup.show();
        }
    }

    /**
     * Dismisses the popup.
     */
    public void dismiss() {
        mPopup.dismiss();
    }

    /**
     * Sets the callback to be notified when the popup has been dismissed.
     * @param onDismiss The callback to be notified.
     */
    public void setOnDismissCallback(Runnable onDismiss) {
        mOnDismissCallback = onDismiss;
    }

    private void centerPopupOverAnchorViewAndShow() {
        assert mInitialized;
        int horizontalOffset = (mPopup.getAnchorView().getWidth() - mPopup.getWidth()) / 2;
        if (horizontalOffset > 0) mPopup.setHorizontalOffset(horizontalOffset);
        mPopup.show();
    }

    private void onDismiss() {
        if (mInitialized) mFaviconHelper.destroy();
        mInitialized = false;
        if (mDefaultFaviconHelper != null) mDefaultFaviconHelper.clearCache();
        if (mAnchorViewLayoutChangeListener != null) {
            mPopup.getAnchorView().removeOnLayoutChangeListener(mAnchorViewLayoutChangeListener);
        }
        if (mOnDismissCallback != null) mOnDismissCallback.run();
    }

    private void initialize() {
        ThreadUtils.assertOnUiThread();
        mInitialized = true;
        mFaviconHelper = new FaviconHelper();

        Set<String> requestedUrls = new HashSet<String>();
        for (int i = 0; i < mHistory.getEntryCount(); i++) {
            NavigationEntry entry = mHistory.getEntryAtIndex(i);
            if (entry.getFavicon() != null) continue;
            final String pageUrl = entry.getUrl();
            if (!requestedUrls.contains(pageUrl)) {
                FaviconImageCallback imageCallback =
                        (bitmap, iconUrl) -> NavigationPopup.this.onFaviconAvailable(pageUrl,
                                bitmap);
                mFaviconHelper.getLocalFaviconImageForURL(
                        mProfile, pageUrl, mFaviconSize, imageCallback);
                requestedUrls.add(pageUrl);
            }
        }
    }

    /**
     * Called when favicon data requested by {@link #initialize()} is retrieved.
     * @param pageUrl the page for which the favicon was retrieved.
     * @param favicon the favicon data.
     */
    private void onFaviconAvailable(String pageUrl, Bitmap favicon) {
        if (favicon == null) {
            if (mDefaultFaviconHelper == null) mDefaultFaviconHelper = new DefaultFaviconHelper();
            favicon = mDefaultFaviconHelper.getDefaultFaviconBitmap(
                    mContext.getResources(), pageUrl, true);
        }
        for (int i = 0; i < mHistory.getEntryCount(); i++) {
            NavigationEntry entry = mHistory.getEntryAtIndex(i);
            if (TextUtils.equals(pageUrl, entry.getUrl())) entry.updateFavicon(favicon);
        }
        mAdapter.notifyDataSetChanged();
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        NavigationEntry entry = (NavigationEntry) parent.getItemAtPosition(position);
        if (entry.getIndex() == FULL_HISTORY_ENTRY_INDEX) {
            RecordUserAction.record(buildComputedAction("ShowFullHistory"));
            assert mContext instanceof ChromeActivity;
            ChromeActivity activity = (ChromeActivity) mContext;
            HistoryManagerUtils.showHistoryManager(activity, activity.getActivityTab());
        } else {
            // 1-based index to keep in line with Desktop implementation.
            RecordUserAction.record(buildComputedAction("HistoryClick" + (position + 1)));
            int index = entry.getIndex();
            RecordHistogram.recordBooleanHistogram(
                    "Navigation.BackForward.NavigatingToEntryMarkedToBeSkipped",
                    mNavigationController.isEntryMarkedToBeSkipped(index));
            mNavigationController.goToNavigationIndex(index);
        }

        mPopup.dismiss();
    }

    private class NavigationAdapter extends BaseAdapter {
        private Integer mTopPadding;

        @Override
        public int getCount() {
            return mHistory.getEntryCount();
        }

        @Override
        public Object getItem(int position) {
            return mHistory.getEntryAtIndex(position);
        }

        @Override
        public long getItemId(int position) {
            return ((NavigationEntry) getItem(position)).getIndex();
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            EntryViewHolder viewHolder;
            if (convertView == null) {
                LayoutInflater inflater = LayoutInflater.from(parent.getContext());
                convertView = inflater.inflate(R.layout.navigation_popup_item, parent, false);
                viewHolder = new EntryViewHolder();
                viewHolder.mContainer = convertView;
                viewHolder.mImageView = convertView.findViewById(R.id.favicon_img);
                viewHolder.mTextView = convertView.findViewById(R.id.entry_title);
                convertView.setTag(viewHolder);
            } else {
                viewHolder = (EntryViewHolder) convertView.getTag();
            }

            NavigationEntry entry = (NavigationEntry) getItem(position);
            setViewText(entry, viewHolder.mTextView);
            viewHolder.mImageView.setImageBitmap(entry.getFavicon());

            if (entry.getIndex() == FULL_HISTORY_ENTRY_INDEX) {
                ApiCompatibilityUtils.setImageTintList(viewHolder.mImageView,
                        AppCompatResources.getColorStateList(
                                mContext, R.color.default_icon_color_blue));
            } else {
                ApiCompatibilityUtils.setImageTintList(viewHolder.mImageView, null);
            }

            if (mType == Type.ANDROID_SYSTEM_BACK) {
                View container = viewHolder.mContainer;
                if (mTopPadding == null) {
                    mTopPadding = container.getResources().getDimensionPixelSize(
                            R.dimen.navigation_popup_top_padding);
                }
                viewHolder.mContainer.setPadding(container.getPaddingLeft(),
                        position == 0 ? mTopPadding : 0, container.getPaddingRight(),
                        container.getPaddingBottom());
            }

            return convertView;
        }

        private void setViewText(NavigationEntry entry, TextView view) {
            String entryText = entry.getTitle();
            if (TextUtils.isEmpty(entryText)) entryText = entry.getVirtualUrl();
            if (TextUtils.isEmpty(entryText)) entryText = entry.getUrl();

            view.setText(entryText);
        }
    }

    private static class EntryViewHolder {
        View mContainer;
        ImageView mImageView;
        TextView mTextView;
    }


}
