// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshot_protection;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;
import android.os.Build;
import android.view.Window;
import android.view.WindowManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.enterprise.util.DataProtectionBridge;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.policy.PolicyServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.components.policy.PolicyMap;
import org.chromium.components.policy.PolicyService;

/**
 * A class providing a complete implementation of using {@link
 * WindowManager.LayoutParams.FLAG_SECURE} to control screenshot protection.
 *
 * <p>For non-managed browsers, this class enables screenshot protection only when the incognito
 * TabModel is active, by observing TabModelSelector. This can be overridden with the {@link
 * IncognitoScreenshot} feature.
 *
 * <p>For enterprise managed browsers, {@link IncognitoScreenshot} is ignored if the page is
 * protected by policy. Certain {@link LayoutState} will also always enable protection. If
 * available, the non-incognito active Tab's {@link DataProtectionNavigationController} determines
 * whether screenshot protection should be enabled based on the Tab's page.
 *
 * <p>In all cases, this class observes updates to {@link PolicyService} on all profiles so that
 * screenshot protection applies when the profile is updated.
 */
@NullMarked
public class ScreenshotProtectionController
        implements PolicyService.Observer, TabModelSelectorObserver {
    private final Activity mActivity;
    private final Window mWindow;
    private final NullableObservableSupplier<Tab> mActivityTabProvider;
    private final TabModelSelector mTabModelSelector;
    private final MonotonicObservableSupplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private final Callback<@Nullable Tab> mCurrentTabObserver = this::onTabChanged;
    private final Callback<LayoutStateProvider> mOnLayoutStateProviderAvailableCallback =
            this::onLayoutStateProviderAvailable;
    private final boolean mIsCustomTab;
    private final IncognitoTabModelObserver mIncognitoObserver =
            new IncognitoTabModelObserver() {
                @Override
                public void wasFirstTabCreated() {
                    Profile profile = mTabModelSelector.getModel(true).getProfile();
                    if (profile != null && !profile.shutdownStarted()) {
                        PolicyServiceFactory.getProfilePolicyService(profile)
                                .addObserver(ScreenshotProtectionController.this);
                    }
                    initialize();
                }

                @Override
                public void didBecomeEmpty() {
                    Profile profile = mTabModelSelector.getModel(true).getProfile();
                    if (profile != null
                            && !profile.shutdownStarted()
                            && ScreenshotProtectionController.this != null) {
                        PolicyServiceFactory.getProfilePolicyService(profile)
                                .removeObserver(ScreenshotProtectionController.this);
                    }
                }
            };
    private final TabObserver mTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onDestroyed(Tab tab) {
                    DataProtectionBridge.clearScreenshotSubscriptionCallback(tab);
                }
            };

    private @Nullable LayoutStateProvider mLayoutStateProvider;
    private @Nullable LayoutStateObserver mLayoutStateObserver;
    private @Nullable Callback<TabModel> mCurrentTabModelObserver;
    private boolean mHasEnterpriseScreenshotRules;
    private boolean mActiveTabBlocked;
    private int mActiveScreenshotCallbackTabId = Tab.INVALID_TAB_ID;

    /**
     * @param activity The {@link Activity} on which the snapshot capability needs to be controlled.
     * @param activityTabProvider The {@link NullableObservableSupplier} of the active tab.
     * @param tabModelSelector The {@link TabModelSelector} to receive onChange events from and
     *     trigger protection updates.
     * @param isCustomTab If false, this class will not observe TabModelSelector or LayoutState.
     * @param layoutStateProviderSupplier Supplier of {@link LayoutStateObserver} to receive layout
     *     change events from and trigger protection updates.
     */
    @VisibleForTesting
    public ScreenshotProtectionController(
            Activity activity,
            NullableObservableSupplier<Tab> activityTabProvider,
            TabModelSelector tabModelSelector,
            boolean isCustomTab,
            MonotonicObservableSupplier<LayoutStateProvider> layoutStateProviderSupplier) {
        mActivity = activity;
        mWindow = activity.getWindow();
        mActivityTabProvider = activityTabProvider;
        mTabModelSelector = tabModelSelector;
        mLayoutStateProviderSupplier = layoutStateProviderSupplier;
        mIsCustomTab = isCustomTab;

        for (TabModel model : tabModelSelector.getModels()) {
            Profile profile = model.getProfile();
            if (profile != null && !profile.shutdownStarted()) {
                PolicyServiceFactory.getProfilePolicyService(profile).addObserver(this);
            }
        }
        // Incognito tabs don't have a profile until tab is opened, and have a different profile
        // each time they are fully opened/closed.
        tabModelSelector.addIncognitoTabModelObserver(mIncognitoObserver);

        initialize();
        mTabModelSelector.addObserver(this);
    }

    /** Returns true iff screenshots are blocked by the controller. Public for testing. */
    public boolean isScreenshotBlocked() {
        TabModel currentModel = mTabModelSelector.getCurrentModel();
        if (currentModel.isIncognito() && !ChromeFeatureList.sIncognitoScreenshot.isEnabled()) {
            return true;
        }
        if (mHasEnterpriseScreenshotRules) {
            if (mLayoutStateProvider != null
                    && (mLayoutStateProvider.isLayoutVisible(LayoutType.HUB)
                            || mLayoutStateProvider.isLayoutVisible(LayoutType.TOOLBAR_SWIPE))) {
                return true;
            }
            return mActiveTabBlocked;
        }
        return false;
    }

    private void maybeClearActiveTabCallback() {
        if (mActiveScreenshotCallbackTabId != Tab.INVALID_TAB_ID) {
            Tab previousTab = mTabModelSelector.getTabById(mActiveScreenshotCallbackTabId);
            if (previousTab != null) {
                DataProtectionBridge.clearScreenshotSubscriptionCallback(previousTab);
                previousTab.removeObserver(mTabObserver);
            }
            mActiveScreenshotCallbackTabId = Tab.INVALID_TAB_ID;
        }
        mActiveTabBlocked = false;
    }

    private void resetStateAndObservers() {
        maybeClearActiveTabCallback();
        mHasEnterpriseScreenshotRules = false;
        mActivityTabProvider.removeObserver(mCurrentTabObserver);

        mLayoutStateProviderSupplier.removeObserver(mOnLayoutStateProviderAvailableCallback);
        if (mLayoutStateProvider != null && mLayoutStateObserver != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
            mLayoutStateObserver = null;
        }

        if (mCurrentTabModelObserver != null) {
            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
        }
    }

    private void initialize() {
        resetStateAndObservers();

        for (TabModel model : mTabModelSelector.getModels()) {
            Profile profile = model.getProfile();
            if (profile != null && !profile.shutdownStarted()) {
                mHasEnterpriseScreenshotRules |=
                        DataProtectionBridge.hasBlockingScreenshotRule(profile)
                                || ManagedBrowserUtils.isEnterpriseRealTimeUrlCheckModeEnabled(
                                        profile);
            }
        }

        if (mHasEnterpriseScreenshotRules) {
            mActivityTabProvider.addSyncObserverAndCallIfNonNull(mCurrentTabObserver);
        }

        /**
         * Custom tabs cannot switch between TabModelSelectors or layouts, so they skip observing.
         * Their screenshot protection state is updated once below.
         */
        if (!mIsCustomTab) {
            mCurrentTabModelObserver = tabModel -> updateScreenshotProtectionState();
            mTabModelSelector
                    .getCurrentTabModelSupplier()
                    .addSyncObserver(mCurrentTabModelObserver);

            mLayoutStateProviderSupplier.addSyncObserverAndCallIfNonNull(
                    mOnLayoutStateProviderAvailableCallback);
        }

        updateScreenshotProtectionState();
    }

    private void onLayoutStateProviderAvailable(LayoutStateProvider layoutStateProvider) {
        assertNonNull(layoutStateProvider);
        mLayoutStateProvider = layoutStateProvider;
        mLayoutStateObserver =
                new LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(int layoutType) {
                        updateScreenshotProtectionState();
                    }

                    @Override
                    public void onFinishedHiding(int layoutType) {
                        updateScreenshotProtectionState();
                    }
                };
        mLayoutStateProvider.addObserver(mLayoutStateObserver);
        mLayoutStateProviderSupplier.removeObserver(mOnLayoutStateProviderAvailableCallback);
    }

    private void onTabChanged(@Nullable Tab tab) {
        maybeClearActiveTabCallback();
        if (tab != null && tab == mTabModelSelector.getCurrentTab()) {
            mActiveTabBlocked = !DataProtectionBridge.isScreenshotAllowed(tab);
            mActiveScreenshotCallbackTabId = tab.getId();
            DataProtectionBridge.registerScreenshotSubscriptionCallback(
                    tab, this::setCurrentTabState);
            tab.addObserver(mTabObserver);
        }
        updateScreenshotProtectionState();
    }

    private void setCurrentTabState(Boolean allowed) {
        mActiveTabBlocked = !allowed;
        updateScreenshotProtectionState();
    }

    /** Sets the attributes flags to secure if screenshots should be blocked */
    private void updateScreenshotProtectionState() {
        WindowManager.LayoutParams attributes = mWindow.getAttributes();
        boolean currentSecureState =
                (attributes.flags & WindowManager.LayoutParams.FLAG_SECURE)
                        == WindowManager.LayoutParams.FLAG_SECURE;

        boolean expectedSecureState = isScreenshotBlocked();

        if (currentSecureState == expectedSecureState) return;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            mActivity.setRecentsScreenshotEnabled(!expectedSecureState);
        }
        if (expectedSecureState) {
            mWindow.addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        } else {
            mWindow.clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
        }
    }

    // PolicyService.Observer override
    @Override
    public void onPolicyUpdated(PolicyMap previous, PolicyMap current) {
        // Post a task to sequence Initialize() calls and avoid re-entry.
        PostTask.postTask(TaskTraits.UI_BEST_EFFORT, this::initialize);
    }

    // TabModelSelectorObserver override
    @Override
    public void onDestroyed() {
        resetStateAndObservers();

        for (TabModel model : mTabModelSelector.getModels()) {
            Profile profile = model.getProfile();
            if (profile != null && !profile.shutdownStarted()) {
                PolicyServiceFactory.getProfilePolicyService(profile).removeObserver(this);
            }
        }

        mTabModelSelector.removeIncognitoTabModelObserver(mIncognitoObserver);
        mTabModelSelector.removeObserver(this);
    }
}
