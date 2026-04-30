// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.tabswitcher;

import android.view.View;

import androidx.annotation.PluralsRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.DeviceInfo;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.data_sharing.ui.versioning.VersionUpdateIphHandler;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabModelDotInfo;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider.IncognitoStateObserver;
import org.chromium.chrome.browser.tabmodel.OverridableTabCount;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.actions.IphIntent;
import org.chromium.chrome.browser.ui.actions.ResourceTextResolver;
import org.chromium.chrome.browser.ui.actions.button.ButtonState;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * Provides the tab switcher action state to the ActionRegistry. Manages the business logic and IPH
 * triggers for the tab switcher button.
 */
@NullMarked
public class TabSwitcherActionProvider implements Destroyable {
    private static final int XR_TAB_COUNT_IPH_TRIGGER = 3;

    private final ActionRegistry mActionRegistry;
    private final TabModelSelector mTabModelSelector;
    private final IncognitoStateProvider mIncognitoStateProvider;
    private final NonNullObservableSupplier<Integer> mOverridableTabCountSupplier;
    private final NonNullObservableSupplier<TabModelDotInfo> mNotificationDotSupplier;
    private final OneshotSupplier<Boolean> mPromoShownOneshotSupplier;
    private final @Nullable NonNullObservableSupplier<Integer> mArchivedTabCountSupplier;
    private final OneshotSupplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private final Runnable mOnTabSwitcherClicked;
    private final Callback<View> mOnTabSwitcherLongClicked;
    private @Nullable Runnable mArchivedTabsIphShownCallback;
    private @Nullable Runnable mArchivedTabsIphDismissedCallback;

    private final Callback<Integer> mTabCountObserver = this::onTabCountChanged;
    private final IncognitoStateObserver mIncognitoStateObserver = this::onIncognitoStateChanged;
    private final Callback<TabModelDotInfo> mNotificationDotObserver =
            this::onNotificationDotChanged;
    private final Callback<Integer> mArchivedTabCountObserver = this::maybeShowDeclutterIph;
    private final CurrentTabObserver mPageLoadObserver;
    private @Nullable LayoutStateProvider mLayoutStateProvider;
    private @Nullable LayoutStateObserver mLayoutStateObserver;

    private final CallbackController mCallbackController = new CallbackController();
    private final PropertyModel mModel;

    private int mTabCount;
    private boolean mIsIncognito;
    private boolean mShowDot;
    private boolean mIsTabStateInitialized;
    private @Nullable Integer mCachedButtonState;

    private boolean mDeclutterIphSignaled;

    public TabSwitcherActionProvider(
            ActionRegistry actionRegistry,
            UserEducationHelper userEducationHelper,
            TabModelSelector tabModelSelector,
            IncognitoStateProvider incognitoStateProvider,
            OverridableTabCount overridableTabCount,
            NonNullObservableSupplier<TabModelDotInfo> notificationDotSupplier,
            OneshotSupplier<Boolean> promoShownOneshotSupplier,
            @Nullable NonNullObservableSupplier<Integer> archivedTabCountSupplier,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            Runnable onTabSwitcherClicked,
            Callback<View> onTabSwitcherLongClicked,
            @Nullable Runnable archivedTabsIphShownCallback,
            @Nullable Runnable archivedTabsIphDismissedCallback) {
        mActionRegistry = actionRegistry;
        mTabModelSelector = tabModelSelector;
        mIncognitoStateProvider = incognitoStateProvider;
        mOverridableTabCountSupplier = overridableTabCount.getObservable();
        mNotificationDotSupplier = notificationDotSupplier;
        mPromoShownOneshotSupplier = promoShownOneshotSupplier;
        mArchivedTabCountSupplier = archivedTabCountSupplier;
        mLayoutStateProviderSupplier = layoutStateProviderSupplier;
        mOnTabSwitcherClicked = onTabSwitcherClicked;
        mOnTabSwitcherLongClicked = onTabSwitcherLongClicked;
        mArchivedTabsIphShownCallback = archivedTabsIphShownCallback;
        mArchivedTabsIphDismissedCallback = archivedTabsIphDismissedCallback;

        mModel = new PropertyModel.Builder(TabSwitcherActionProperties.ALL_KEYS).build();
        mModel.set(ActionProperties.USER_EDUCATION_HELPER, userEducationHelper);
        mModel.set(
                ActionProperties.ON_PRESS_CALLBACK,
                (v) -> {
                    mOnTabSwitcherClicked.run();
                });
        mModel.set(ActionProperties.ON_LONG_PRESS_CALLBACK, mOnTabSwitcherLongClicked);

        mIsIncognito = mIncognitoStateProvider.isIncognitoSelected();

        // Observe tab count.
        mOverridableTabCountSupplier.addSyncObserverAndPostIfNonNull(mTabCountObserver);

        // Observe incognito state.
        mIncognitoStateProvider.addIncognitoStateObserverAndTrigger(mIncognitoStateObserver);

        // Observe notification dot.
        mNotificationDotSupplier.addSyncObserverAndPostIfNonNull(mNotificationDotObserver);

        // Observe archived tabs count.
        if (mArchivedTabCountSupplier != null) {
            mArchivedTabCountSupplier.addSyncObserverAndPostIfNonNull(mArchivedTabCountObserver);
        }

        // Observe layout state.
        mLayoutStateProviderSupplier.onAvailable(
                mCallbackController.makeCancelable(this::setLayoutStateProvider));

        // Tab restoration
        TabModelUtils.runOnTabStateInitialized(
                mTabModelSelector,
                mCallbackController.makeCancelable(
                        (unusedTabModelSelector) -> {
                            mIsTabStateInitialized = true;
                            updateDependentProperties();
                        }));
        if (mTabModelSelector.isTabStateInitialized()) {
            mIsTabStateInitialized = true;
        }

        // Observe page loads for IPH triggers.
        mPageLoadObserver =
                new CurrentTabObserver(
                        mTabModelSelector.getCurrentTabSupplier(),
                        new EmptyTabObserver() {
                            @Override
                            public void onPageLoadFinished(Tab tab, GURL url) {
                                handlePageLoadFinished();
                            }
                        },
                        /* swapCallback= */ null);

        // Register initial state.
        mActionRegistry.register(ActionId.TAB_SWITCHER, mModel);

        updateDependentProperties();
    }

