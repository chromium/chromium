// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.ACCESSIBILITY_ENABLED;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.BUTTONS_CLICKABLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.HOMEPAGE_ENABLED_SUPPLIER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.HOMEPAGE_MANAGED_BY_POLICY_SUPPLIER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.HOME_BUTTON_CLICK_HANDLER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.HOME_BUTTON_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_AT_START;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_CLICK_HANDLER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_DESCRIPTION;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IMAGE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_STATE_PROVIDER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_SWITCHER_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_TAB_COUNT_PROVIDER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_TAB_MODEL_SELECTOR;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IN_START_SURFACE_MODE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.LOGO_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_BUTTON_HIGHLIGHT;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_BUTTON_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_CLICK_HANDLER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.TAB_SWITCHER_BUTTON_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.TRANSLATION_Y;

import android.view.View;
import android.view.View.OnClickListener;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.features.start_surface.StartSurfaceState;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** The mediator implements interacts between the views and the caller. */
class StartSurfaceToolbarMediator {
    private final PropertyModel mPropertyModel;
    private final Callback<IPHCommandBuilder> mShowIPHCallback;
    private final boolean mHideIncognitoSwitchWhenNoTabs;
    private final boolean mShouldShowTabSwitcherButtonOnHomepage;
    private final Supplier<ButtonData> mIdentityDiscButtonSupplier;
    private final boolean mIsTabGroupsAndroidContinuationEnabled;

    private TabModelSelector mTabModelSelector;
    private TabCountProvider mTabCountProvider;
    private TemplateUrlServiceObserver mTemplateUrlObserver;
    private TabModelSelectorObserver mTabModelSelectorObserver;
    private LayoutStateProvider mLayoutStateProvider;
    private LayoutStateProvider.LayoutStateObserver mLayoutStateObserver;
    private MenuButtonCoordinator mMenuButtonCoordinator;
    @StartSurfaceState
    private int mOverviewModeState;
    private boolean mIsGoogleSearchEngine;
    private boolean mShouldShowStartSurfaceAsHomepage;
    private boolean mHomepageEnabled;

    private CallbackController mCallbackController = new CallbackController();
    private float mNonIncognitoHomepageTranslationY;

    private boolean mShowHomeButtonOnTabSwitcher;

    StartSurfaceToolbarMediator(PropertyModel model, Callback<IPHCommandBuilder> showIPHCallback,
            boolean hideIncognitoSwitchWhenNoTabs, boolean showHomeButtonOnTabSwitcher,
            MenuButtonCoordinator menuButtonCoordinator,
            ObservableSupplier<Boolean> identityDiscStateSupplier,
            Supplier<ButtonData> identityDiscButtonSupplier,
            ObservableSupplier<Boolean> homepageEnabledSupplier,
            ObservableSupplier<Boolean> startSurfaceAsHomepageSupplier,
            ObservableSupplier<Boolean> homepageManagedByPolicySupplier,
            OnClickListener homeButtonOnClickHandler, boolean shouldShowTabSwitcherButtonOnHomepage,
            boolean isTabGroupsAndroidContinuationEnabled) {
        mPropertyModel = model;
        mOverviewModeState = StartSurfaceState.NOT_SHOWN;
        mShowIPHCallback = showIPHCallback;
        mHideIncognitoSwitchWhenNoTabs = hideIncognitoSwitchWhenNoTabs;
        mMenuButtonCoordinator = menuButtonCoordinator;
        mIdentityDiscButtonSupplier = identityDiscButtonSupplier;
        mIsTabGroupsAndroidContinuationEnabled = isTabGroupsAndroidContinuationEnabled;
        identityDiscStateSupplier.addObserver((canShowHint) -> {
            // If the identity disc wants to be hidden and is hidden, there's nothing we need to do.
            if (!canShowHint && !mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE)) return;
            updateIdentityDisc(mIdentityDiscButtonSupplier.get());
        });

