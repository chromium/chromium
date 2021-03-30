// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.pm.ResolveInfo;
import android.text.TextUtils;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.Consumer;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegateImpl.ShareOrigin;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab.TabWebContentsObserver;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.content.R;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * A class that handles selection action mode for the active {@link Tab}.
 */
public class ChromeActionModeHandler {
    /** Observes the active WebContents being initialized into a Tab. */
    private final Callback<WebContents> mInitWebContentsObserver;

    private final ActivityTabProvider.ActivityTabTabObserver mActivityTabTabObserver;

    private Tab mActiveTab;

    /**
     * @param activityTabProvider {@link ActivityTabProvider} instance.
     * @param actionBarObserver observer called when the contextual action bar's visibility
     *        has changed.
     * @param searchCallback Callback to run when search action is selected in the action mode.
     * @param shareDelegateSupplier The {@link Supplier} of the {@link ShareDelegate} that will be
     *        notified when a share action is performed.
     */
    public ChromeActionModeHandler(ActivityTabProvider activityTabProvider,
            Consumer<Boolean> actionBarObserver, Callback<String> searchCallback,
            Supplier<ShareDelegate> shareDelegateSupplier) {
        mInitWebContentsObserver = (webContents) -> {
            SelectionPopupController.fromWebContents(webContents)
                    .setActionModeCallback(new ActionModeCallback(mActiveTab, webContents,
                            actionBarObserver, searchCallback, shareDelegateSupplier));
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
                        TabWebContentsObserver.from(tab).addInitWebContentsObserver(
                                mInitWebContentsObserver);
                    }
                };
    }

    @VisibleForTesting
    static class ActionModeCallback implements ActionMode.Callback {
        /**
         * Android Intent size limitations prevent sending over a megabyte of data. Limit
         * query lengths to 100kB because other things may be added to the Intent.
         */
        private static final int MAX_SHARE_QUERY_LENGTH_CHARS = 100000;

        private final Tab mTab;
        private final ActionModeCallbackHelper mHelper;
        private final Consumer<Boolean> mActionBarObserver;
        private final Callback<String> mSearchCallback;
        private final Supplier<ShareDelegate> mShareDelegateSupplier;

        // Used for recording UMA histograms.
        private long mContextMenuStartTime;

        ActionModeCallback(Tab tab, WebContents webContents, Consumer<Boolean> observer,
                Callback<String> searchCallback, Supplier<ShareDelegate> shareDelegateSupplier) {
            mTab = tab;
            mHelper = getActionModeCallbackHelper(webContents);
            mActionBarObserver = observer;
            mSearchCallback = searchCallback;
            mShareDelegateSupplier = shareDelegateSupplier;
        }

        @VisibleForTesting
        protected ActionModeCallbackHelper getActionModeCallbackHelper(WebContents webContents) {
            return SelectionPopupController.fromWebContents(webContents)
                    .getActionModeCallbackHelper();
        }

        @Override
        public boolean onCreateActionMode(ActionMode mode, Menu menu) {
            mContextMenuStartTime = System.currentTimeMillis();
            notifyContextualActionBarVisibilityChanged(true);

            int allowedActionModes = ActionModeCallbackHelper.MENU_ITEM_PROCESS_TEXT
                    | ActionModeCallbackHelper.MENU_ITEM_SHARE;
            // Disable options that expose additional Chrome functionality prior to the FRE being
            // completed (i.e. creation of a new tab).
            if (FirstRunStatus.getFirstRunFlowComplete()) {
                allowedActionModes |= ActionModeCallbackHelper.MENU_ITEM_WEB_SEARCH;
            }
            mHelper.setAllowedMenuItems(allowedActionModes);

            mHelper.onCreateActionMode(mode, menu);
            return true;
        }

        @Override
        public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
            recordUserAction();
            notifyContextualActionBarVisibilityChanged(true);
            boolean res = mHelper.onPrepareActionMode(mode, menu);
            Set<String> browsers = getPackageNames(PackageManagerUtils.queryAllWebBrowsersInfo());
            Set<String> launchers = getPackageNames(PackageManagerUtils.queryAllLaunchersInfo());
            for (int i = 0; i < menu.size(); i++) {
                MenuItem item = menu.getItem(i);
                if (item.getGroupId() != R.id.select_action_menu_text_processing_menus
                        || item.getIntent() == null || item.getIntent().getComponent() == null) {
                    continue;
                }
                String packageName = item.getIntent().getComponent().getPackageName();
                // Exclude actions from browsers and system launchers. https://crbug.com/850195
                if (browsers.contains(packageName) || launchers.contains(packageName)) {
                    item.setVisible(false);
                }
            }
            return res;
        }

        @Override
        public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
            if (!mHelper.isActionModeValid()) return true;

            if (item.getItemId() == R.id.select_action_menu_web_search) {
                final String selectedText = mHelper.getSelectedText();
                Callback<Boolean> callback = result -> {
                    if (result != null && result) search(selectedText);
                };
                LocaleManager.getInstance().showSearchEnginePromoIfNeeded(
                        TabUtils.getActivity(mTab), callback);
                mHelper.finishActionMode();
            } else if (mShareDelegateSupplier.get().isSharingHubV15Enabled()
                    && item.getItemId() == R.id.select_action_menu_share) {
                RecordUserAction.record(SelectionPopupController.UMA_MOBILE_ACTION_MODE_SHARE);
                RecordHistogram.recordMediumTimesHistogram("ContextMenu.TimeToSelectShare",
                        System.currentTimeMillis() - mContextMenuStartTime);
                mShareDelegateSupplier.get().share(
                        new ShareParams.Builder(mTab.getWindowAndroid(), /*url=*/"", /*title=*/"")
                                .setText(sanitizeTextForShare(mHelper.getSelectedText()))
                                .build(),
                        new ChromeShareExtras.Builder()
                                .setSaveLastUsed(true)
                                .setIsUserHighlightedText(true)
                                .build(),
                        ShareOrigin.MOBILE_ACTION_MODE);
            } else {
                return mHelper.onActionItemClicked(mode, item);
            }
            return true;
        }

        @Override
        public void onDestroyActionMode(ActionMode mode) {
            mHelper.onDestroyActionMode();
            notifyContextualActionBarVisibilityChanged(false);
        }

        private void notifyContextualActionBarVisibilityChanged(boolean show) {
            if (!mHelper.supportsFloatingActionMode()) mActionBarObserver.accept(show);
        }

        private Set<String> getPackageNames(List<ResolveInfo> list) {
            Set<String> set = new HashSet<>();
            CollectionUtil.forEach(list, (info) -> set.add(info.activityInfo.packageName));
            return set;
        }

        private void search(String searchText) {
            RecordUserAction.record("MobileActionMode.WebSearch");
            mSearchCallback.onResult(searchText);
        }

        private void recordUserAction() {
            if (mHelper.supportsFloatingActionMode()) {
                RecordUserAction.record("MobileActionBarShown.Floating");
            } else {
                RecordUserAction.record("MobileActionBarShown.Toolbar");
            }
        }

        private static String sanitizeTextForShare(String text) {
            if (TextUtils.isEmpty(text) || text.length() < MAX_SHARE_QUERY_LENGTH_CHARS) {
                return text;
            }
            return text.substring(0, MAX_SHARE_QUERY_LENGTH_CHARS) + "…";
        }
    }
}
