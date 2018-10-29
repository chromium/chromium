// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.AppCompatImageButton;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.tab.Tab;

/**
 * A {@link LinearLayout} that displays a horizontal row of icons for page actions.
 */
public class AppMenuIconRowFooter extends LinearLayout implements View.OnClickListener {
    private ChromeActivity mActivity;
    private AppMenu mAppMenu;

    private AppCompatImageButton mForwardButton;
    private AppCompatImageButton mBookmarkButton;
    private AppCompatImageButton mDownloadButton;
    private AppCompatImageButton mPageInfoButton;
    private AppCompatImageButton mReloadButton;

    public AppMenuIconRowFooter(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mForwardButton = (AppCompatImageButton) findViewById(R.id.forward_menu_id);
        mForwardButton.setOnClickListener(this);

        mBookmarkButton = (AppCompatImageButton) findViewById(R.id.bookmark_this_page_id);
        mBookmarkButton.setOnClickListener(this);

        mDownloadButton = (AppCompatImageButton) findViewById(R.id.offline_page_id);
        mDownloadButton.setOnClickListener(this);

        mPageInfoButton = (AppCompatImageButton) findViewById(R.id.info_menu_id);
        mPageInfoButton.setOnClickListener(this);

        mReloadButton = (AppCompatImageButton) findViewById(R.id.reload_menu_id);
        mReloadButton.setOnClickListener(this);

        // ImageView tinting doesn't work with LevelListDrawable, use Drawable tinting instead.
        // See https://crbug.com/891593 for details.
        Drawable icon = AppCompatResources.getDrawable(getContext(), R.drawable.btn_reload_stop);
        DrawableCompat.setTintList(
                icon, AppCompatResources.getColorStateList(getContext(), R.color.dark_mode_tint));
        mReloadButton.setImageDrawable(icon);
    }

    /**
     * Initializes the icons, setting enabled state, drawables, and content descriptions.
     * @param activity The {@link ChromeActivity} displaying the menu.
     * @param appMenu The {@link AppMenu} that contains the icon row.
     * @param bookmarkBridge The {@link BookmarkBridge} used to retrieve information about
     *                       bookmarks.
     */
    public void initialize(
            ChromeActivity activity, AppMenu appMenu, BookmarkBridge bookmarkBridge) {
        mActivity = activity;
        mAppMenu = appMenu;
        Tab currentTab = mActivity.getActivityTab();

        mForwardButton.setEnabled(currentTab.canGoForward());

        updateBookmarkMenuItem(bookmarkBridge, currentTab);

        mDownloadButton.setEnabled(DownloadUtils.isAllowedToDownloadPage(currentTab));

        loadingStateChanged(currentTab.isLoading());
    }

    @Override
    public void onClick(View v) {
        mActivity.onMenuOrKeyboardAction(v.getId(), true);
        mAppMenu.dismiss();
    }

    /**
     * Called when the current tab's load state  has changed.
     * @param isLoading Whether the tab is currently loading.
     */
    public void loadingStateChanged(boolean isLoading) {
        mReloadButton.getDrawable().setLevel(isLoading
                        ? getResources().getInteger(R.integer.reload_button_level_stop)
                        : getResources().getInteger(R.integer.reload_button_level_reload));
        mReloadButton.setContentDescription(isLoading
                        ? mActivity.getString(R.string.accessibility_btn_stop_loading)
                        : mActivity.getString(R.string.accessibility_btn_refresh));
    }

    private void updateBookmarkMenuItem(BookmarkBridge bookmarkBridge, Tab currentTab) {
        mBookmarkButton.setEnabled(bookmarkBridge.isEditBookmarksEnabled());

        if (currentTab.getBookmarkId() != Tab.INVALID_BOOKMARK_ID) {
            mBookmarkButton.setImageResource(R.drawable.btn_star_filled);
            mBookmarkButton.setContentDescription(mActivity.getString(R.string.edit_bookmark));
            ApiCompatibilityUtils.setImageTintList(mBookmarkButton,
                    AppCompatResources.getColorStateList(getContext(), R.color.blue_mode_tint));
        } else {
            mBookmarkButton.setImageResource(R.drawable.btn_star);
            mBookmarkButton.setContentDescription(
                    mActivity.getString(R.string.accessibility_menu_bookmark));
        }
    }
}
