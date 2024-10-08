// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.graphics.Rect;
import android.os.Handler;
import android.text.TextUtils;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.selection.ChromeSelectionDropdownMenuDelegate;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab.TabWebContentsObserver;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.content_public.browser.ActionModeCallback;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** A class that handles selection action mode for the active {@link Tab}. */
public class ChromeActionModeHandler {
    /** Observes the active WebContents being initialized into a Tab. */
    private final Callback<WebContents> mInitWebContentsObserver;

    private final ActivityTabProvider.ActivityTabTabObserver mActivityTabTabObserver;

    private Tab mActiveTab;

    /**
     * @param activityTabProvider {@link ActivityTabProvider} instance.
     * @param showWebSearch Whether 'Web Search' option will be shown.
     * @param searchCallback Callback to run when search action is selected in the action mode.
     * @param shareDelegateSupplier The {@link Supplier} of the {@link ShareDelegate} that will be
     *     notified when a share action is performed.
     */
    public ChromeActionModeHandler(
            ActivityTabProvider activityTabProvider,
            Callback<String> searchCallback,
            boolean showWebSearch,
            Supplier<ShareDelegate> shareDelegateSupplier,
            Supplier<ReadAloudController> readAloudControllerSupplier) {
        mInitWebContentsObserver =
                (webContents) -> {
                    SelectionPopupController spc =
                            SelectionPopupController.fromWebContents(webContents);
                    spc.setActionModeCallback(
                            new ChromeActionModeCallback(
                                    mActiveTab,
                                    webContents,
                                    searchCallback,
                                    showWebSearch,
                                    shareDelegateSupplier,
                                    readAloudControllerSupplier));
                    spc.setDropdownMenuDelegate(new ChromeSelectionDropdownMenuDelegate());
                };

        mActivityTabTabObserver =
                new ActivityTabProvider.ActivityTabTabObserver(activityTabProvider) {
                    @Override
                    public void onObservingDifferentTab(Tab tab, boolean hint) {
                        // ActivityTabProvider will null out the tab passed to
                        // onObservingDifferentTab when the tab is non-interactive (e.g. when
                        // entering the TabSwitcher), but in those cases we actually still want to
                        // use the most recently selected tab.
                        if (tab == null || tab == mActiveTab) return;

                        if (mActiveTab != null && mActiveTab.isInitialized()) {
                            TabWebContentsObserver.from(mActiveTab)
                                    .removeInitWebContentsObserver(mInitWebContentsObserver);
                        }
                        mActiveTab = tab;
                        TabWebContentsObserver.from(tab)
                                .addInitWebContentsObserver(mInitWebContentsObserver);
                    }

                    @Override
                    public void onPageLoadStarted(Tab tab, GURL url) {
                        SelectionPopupController.fromWebContents(tab.getWebContents())
                                .clearSelection();
                    }

                    @Override
                    public void onContentChanged(Tab tab) {
                        SelectionPopupController.fromWebContents(tab.getWebContents())
                                .clearSelection();
                    }
                };
    }

    @VisibleForTesting
    static class ChromeActionModeCallback extends ActionModeCallback {
        /**
         * Android Intent size limitations prevent sending over a megabyte of data. Limit
         * query lengths to 100kB because other things may be added to the Intent.
         */
        private static final int MAX_SHARE_QUERY_LENGTH_CHARS = 100000;

        private final Tab mTab;
        private final ActionModeCallbackHelper mHelper;
        private final Callback<String> mSearchCallback;
        private final boolean mShowWebSearch;
        private final Supplier<ShareDelegate> mShareDelegateSupplier;
        private final Supplier<ReadAloudController> mReadAloudControllerSupplier;

        // Used for recording UMA histograms.
        private long mContextMenuStartTime;

        ChromeActionModeCallback(
                Tab tab,
                WebContents webContents,
                Callback<String> searchCallback,
                boolean showWebSearch,
                Supplier<ShareDelegate> shareDelegateSupplier,
                Supplier<ReadAloudController> readAloudControllerSupplier) {
            mTab = tab;
            mHelper = getActionModeCallbackHelper(webContents);
            mShowWebSearch = showWebSearch;
            mSearchCallback = searchCallback;
            mShareDelegateSupplier = shareDelegateSupplier;
            mReadAloudControllerSupplier = readAloudControllerSupplier;
        }

        @VisibleForTesting
        protected ActionModeCallbackHelper getActionModeCallbackHelper(WebContents webContents) {
            return SelectionPopupController.fromWebContents(webContents)
                    .getActionModeCallbackHelper();
        }

        @Override
        public boolean onCreateActionMode(ActionMode mode, Menu menu) {
            mContextMenuStartTime = System.currentTimeMillis();

            int allowedActionModes =
                    ActionModeCallbackHelper.MENU_ITEM_PROCESS_TEXT
                            | ActionModeCallbackHelper.MENU_ITEM_SHARE;
            // Disable options that expose additional Chrome functionality prior to the FRE being
            // completed (i.e. creation of a new tab).
            if (FirstRunStatus.getFirstRunFlowComplete() && mShowWebSearch) {
                allowedActionModes |= ActionModeCallbackHelper.MENU_ITEM_WEB_SEARCH;
            }
            mHelper.setAllowedMenuItems(allowedActionModes);

            mHelper.onCreateActionMode(mode, menu);
            return true;
        }

