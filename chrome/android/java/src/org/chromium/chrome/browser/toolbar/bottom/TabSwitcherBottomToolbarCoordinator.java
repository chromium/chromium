// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.MenuButton;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The coordinator for the tab switcher mode bottom toolbar. This class handles all interactions
 * that the tab switcher bottom toolbar has with the outside world.
 */
public class TabSwitcherBottomToolbarCoordinator {
    /** The mediator that handles events from outside the tab switcher bottom toolbar. */
    private final TabSwitcherBottomToolbarMediator mMediator;

    /** The close all tabs button that lives in the tab switcher bottom bar. */
    private final CloseAllTabsButton mCloseAllTabsButton;

    /** The new tab button that lives in the tab switcher bottom toolbar. */
    private final BottomToolbarNewTabButton mNewTabButton;

    /** The menu button that lives in the tab switcher bottom toolbar. */
    private final MenuButton mMenuButton;

    /**
     * Build the coordinator that manages the tab switcher bottom toolbar.
     * @param stub The tab switcher bottom toolbar {@link ViewStub} to inflate.
     * @param topToolbarRoot The root {@link ViewGroup} of the top toolbar.
     * @param incognitoStateProvider Notifies components when incognito mode is entered or exited.
     * @param themeColorProvider Notifies components when the theme color changes.
     * @param newTabClickListener An {@link OnClickListener} that is triggered when the
     *                            new tab button is clicked.
     * @param closeTabsClickListener An {@link OnClickListener} that is triggered when the
     *                               close all tabs button is clicked.
     * @param menuButtonHelper An {@link AppMenuButtonHelper} that is triggered when the
     *                         menu button is clicked.
     * @param overviewModeBehavior The overview mode manager.
     * @param tabCountProvider Updates the tab count number in the tab switcher button and in the
     *                         incognito toggle tab layout.
     */
    TabSwitcherBottomToolbarCoordinator(ViewStub stub, ViewGroup topToolbarRoot,
            IncognitoStateProvider incognitoStateProvider, ThemeColorProvider themeColorProvider,
            OnClickListener newTabClickListener, OnClickListener closeTabsClickListener,
            AppMenuButtonHelper menuButtonHelper, OverviewModeBehavior overviewModeBehavior,
            TabCountProvider tabCountProvider) {
        final ViewGroup root = (ViewGroup) stub.inflate();

        View toolbar = root.findViewById(R.id.bottom_toolbar_buttons);
        ViewGroup.LayoutParams params = toolbar.getLayoutParams();
        params.height = root.getResources().getDimensionPixelOffset(
                FeatureUtilities.isLabeledBottomToolbarEnabled()
                        ? R.dimen.labeled_bottom_toolbar_height
                        : R.dimen.bottom_toolbar_height);

        TabSwitcherBottomToolbarModel model = new TabSwitcherBottomToolbarModel();

        PropertyModelChangeProcessor.create(model, root,
                new TabSwitcherBottomToolbarViewBinder(
                        topToolbarRoot, (ViewGroup) root.getParent()));

        mMediator = new TabSwitcherBottomToolbarMediator(
                model, themeColorProvider, overviewModeBehavior);

        mCloseAllTabsButton = root.findViewById(R.id.close_all_tabs_button);
        mCloseAllTabsButton.setOnClickListener(closeTabsClickListener);
        mCloseAllTabsButton.setIncognitoStateProvider(incognitoStateProvider);
        mCloseAllTabsButton.setThemeColorProvider(themeColorProvider);
        mCloseAllTabsButton.setTabCountProvider(tabCountProvider);
        mCloseAllTabsButton.setVisibility(View.INVISIBLE);

        mNewTabButton = root.findViewById(R.id.tab_switcher_new_tab_button);
        mNewTabButton.setWrapperView(root.findViewById(R.id.new_tab_button_wrapper));
        mNewTabButton.setOnClickListener(newTabClickListener);
        mNewTabButton.setIncognitoStateProvider(incognitoStateProvider);
        mNewTabButton.setThemeColorProvider(themeColorProvider);

        assert menuButtonHelper != null;
        mMenuButton = root.findViewById(R.id.menu_button_wrapper);
        mMenuButton.setWrapperView(root.findViewById(R.id.labeled_menu_button_wrapper));
        mMenuButton.setThemeColorProvider(themeColorProvider);
        mMenuButton.setAppMenuButtonHelper(menuButtonHelper);
    }

    /**
     * @param showOnTop Whether to show the tab switcher bottom toolbar on the top of the screen.
     */
    void showToolbarOnTop(boolean showOnTop) {
        mMediator.showToolbarOnTop(showOnTop);
    }

    /**
     * Clean up any state when the bottom toolbar is destroyed.
     */
    public void destroy() {
        mMediator.destroy();
        mCloseAllTabsButton.destroy();
        mNewTabButton.destroy();
        mMenuButton.destroy();
    }
}
