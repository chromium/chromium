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
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.FilterLayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** A class to provide common functionalities related to allowing/blocking screenshots. */
@NullMarked
public class ScreenshotProtectionController implements DestroyObserver {
    private final Activity mActivity;
    private final Window mWindow;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final TabModelSelector mTabModelSelector;
    private final MonotonicObservableSupplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private final Callback<LayoutStateProvider> mOnLayoutStateProviderAvailableCallback =
            this::onLayoutStateProviderAvailable;
    private @Nullable LayoutStateProvider mLayoutStateProvider;
    private @Nullable LayoutStateObserver mLayoutStateObserver;
    private @Nullable Callback<TabModel> mCurrentTabModelObserver;

    /**
     * @param activity The {@link Activity} on which the snapshot capability needs to be controlled.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} `this` will use to
     *     unregister observers during destruction.
     * @param tabModelSelector The {@link TabModelSelector} to receive onChange events from and
     *     trigger protection updates.
     * @param isCustomTab If false, this class will not observe TabModelSelector or LayoutState.
     * @param layoutStateProviderSupplier Supplier of {@link LayoutStateObserver} to receive layout
     *     change events from and trigger protection updates.
     */
    @VisibleForTesting
    public ScreenshotProtectionController(
            Activity activity,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            TabModelSelector tabModelSelector,
            boolean isCustomTab,
            MonotonicObservableSupplier<LayoutStateProvider> layoutStateProviderSupplier) {
        mActivity = activity;
        mWindow = activity.getWindow();

        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mTabModelSelector = tabModelSelector;
        mLayoutStateProviderSupplier = layoutStateProviderSupplier;
        if (!isCustomTab) {
            // Custom tabs cannot switch between TabModelSelectors or layouts, so they skip
            // observing. Their screenshot protection state is updated once below.
            mCurrentTabModelObserver = tabModel -> updateScreenshotProtectionState();
            mTabModelSelector
                    .getCurrentTabModelSupplier()
                    .addSyncObserver(mCurrentTabModelObserver);

            mLayoutStateProviderSupplier.addSyncObserverAndCallIfNonNull(
                    mOnLayoutStateProviderAvailableCallback);
        }

        updateScreenshotProtectionState();

        mActivityLifecycleDispatcher.register(this);
    }

    public final boolean isScreenshotBlocked() {
        return mTabModelSelector != null && mTabModelSelector.getCurrentModel().isIncognito();
    }

    private void onLayoutStateProviderAvailable(LayoutStateProvider layoutStateProvider) {
        assertNonNull(layoutStateProvider);
        mLayoutStateProvider = layoutStateProvider;
        mLayoutStateObserver =
                new FilterLayoutStateObserver(
                        LayoutType.HUB,
                        new LayoutStateObserver() {
                            @Override
                            public void onStartedShowing(int layoutType) {
                                assert layoutType == LayoutType.HUB;
                                updateScreenshotProtectionState();
                            }

                            @Override
                            public void onStartedHiding(int layoutType) {
                                assert layoutType == LayoutType.HUB;
                                updateScreenshotProtectionState();
                            }
                        });
        mLayoutStateProvider.addObserver(mLayoutStateObserver);
        mLayoutStateProviderSupplier.removeObserver(mOnLayoutStateProviderAvailableCallback);
    }

    /** Sets the attributes flags to secure if screenshots should be blocked */
    protected void updateScreenshotProtectionState() {
        WindowManager.LayoutParams attributes = mWindow.getAttributes();
        boolean currentSecureState =
                (attributes.flags & WindowManager.LayoutParams.FLAG_SECURE)
                        == WindowManager.LayoutParams.FLAG_SECURE;

        boolean expectedSecureState = isScreenshotBlocked();
        if (ChromeFeatureList.sIncognitoScreenshot.isEnabled()) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                mActivity.setRecentsScreenshotEnabled(!expectedSecureState);
            }
            expectedSecureState = false;
        }
        if (currentSecureState == expectedSecureState) return;

        if (expectedSecureState) {
            mWindow.addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        } else {
            mWindow.clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
        }
    }

    // ActivityLifecycleDispatcher override
    @Override
    public void onDestroy() {
        mLayoutStateProviderSupplier.removeObserver(mOnLayoutStateProviderAvailableCallback);
        if (mLayoutStateProvider != null && mLayoutStateObserver != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
        }
        if (mCurrentTabModelObserver != null) {
            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
        }

        mActivityLifecycleDispatcher.unregister(this);
    }
}
