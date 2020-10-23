// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.view.View;
import android.view.ViewStub;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;

/**
 * The coordinator for the tab switcher mode top toolbar shown on phones, responsible for
 * communication with other UI components and lifecycle. Lazily creates the tab
 * switcher mode top toolbar the first time it's needed.
 */
class TabSwitcherModeTTCoordinatorPhone {
    private final ViewStub mTabSwitcherToolbarStub;

    // TODO(twellington): Create a model to hold all of these properties. Consider using
    // LazyConstructionPropertyMcp to collect all of the properties since it is designed to
    // aggregate properties and bind them to a view the first time it's shown.
    private View.OnClickListener mTabSwitcherListener;
    private View.OnClickListener mNewTabListener;
    private TabCountProvider mTabCountProvider;
    private TabModelSelector mTabModelSelector;
    private IncognitoStateProvider mIncognitoStateProvider;
    private MenuButtonCoordinator mMenuButtonCoordinator;
    private boolean mAccessibilityEnabled;

    private TabSwitcherModeTTPhone mTabSwitcherModeToolbar;

    @Nullable
    private IncognitoTabModelObserver mIncognitoTabModelObserver;

    TabSwitcherModeTTCoordinatorPhone(
            ViewStub tabSwitcherToolbarStub, MenuButtonCoordinator menuButtonCoordinator) {
        mTabSwitcherToolbarStub = tabSwitcherToolbarStub;
        mMenuButtonCoordinator = menuButtonCoordinator;
    }

    /**
     * Cleans up any code and removes observers as necessary.
     */
    void destroy() {
        if (mTabSwitcherModeToolbar != null) {
            mTabSwitcherModeToolbar.destroy();
            mTabSwitcherModeToolbar = null;
        }
        if (mTabModelSelector != null && mIncognitoTabModelObserver != null) {
            mTabModelSelector.removeIncognitoTabModelObserver(mIncognitoTabModelObserver);
        }
        if (mMenuButtonCoordinator != null) {
            mMenuButtonCoordinator.destroy();
            mMenuButtonCoordinator = null;
        }
    }

    /**
     * Called when tab switcher mode is entered or exited.
     * @param inTabSwitcherMode Whether or not tab switcher mode should be shown or hidden.
     */
    void setTabSwitcherMode(boolean inTabSwitcherMode) {
        if (inTabSwitcherMode) {
            if (mTabSwitcherModeToolbar == null) {
                initializeTabSwitcherToolbar();
            }

            mTabSwitcherModeToolbar.setTabSwitcherMode(inTabSwitcherMode);
        } else if (mTabSwitcherModeToolbar != null) {
            mTabSwitcherModeToolbar.setTabSwitcherMode(inTabSwitcherMode);
        }
    }

    /**
     * Sets the OnClickListener that will be notified when the TabSwitcher button is pressed.
     * @param listener The callback that will be notified when the TabSwitcher button is pressed.
     */
    void setOnTabSwitcherClickHandler(View.OnClickListener listener) {
        mTabSwitcherListener = listener;
        if (mTabSwitcherModeToolbar != null) {
            mTabSwitcherModeToolbar.setOnTabSwitcherClickHandler(listener);
        }
    }

    /**
     * Sets the OnClickListener that will be notified when the New Tab button is pressed.
     * @param listener The callback that will be notified when the New Tab button is pressed.
     */
    void setOnNewTabClickHandler(View.OnClickListener listener) {
        mNewTabListener = listener;
        if (mTabSwitcherModeToolbar != null) {
            mTabSwitcherModeToolbar.setOnNewTabClickHandler(listener);
        }
    }

    /**
     * @param tabCountProvider The {@link TabCountProvider} used to observe the number of tabs in
     *                         the current model.
     */
    void setTabCountProvider(TabCountProvider tabCountProvider) {
        mTabCountProvider = tabCountProvider;
        if (mTabSwitcherModeToolbar != null) {
            mTabSwitcherModeToolbar.setTabCountProvider(tabCountProvider);
        }
    }

    /**
     * Sets the current TabModelSelector so the toolbar can pass it into buttons that need access to
     * it.
     */
    void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;
        if (mTabSwitcherModeToolbar != null) {
            mTabSwitcherModeToolbar.setTabModelSelector(selector);
        }

        if (isNewTabVariationEnabled()) {
            mIncognitoTabModelObserver = new IncognitoTabModelObserver() {
                @Override
                public void wasFirstTabCreated() {
                    mTabSwitcherModeToolbar.onIncognitoTabsExistenceChanged(true);
                }

                @Override
                public void didBecomeEmpty() {
                    mTabSwitcherModeToolbar.onIncognitoTabsExistenceChanged(false);
                }
            };
            mTabModelSelector.addIncognitoTabModelObserver(mIncognitoTabModelObserver);
            if (mTabSwitcherModeToolbar != null) {
                boolean doesExist = mTabModelSelector.getModel(true).getCount() != 0;
                mTabSwitcherModeToolbar.onIncognitoTabsExistenceChanged(doesExist);
            }
        }
    }

    /**
     * @param provider The provider used to determine incognito state.
     */
    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mIncognitoStateProvider = provider;
        if (mTabSwitcherModeToolbar != null) {
            mTabSwitcherModeToolbar.setIncognitoStateProvider(provider);
        }
    }

    /** Called when accessibility status changes. */
    void onAccessibilityStatusChanged(boolean enabled) {
        mAccessibilityEnabled = enabled;
        if (mTabSwitcherModeToolbar != null) {
            mTabSwitcherModeToolbar.onAccessibilityStatusChanged(enabled);
        }
    }

    private void initializeTabSwitcherToolbar() {
        mTabSwitcherModeToolbar = (TabSwitcherModeTTPhone) mTabSwitcherToolbarStub.inflate();
        mMenuButtonCoordinator.setMenuButton(
                mTabSwitcherModeToolbar.findViewById(R.id.menu_button_wrapper));

        // It's expected that these properties are set by the time the tab switcher is entered.
        assert mTabSwitcherListener != null;
        mTabSwitcherModeToolbar.setOnTabSwitcherClickHandler(mTabSwitcherListener);

        assert mNewTabListener != null;
        mTabSwitcherModeToolbar.setOnNewTabClickHandler(mNewTabListener);

        assert mTabCountProvider != null;
        mTabSwitcherModeToolbar.setTabCountProvider(mTabCountProvider);

        assert mTabModelSelector != null;
        mTabSwitcherModeToolbar.setTabModelSelector(mTabModelSelector);

        assert mIncognitoStateProvider != null;
        mTabSwitcherModeToolbar.setIncognitoStateProvider(mIncognitoStateProvider);

        if (isNewTabVariationEnabled()) {
            boolean doesExist = mTabModelSelector.getModel(true).getCount() != 0;
            mTabSwitcherModeToolbar.onIncognitoTabsExistenceChanged(doesExist);
        }

        if (mAccessibilityEnabled) {
            mTabSwitcherModeToolbar.onAccessibilityStatusChanged(mAccessibilityEnabled);
        }
    }

    private boolean isNewTabVariationEnabled() {
        return TabUiFeatureUtilities.isGridTabSwitcherEnabled() && ChromeFeatureList.isInitialized()
                && IncognitoUtils.isIncognitoModeEnabled()
                && ChromeFeatureList
                           .getFieldTrialParamByFeature(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                   "tab_grid_layout_android_new_tab")
                           .equals("NewTabVariation");
    }
}