        @Override
        public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
            recordUserAction();
            boolean res = mHelper.onPrepareActionMode(mode, menu);
            Set<String> browsers = getPackageNames(PackageManagerUtils.queryAllWebBrowsersInfo());
            Set<String> launchers = getPackageNames(PackageManagerUtils.queryAllLaunchersInfo());
            for (int i = 0; i < menu.size(); i++) {
                MenuItem item = menu.getItem(i);
                if (item.getGroupId() != R.id.select_action_menu_text_processing_items
                        || item.getIntent() == null
                        || item.getIntent().getComponent() == null) {
                    continue;
                }
                String packageName = item.getIntent().getComponent().getPackageName();
                // Exclude actions from browsers and system launchers. https://crbug.com/850195
                if (browsers.contains(packageName) || launchers.contains(packageName)) {
                    item.setVisible(false);
                }
            }
            if (menu.findItem(R.id.select_action_menu_share) != null
                    && mode.getType() == ActionMode.TYPE_FLOATING) {
                showShareIph();
            }
            return res;
        }

        private void showShareIph() {
            View view = mTab.getView();
            int padding =
                    view.getResources()
                            .getDimensionPixelSize(R.dimen.iph_shared_highlighting_padding_top);
            Rect anchorRect = new Rect(view.getWidth() / 2, padding, view.getWidth() / 2, padding);
            UserEducationHelper mUserEducationHelper =
                    new UserEducationHelper(
                            TabUtils.getActivity(mTab), mTab.getProfile(), new Handler());
            mUserEducationHelper.requestShowIPH(
                    new IPHCommandBuilder(
                                    view.getResources(),
                                    FeatureConstants.SHARED_HIGHLIGHTING_BUILDER_FEATURE,
                                    R.string.iph_shared_highlighting_builder,
                                    R.string.iph_shared_highlighting_builder)
                            .setAnchorRect(anchorRect)
                            .setAnchorView(view)
                            .setRemoveArrow(true)
                            .build());
        }

        @Override
        public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
            if (!mHelper.isActionModeValid()) return true;

            ReadAloudController readAloud = mReadAloudControllerSupplier.get();
            if (readAloud != null) {
                readAloud.maybePauseForOutgoingIntent(item.getIntent());
            }

            return handleItemClick(item.getItemId()) || mHelper.onActionItemClicked(mode, item);
        }

        @Override
        public boolean onDropdownItemClicked(
                int groupId,
                int id,
                @Nullable Intent intent,
                @Nullable View.OnClickListener clickListener) {
            boolean res =
                    handleItemClick(id)
                            || mHelper.onDropdownItemClicked(groupId, id, intent, clickListener);
            // We will always dismiss the drop-down menu here.
            mHelper.dismissMenu();
            return res;
        }

        private boolean handleItemClick(int id) {
            if (id == R.id.select_action_menu_web_search) {
                final String selectedText = mHelper.getSelectedText();
                Callback<Boolean> callback =
                        result -> {
                            if (result != null && result) search(selectedText);
                        };
                LocaleManager.getInstance()
                        .showSearchEnginePromoIfNeeded(TabUtils.getActivity(mTab), callback);
                mHelper.dismissMenu();
                return true;
            } else if (mShareDelegateSupplier.get() != null
                    && id == R.id.select_action_menu_share) {
                RecordUserAction.record(SelectionPopupController.UMA_MOBILE_ACTION_MODE_SHARE);
                RecordHistogram.recordMediumTimesHistogram(
                        "ContextMenu.TimeToSelectShare",
                        System.currentTimeMillis() - mContextMenuStartTime);
                mShareDelegateSupplier
                        .get()
                        .share(
                                new ShareParams.Builder(
                                                mTab.getWindowAndroid(),
                                                /* url= */ "",
                                                /* title= */ "")
                                        .setText(sanitizeTextForShare(mHelper.getSelectedText()))
                                        .build(),
                                new ChromeShareExtras.Builder()
                                        .setSaveLastUsed(true)
                                        .setRenderFrameHost(mHelper.getRenderFrameHost())
                                        .setDetailedContentType(
                                                ChromeShareExtras.DetailedContentType
                                                        .HIGHLIGHTED_TEXT)
                                        .build(),
                                ShareOrigin.MOBILE_ACTION_MODE);
                return true;
            }
            return false;
        }

        @Override
        public void onDestroyActionMode(ActionMode mode) {
            mHelper.onDestroyActionMode();
        }

        @Override
        public void onGetContentRect(ActionMode mode, View view, Rect outRect) {
            mHelper.onGetContentRect(mode, view, outRect);
        }

        private Set<String> getPackageNames(List<ResolveInfo> list) {
            Set<String> set = new HashSet<>();
            for (var info : list) {
                set.add(info.activityInfo.packageName);
            }
            return set;
        }

        private void search(String searchText) {
            RecordUserAction.record("MobileActionMode.WebSearch");
            mSearchCallback.onResult(searchText);
        }

        private void recordUserAction() {
            RecordUserAction.record("MobileActionBarShown.Floating");
        }

        private static String sanitizeTextForShare(String text) {
            if (TextUtils.isEmpty(text) || text.length() < MAX_SHARE_QUERY_LENGTH_CHARS) {
                return text;
            }
            return text.substring(0, MAX_SHARE_QUERY_LENGTH_CHARS) + "…";
        }
    }
}