        mShowHomeButtonOnTabSwitcher = showHomeButtonOnTabSwitcher;
        if (mShowHomeButtonOnTabSwitcher) {
            mPropertyModel.set(HOMEPAGE_ENABLED_SUPPLIER, homepageEnabledSupplier);
            homepageEnabledSupplier.addObserver(mCallbackController.makeCancelable((enabled) -> {
                mHomepageEnabled = enabled;
                updateHomeButtonVisibility();
            }));
            mPropertyModel.set(
                    HOMEPAGE_MANAGED_BY_POLICY_SUPPLIER, homepageManagedByPolicySupplier);
            mPropertyModel.set(HOME_BUTTON_CLICK_HANDLER, homeButtonOnClickHandler);
            startSurfaceAsHomepageSupplier.addObserver(
                    mCallbackController.makeCancelable((showStartSurfaceAsHomepage) -> {
                        mShouldShowStartSurfaceAsHomepage = showStartSurfaceAsHomepage;
                        updateHomeButtonVisibility();
                    }));
        }
        mShouldShowTabSwitcherButtonOnHomepage = shouldShowTabSwitcherButtonOnHomepage;
    }

    void onNativeLibraryReady() {
        assert mTemplateUrlObserver == null;

        mTemplateUrlObserver = new TemplateUrlServiceObserver() {
            @Override
            public void onTemplateURLServiceChanged() {
                updateLogoVisibility(TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle());
            }
        };

        TemplateUrlServiceFactory.get().addObserver(mTemplateUrlObserver);
        mIsGoogleSearchEngine = TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle();
        updateLogoVisibility(TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle());
    }

    void destroy() {
        if (mTemplateUrlObserver != null) {
            TemplateUrlServiceFactory.get().removeObserver(mTemplateUrlObserver);
        }
        if (mTabModelSelectorObserver != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        }
        if (mLayoutStateObserver != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
        }
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
    }

    void onStartSurfaceStateChanged(
            @StartSurfaceState int newState, boolean shouldShowStartSurfaceToolbar) {
        mOverviewModeState = newState;
        setStartSurfaceToolbarVisibility(shouldShowStartSurfaceToolbar);
        updateIncognitoToggleTabVisibility();
        updateNewTabButtonVisibility();
        updateHomeButtonVisibility();
        updateLogoVisibility(mIsGoogleSearchEngine);
        updateIdentityDisc(mIdentityDiscButtonSupplier.get());
        updateTranslationY(mNonIncognitoHomepageTranslationY);
        if (mShouldShowTabSwitcherButtonOnHomepage) {
            updateTabSwitcherButtonVisibility();
        }
    }

    void onStartSurfaceHeaderOffsetChanged(int verticalOffset) {
        updateTranslationY(verticalOffset);
    }

    boolean shouldHideToolbarLayout(int toolbarHeight) {
        // If it's on the non-incognito homepage, start surface toolbar is visible (omnibox has no
        // focus), and scrolling offset is smaller than toolbar's height, we need to hide toolbar
        // layout until start surface toolbar is disappearing.
        // When mPropertyModel.get(TRANSLATION_Y) is 0, we cannot hide toolbar layout, otherwise
        // previous toolbar focus may be cleared.
        return mOverviewModeState == StartSurfaceState.SHOWN_HOMEPAGE
                && !mPropertyModel.get(IS_INCOGNITO) && mPropertyModel.get(IS_VISIBLE)
                && -mPropertyModel.get(TRANSLATION_Y) != 0
                && -mPropertyModel.get(TRANSLATION_Y) < toolbarHeight;
    }

    boolean isToolbarOnScreenTop() {
        return (mOverviewModeState == StartSurfaceState.SHOWN_HOMEPAGE
                       || mOverviewModeState == StartSurfaceState.SHOWN_TABSWITCHER)
                && !mPropertyModel.get(IS_INCOGNITO) && mPropertyModel.get(IS_VISIBLE)
                && mPropertyModel.get(TRANSLATION_Y) == 0;
    }

    void setOnNewTabClickHandler(View.OnClickListener listener) {
        mPropertyModel.set(NEW_TAB_CLICK_HANDLER, listener);
    }

    void setTabCountProvider(TabCountProvider tabCountProvider) {
        mTabCountProvider = tabCountProvider;
    }

    void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;

        if (mTabModelSelectorObserver == null) {
            mTabModelSelectorObserver = new TabModelSelectorObserver() {
                @Override
                public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                    mPropertyModel.set(IS_INCOGNITO, mTabModelSelector.isIncognitoSelected());
                    updateIdentityDisc(mIdentityDiscButtonSupplier.get());
                    updateIncognitoToggleTabVisibility();
                }

                @Override
                public void onTabStateInitialized() {
                    maybeInitializeIncognitoToggle();
                }
            };
        }
        if (mTabModelSelector.isTabStateInitialized()) {
            maybeInitializeIncognitoToggle();
        }
        mPropertyModel.set(IS_INCOGNITO, mTabModelSelector.isIncognitoSelected());
        updateIdentityDisc(mIdentityDiscButtonSupplier.get());
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
    }

    private void maybeInitializeIncognitoToggle() {
        if (IncognitoUtils.isIncognitoModeEnabled()) {
            assert mTabCountProvider != null;
            mPropertyModel.set(INCOGNITO_TAB_COUNT_PROVIDER, mTabCountProvider);
            mPropertyModel.set(INCOGNITO_TAB_MODEL_SELECTOR, mTabModelSelector);
        }
    }

    private void updateIncognitoToggleTabVisibility() {
        if (mOverviewModeState == StartSurfaceState.SHOWN_HOMEPAGE) {
            mPropertyModel.set(INCOGNITO_SWITCHER_VISIBLE, false);
            return;
        }

        if (mHideIncognitoSwitchWhenNoTabs) {
            mPropertyModel.set(INCOGNITO_SWITCHER_VISIBLE, hasIncognitoTabs());
        } else {
            mPropertyModel.set(INCOGNITO_SWITCHER_VISIBLE, true);
        }
    }

    // TODO(crbug.com/1042997): share with TabSwitcherModeTTPhone.
    private boolean hasIncognitoTabs() {
        if (mTabModelSelector == null) return false;

        // Check if there is no incognito tab, or all the incognito tabs are being closed.
        TabModel incognitoTabModel = mTabModelSelector.getModel(true);
        for (int i = 0; i < incognitoTabModel.getCount(); i++) {
            if (!incognitoTabModel.getTabAt(i).isClosing()) return true;
        }
        return false;
    }

    void setStartSurfaceMode(boolean inStartSurfaceMode) {
        mPropertyModel.set(IN_START_SURFACE_MODE, inStartSurfaceMode);
    }

    void setStartSurfaceToolbarVisibility(boolean shouldShowStartSurfaceToolbar) {
        mPropertyModel.set(IS_VISIBLE, shouldShowStartSurfaceToolbar);
    }

    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mPropertyModel.set(INCOGNITO_STATE_PROVIDER, provider);
    }

    void onAccessibilityStatusChanged(boolean enabled) {
        mPropertyModel.set(ACCESSIBILITY_ENABLED, enabled);
        updateNewTabButtonVisibility();
    }

    void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        assert layoutStateProvider != null;
        assert mLayoutStateProvider == null : "the mLayoutStateProvider should set at most once.";

        mLayoutStateProvider = layoutStateProvider;
        mLayoutStateObserver = new LayoutStateProvider.LayoutStateObserver() {
            @Override
            public void onStartedShowing(@LayoutType int layoutType, boolean showToolbar) {
                if (layoutType == LayoutType.TAB_SWITCHER) {
                    updateIncognitoToggleTabVisibility();
                }
            }
            @Override
            public void onFinishedShowing(@LayoutType int layoutType) {
                if (layoutType == LayoutType.TAB_SWITCHER) {
                    mPropertyModel.set(BUTTONS_CLICKABLE, true);
                    mMenuButtonCoordinator.setClickable(true);
                }
            }
            @Override
            public void onStartedHiding(
                    @LayoutType int layoutType, boolean showToolbar, boolean delayAnimation) {
                if (layoutType == LayoutType.TAB_SWITCHER) {
                    mPropertyModel.set(BUTTONS_CLICKABLE, false);
                    mMenuButtonCoordinator.setClickable(false);
                }
            }
        };
        mLayoutStateProvider.addObserver(mLayoutStateObserver);
    }

    /**
     * @param highlight If the new tab button should be highlighted.
     */
    void setNewTabButtonHighlight(boolean highlight) {
        mPropertyModel.set(NEW_TAB_BUTTON_HIGHLIGHT, highlight);
    }

    private void updateLogoVisibility(boolean isGoogleSearchEngine) {
        mIsGoogleSearchEngine = isGoogleSearchEngine;
        boolean shouldShowLogo =
                (mOverviewModeState == StartSurfaceState.SHOWN_HOMEPAGE
                        || mOverviewModeState == StartSurfaceState.SHOWN_TABSWITCHER_TASKS_ONLY
                        || mOverviewModeState == StartSurfaceState.SHOWN_TABSWITCHER_OMNIBOX_ONLY
                        || mOverviewModeState == StartSurfaceState.SHOWN_TABSWITCHER_TRENDY_TERMS)
                && mIsGoogleSearchEngine;
        mPropertyModel.set(LOGO_IS_VISIBLE, shouldShowLogo);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void updateIdentityDisc(ButtonData buttonData) {
        boolean shouldShow = buttonData.canShow() && !mTabModelSelector.isIncognitoSelected();
        if (shouldShow) {
            ButtonSpec buttonSpec = buttonData.getButtonSpec();
            mPropertyModel.set(IDENTITY_DISC_CLICK_HANDLER, buttonSpec.getOnClickListener());
            // Take a defensive copy of the Drawable, since Drawables aren't immutable, and another
            // view mutating our drawable could cause it to display incorrectly.
            mPropertyModel.set(
                    IDENTITY_DISC_IMAGE, buttonSpec.getDrawable().getConstantState().newDrawable());
            mPropertyModel.set(IDENTITY_DISC_DESCRIPTION, buttonSpec.getContentDescriptionResId());
            mPropertyModel.set(IDENTITY_DISC_IS_VISIBLE, true);
            mShowIPHCallback.onResult(buttonSpec.getIPHCommandBuilder());
        } else {
            mPropertyModel.set(IDENTITY_DISC_IS_VISIBLE, false);
        }
    }

    private void updateNewTabButtonVisibility() {
        boolean isShownTabswitcherState = mOverviewModeState == StartSurfaceState.SHOWN_TABSWITCHER
                || mOverviewModeState == StartSurfaceState.SHOWN_TABSWITCHER_TASKS_ONLY
                || mOverviewModeState == StartSurfaceState.SHOWN_TABSWITCHER_OMNIBOX_ONLY
                || mOverviewModeState == StartSurfaceState.SHOWN_TABSWITCHER_TRENDY_TERMS;

        // This button is only shown for homepage when accessibility is enabled and
        // OverviewListLayout is shown as the tab switcher instead of the start surface.
        mPropertyModel.set(NEW_TAB_BUTTON_IS_VISIBLE,
                isShownTabswitcherState
                        || (ChromeAccessibilityUtil.get().isAccessibilityEnabled()
                                && !mIsTabGroupsAndroidContinuationEnabled));
    }

    private void updateHomeButtonVisibility() {
        boolean isShownTabswitcherState = mOverviewModeState == StartSurfaceState.SHOWN_TABSWITCHER
                || mOverviewModeState == StartSurfaceState.SHOWN_TABSWITCHER_TASKS_ONLY
                || mOverviewModeState == StartSurfaceState.SHOWN_TABSWITCHER_OMNIBOX_ONLY
                || mOverviewModeState == StartSurfaceState.SHOWN_TABSWITCHER_TRENDY_TERMS;
        // If start surface is not shown as the homepage, home button shouldn't be shown on tab
        // switcher page.
        mPropertyModel.set(HOME_BUTTON_IS_VISIBLE,
                mHomepageEnabled && isShownTabswitcherState && !mPropertyModel.get(IS_INCOGNITO)
                        && mShowHomeButtonOnTabSwitcher && mShouldShowStartSurfaceAsHomepage);
    }

    private void updateTabSwitcherButtonVisibility() {
        // This button should only be shown on homepage. On tab switcher page, new tab button is
        // shown.
        boolean shouldTabSwitcherButton = mOverviewModeState == StartSurfaceState.SHOWN_HOMEPAGE;
        mPropertyModel.set(TAB_SWITCHER_BUTTON_IS_VISIBLE, shouldTabSwitcherButton);
        // If tab switcher button is visible, we should move identity disc to the left.
        mPropertyModel.set(IDENTITY_DISC_AT_START, shouldTabSwitcherButton);
    }

    private void updateTranslationY(float transY) {
        if (mOverviewModeState == StartSurfaceState.SHOWN_HOMEPAGE
                && !mPropertyModel.get(IS_INCOGNITO)) {
            // If it's on the non-incognito homepage, document the homepage translationY.
            mNonIncognitoHomepageTranslationY = transY;
            // Update the translationY of the toolbarView.
            mPropertyModel.set(TRANSLATION_Y, transY);
        } else {
            // If it's not on the non-incognito homepage, set the translationY as 0.
            mPropertyModel.set(TRANSLATION_Y, 0);
        }
    }

    @VisibleForTesting
    @StartSurfaceState
    int getOverviewModeStateForTesting() {
        return mOverviewModeState;
    }

    @VisibleForTesting
    void setShowHomeButtonOnTabSwitcherForTesting(boolean showHomeButtonOnTabSwitcher) {
        mShowHomeButtonOnTabSwitcher = showHomeButtonOnTabSwitcher;
    }
}