    private void onTabCountChanged(int newCount) {
        mTabCount = newCount;
        mModel.set(TabSwitcherActionProperties.TAB_COUNT, newCount);
        maybeTriggerXrIph(newCount);
        updateDependentProperties();
    }

    private void onIncognitoStateChanged(boolean isIncognito) {
        mIsIncognito = isIncognito;
        updateDependentProperties();
    }

    private void onNotificationDotChanged(TabModelDotInfo dotInfo) {
        boolean showDot = dotInfo.showDot;
        mShowDot = showDot;
        mModel.set(TabSwitcherActionProperties.HAS_NOTIFICATION_DOT, showDot);
        if (showDot) {
            mModel.set(
                    ActionProperties.IPH_INTENT,
                    TabSwitcherActionUtils.getUpdateNotificationIphIntent(dotInfo));
        }
        updateDependentProperties();
    }

    private void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        mLayoutStateProvider = layoutStateProvider;
        mLayoutStateObserver =
                new LayoutStateProvider.LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(int layoutType) {
                        if (layoutType == LayoutType.BROWSING) {
                            mCachedButtonState = mModel.get(ActionProperties.BUTTON_STATE);
                            mModel.set(ActionProperties.BUTTON_STATE, ButtonState.UNCLICKABLE);
                        } else if (layoutType == LayoutType.TAB_SWITCHER) {
                            mModel.set(TabSwitcherActionProperties.SHOW_TAB_SWITCHER_TRIGGER, null);
                        }
                    }

                    @Override
                    public void onStartedHiding(int layoutType) {
                        if (layoutType == LayoutType.BROWSING) {
                            mCachedButtonState = mModel.get(ActionProperties.BUTTON_STATE);
                            mModel.set(ActionProperties.BUTTON_STATE, ButtonState.UNCLICKABLE);
                        }
                    }

                    @Override
                    public void onFinishedShowing(int layoutType) {
                        if (layoutType == LayoutType.BROWSING) {
                            updateDependentProperties();
                        }
                    }

