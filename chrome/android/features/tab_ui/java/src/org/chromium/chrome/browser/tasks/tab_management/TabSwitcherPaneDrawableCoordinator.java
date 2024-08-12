// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable.TabSwitcherDrawableLocation;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the regular tab model {@link TabSwitcherDrawable} used in the {@link
 * TabSwitcherPane}.
 */
public class TabSwitcherPaneDrawableCoordinator {
    private final TabSwitcherDrawable mTabSwitcherDrawable;
    private final TabSwitcherPaneDrawableMediator mMediator;

    /**
     * @param context The activity context.
     * @param tabModelSelector The {@link TabModelSelector} to act on.
     */
    public TabSwitcherPaneDrawableCoordinator(
            @NonNull Context context, @NonNull TabModelSelector tabModelSelector) {
        @BrandedColorScheme int brandedColorScheme = BrandedColorScheme.APP_DEFAULT;
        @TabSwitcherDrawableLocation
        int tabSwitcherDrawableLocation = TabSwitcherDrawableLocation.HUB_TOOLBAR;
        mTabSwitcherDrawable =
                TabSwitcherDrawable.createTabSwitcherDrawable(
                        context, brandedColorScheme, tabSwitcherDrawableLocation);
        PropertyModel model =
                new PropertyModel.Builder(TabSwitcherPaneDrawableProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                model, mTabSwitcherDrawable, TabSwitcherPaneDrawableViewBinder::bind);
        mMediator = new TabSwitcherPaneDrawableMediator(tabModelSelector, model);
    }

    /** Destroys the coordinator. */
    public void destroy() {
        mMediator.destroy();
    }

    /** Returns the {@link TabSwitcherDrawable}. */
    public TabSwitcherDrawable getTabSwitcherDrawable() {
        return mTabSwitcherDrawable;
    }
}
