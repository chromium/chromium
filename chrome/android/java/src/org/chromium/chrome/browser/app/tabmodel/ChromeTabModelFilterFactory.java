// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.app.Activity;
import android.content.Context;

import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterFactory;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegateProvider;

import javax.inject.Inject;

/**
 * Glue code that decides which concrete {@link TabModelFilterFactory} should be used.
 */
@ActivityScope
public class ChromeTabModelFilterFactory implements TabModelFilterFactory {
    private Context mContext;

    @Inject
    /**
     * @param context The activity context.
     */
    public ChromeTabModelFilterFactory(Activity activity) {
        mContext = activity;
    }

    /**
     * Return a {@link TabModelFilter} for handling tab groups.
     *
     * @param model The {@link TabModel} that the {@link TabModelFilter} acts on.
     * @return a {@link TabModelFilter}.
     */
    @Override
    public TabModelFilter createTabModelFilter(TabModel model) {
        TabManagementDelegate tabManagementDelegate = TabManagementDelegateProvider.getDelegate();
        return tabManagementDelegate.createTabGroupModelFilter(model);
    }
}
