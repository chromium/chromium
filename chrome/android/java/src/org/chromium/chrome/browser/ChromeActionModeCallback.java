// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.text.TextUtils;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.content.R;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;

/**
 * A class that handles selection action mode for an associated {@link Tab}.
 */
public class ChromeActionModeCallback implements ActionMode.Callback {
    private final Tab mTab;
    private final ActionModeCallbackHelper mHelper;

    public ChromeActionModeCallback(Tab tab, WebContents webContents) {
        mTab = tab;
        mHelper = getActionModeCallbackHelper(webContents);
    }

    @VisibleForTesting
    protected ActionModeCallbackHelper getActionModeCallbackHelper(WebContents webContents) {
        return SelectionPopupController.fromWebContents(webContents).getActionModeCallbackHelper();
    }

    @Override
    public boolean onCreateActionMode(ActionMode mode, Menu menu) {
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
        notifyContextualActionBarVisibilityChanged(true);
        return mHelper.onPrepareActionMode(mode, menu);
    }

    @Override
    public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        if (!mHelper.isActionModeValid()) return true;

        if (item.getItemId() == R.id.select_action_menu_web_search) {
            final String selectedText = mHelper.getSelectedText();
            Callback<Boolean> callback = result -> {
                if (result != null && result) search(selectedText);
            };
            LocaleManager.getInstance().showSearchEnginePromoIfNeeded(mTab.getActivity(), callback);
            mHelper.finishActionMode();
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
        if (!mHelper.supportsFloatingActionMode()) {
            mTab.notifyContextualActionBarVisibilityChanged(show);
        }
    }

    /**
     * Generate the LoadUrlParams necessary to load the specified search query.
     */
    @VisibleForTesting
    protected LoadUrlParams generateUrlParamsForSearch(String query) {
        String url = TemplateUrlServiceFactory.get().getUrlForSearchQuery(query);
        String headers = GeolocationHeader.getGeoHeader(url, mTab);

        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setVerbatimHeaders(headers);
        loadUrlParams.setTransitionType(PageTransition.GENERATED);
        return loadUrlParams;
    }

    private void search(String searchText) {
        RecordUserAction.record("MobileActionMode.WebSearch");
        TabModelSelector selector = TabModelSelector.from(mTab);
        if (selector == null) return;

        String query = ActionModeCallbackHelper.sanitizeQuery(
                searchText, ActionModeCallbackHelper.MAX_SEARCH_QUERY_LENGTH);
        if (TextUtils.isEmpty(query)) return;

        TrackerFactory.getTrackerForProfile(mTab.getProfile())
                .notifyEvent(EventConstants.WEB_SEARCH_PERFORMED);
        selector.openNewTab(generateUrlParamsForSearch(query),
                TabLaunchType.FROM_LONGPRESS_FOREGROUND, mTab, mTab.isIncognito());
    }
}
