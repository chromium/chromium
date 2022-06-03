// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import android.os.Bundle;
import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.directactions.DirectActionReporter.Type;
import org.chromium.chrome.browser.findinpage.FindToolbarManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * Maps the direct action {@code find_in_page} to Chrome's find in page feature.
 */
public class FindInPageDirectActionHandler implements DirectActionHandler {
    private static final String ACTION_ID = ChromeDirectActionIds.FIND_IN_PAGE;
    private static final String ARGUMENT_NAME = "SEARCH_QUERY";

    private final TabModelSelector mTabModelSelector;
    private final FindToolbarManager mFindToolbarManager;

    public FindInPageDirectActionHandler(
            TabModelSelector tabModelSelector, FindToolbarManager findToolbarManager) {
        mTabModelSelector = tabModelSelector;
        mFindToolbarManager = findToolbarManager;
    }

    @Override
    public final void reportAvailableDirectActions(DirectActionReporter reporter) {
        if (isAvailable()) {
            reporter.addDirectAction(ACTION_ID).withParameter(
                    ARGUMENT_NAME, Type.STRING, /* required = */ true);
        }
    }

    @Override
    public final boolean performDirectAction(
            String actionId, Bundle arguments, Callback<Bundle> callback) {
        if (!ACTION_ID.equals(actionId)) return false;

        if (!isAvailable()) return false;

        mFindToolbarManager.showToolbar();
        String findText = arguments.getString(ARGUMENT_NAME, "");
        if (!TextUtils.isEmpty(findText)) {
            mFindToolbarManager.setFindQuery(findText);
        }
        callback.onResult(Bundle.EMPTY);
        return true;
    }

    /** Returns {@code true} if the action is currently available. */
    private final boolean isAvailable() {
        Tab tab = mTabModelSelector.getCurrentTab();
        return tab != null && !tab.isNativePage() && tab.getWebContents() != null;
    }
}
