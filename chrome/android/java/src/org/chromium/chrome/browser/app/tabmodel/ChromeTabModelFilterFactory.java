// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.ACTIVITY_CONTEXT;

import android.content.Context;

import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterFactory;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;

import javax.inject.Inject;
import javax.inject.Named;

/** Glue code that decides which concrete {@link TabModelFilterFactory} should be used. */
@ActivityScope
public class ChromeTabModelFilterFactory implements TabModelFilterFactory {
    private Context mContext;

    /**
     * @param context The activity context.
     */
    @Inject
    public ChromeTabModelFilterFactory(@Named(ACTIVITY_CONTEXT) Context context) {
        mContext = context;
    }

    /**
     * Return a {@link TabModelFilter} for handling tab groups.
     *
     * @param model The {@link TabModel} that the {@link TabModelFilter} acts on.
     * @return a {@link TabModelFilter}.
     */
    @Override
    public TabModelFilter createTabModelFilter(TabModel model) {
        return new TabGroupModelFilter(model);
    }
}