                    @Override
                    public void onFinishedHiding(int layoutType) {
                        if (layoutType == LayoutType.BROWSING) {
                            updateDependentProperties();
                        }
                    }
                };
        mLayoutStateProvider.addObserver(mLayoutStateObserver);
    }

    private void maybeShowDeclutterIph(int tabCount) {
        if (mDeclutterIphSignaled || mIsIncognito || tabCount == 0) return;

        if (mArchivedTabsIphShownCallback != null && mArchivedTabsIphDismissedCallback != null) {
            mDeclutterIphSignaled = true;
            mModel.set(
                    ActionProperties.IPH_INTENT,
                    TabSwitcherActionUtils.getDeclutterIphIntent(
                            mArchivedTabsIphShownCallback, mArchivedTabsIphDismissedCallback));
        }
    }

    @VisibleForTesting
    void handlePageLoadFinished() {
        if (!mIsTabStateInitialized) return;

        Profile profile = mTabModelSelector.getCurrentModel().getProfile();
        if (profile != null) {
            if (VersionUpdateIphHandler.shouldShowVersioningIph(profile, true)) {
                mModel.set(
                        ActionProperties.IPH_INTENT,
                        TabSwitcherActionUtils.getVersioningIphIntent());
            }
        }

        IphIntent iphIntent = null;
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            if (mTabModelSelector.getCurrentModel().isIncognitoBranded()) {
                iphIntent = TabSwitcherActionUtils.getSwitchOutOfIncognitoIphIntent();
            } else if (mTabModelSelector.getModel(true).getCount() > 0) {
                iphIntent = TabSwitcherActionUtils.getSwitchIntoIncognitoIphIntent();
            }
        }

        if (iphIntent == null && !mIsIncognito) {
            Boolean promoShown = mPromoShownOneshotSupplier.get();
            if (promoShown != null && !promoShown) {
                iphIntent = TabSwitcherActionUtils.getTabSwitcherButtonIphIntent();
            }
        }

        if (iphIntent != null) {
            mModel.set(ActionProperties.IPH_INTENT, iphIntent);
        }
    }

    private void maybeTriggerXrIph(int tabCount) {
        if (DeviceInfo.isXr() && tabCount >= XR_TAB_COUNT_IPH_TRIGGER) {
            mModel.set(ActionProperties.IPH_INTENT, TabSwitcherActionUtils.getXrIphIntent());
        }
    }

    private void updateDependentProperties() {
        mModel.set(TabSwitcherActionProperties.IS_INCOGNITO, mIsIncognito);

        @PluralsRes
        int contentDescriptionRes =
                mShowDot && TabSwitcherActionUtils.isDataSharingEnabled()
                        ? R.plurals
                                .accessibility_toolbar_btn_tabswitcher_toggle_default_with_notification
                        : R.plurals.accessibility_toolbar_btn_tabswitcher_toggle_default;

        ResourceTextResolver resolver = new ResourceTextResolver(contentDescriptionRes, mTabCount);

        mModel.set(ActionProperties.CONTENT_DESCRIPTION_RESOLVER, resolver);
        mModel.set(ActionProperties.TOOLTIP_TEXT_RESOLVER, resolver);

        int buttonState =
                mCachedButtonState != null
                        ? mCachedButtonState
                        : mModel.get(ActionProperties.BUTTON_STATE);

        boolean isLayoutAnimating =
                mLayoutStateProvider != null
                        && (mLayoutStateProvider.isLayoutStartingToShow(LayoutType.BROWSING)
                                || mLayoutStateProvider.isLayoutStartingToHide(
                                        LayoutType.BROWSING));

        if (!mIsTabStateInitialized || mTabCount < 1) {
            buttonState = ButtonState.UNCLICKABLE;
        } else if (buttonState == ButtonState.UNCLICKABLE) {
            buttonState = ButtonState.DEFAULT;
        }

        if (isLayoutAnimating) {
            mCachedButtonState = buttonState;
            mModel.set(ActionProperties.BUTTON_STATE, ButtonState.UNCLICKABLE);
        } else {
            mCachedButtonState = null;
            mModel.set(ActionProperties.BUTTON_STATE, buttonState);
        }
    }

    @Override
    public void destroy() {
        mCallbackController.destroy();
        mOverridableTabCountSupplier.removeObserver(mTabCountObserver);
        mIncognitoStateProvider.removeObserver(mIncognitoStateObserver);
        mNotificationDotSupplier.removeObserver(mNotificationDotObserver);
        if (mArchivedTabCountSupplier != null) {
            mArchivedTabCountSupplier.removeObserver(mArchivedTabCountObserver);
        }
        if (mLayoutStateProvider != null && mLayoutStateObserver != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
            mLayoutStateProvider = null;
            mLayoutStateObserver = null;
        }
        mPageLoadObserver.destroy();
        mActionRegistry.unregister(ActionId.TAB_SWITCHER);

        mArchivedTabsIphShownCallback = null;
        mArchivedTabsIphDismissedCallback = null;
    }
}
