// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CloseButtonVisibilityManager;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;

/**
 * Updates the browser controls state based on whether the browser is in TWA mode, the page's
 * security level, and desktop windowing state.
 */
@NullMarked
public class TrustedWebActivityBrowserControlsVisibilityManager
        implements DesktopWindowStateManager.AppHeaderObserver {
    static final @BrowserControlsState int DEFAULT_BROWSER_CONTROLS_STATE =
            BrowserControlsState.BOTH;

    private final TabObserverRegistrar mTabObserverRegistrar;
    private final CustomTabActivityTabProvider mTabProvider;
    private final CustomTabToolbarCoordinator mToolbarCoordinator;
    private final CloseButtonVisibilityManager mCloseButtonVisibilityManager;
    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final FullscreenManager mFullscreenManager;

    private boolean mInAppMode;
    private final boolean mShowBrowserControlsForChildTab;
    private boolean mIsInDesktopWindow;

    private @BrowserControlsState int mBrowserControlsState = DEFAULT_BROWSER_CONTROLS_STATE;

    private final SettableNonNullObservableSupplier<Boolean> mControlsVisibleSupplier =
            ObservableSuppliers.createNonNull(false);

    private final CustomTabTabObserver mTabObserver =
            new CustomTabTabObserver() {
                @Override
                public void onSSLStateUpdated(Tab tab) {
                    updateBrowserControlsState();
                    updateCloseButtonVisibility();
                }

                @Override
                public void onObservingDifferentTab(@Nullable Tab tab) {
                    updateBrowserControlsState();
                    updateCloseButtonVisibility();
                }
            };

    public TrustedWebActivityBrowserControlsVisibilityManager(
            TabObserverRegistrar tabObserverRegistrar,
            CustomTabActivityTabProvider tabProvider,
            CustomTabToolbarCoordinator toolbarCoordinator,
            CloseButtonVisibilityManager closeButtonVisibilityManager,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            BrowserServicesIntentDataProvider intentDataProvider,
            FullscreenManager fullscreenManager) {
        mTabObserverRegistrar = tabObserverRegistrar;
        mTabProvider = tabProvider;
        mToolbarCoordinator = toolbarCoordinator;
        mCloseButtonVisibilityManager = closeButtonVisibilityManager;
        mDesktopWindowStateManager = desktopWindowStateManager;
        mIntentDataProvider = intentDataProvider;
        mFullscreenManager = fullscreenManager;

        mShowBrowserControlsForChildTab = (mIntentDataProvider.getWebappExtras() != null);
        mIsInDesktopWindow = AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateManager);

        if (mDesktopWindowStateManager != null) {
            mDesktopWindowStateManager.addObserver(this);
        }
    }

    @Override
    public void onDesktopWindowingModeChanged(boolean isInDesktopWindow) {
        if (mIsInDesktopWindow == isInDesktopWindow) return;
        mIsInDesktopWindow = isInDesktopWindow;

        if (!shouldShowWebAppControls()) return;
        updateBrowserControlsState();
        updateCloseButtonVisibility();
    }

    private boolean shouldShowWebAppControls() {
        return mInAppMode && mIntentDataProvider.getResolvedDisplayMode() == DisplayMode.MINIMAL_UI;
    }

    /** Should be called when the browser enters and exits TWA mode. */
    public void updateIsInAppMode(boolean inAppMode) {
        if (mInAppMode == inAppMode) return;

        mInAppMode = inAppMode;

        updateBrowserControlsState();
        updateCloseButtonVisibility();

        if (mInAppMode) {
            mTabObserverRegistrar.registerActivityTabObserver(mTabObserver);
        } else {
            mTabObserverRegistrar.unregisterActivityTabObserver(mTabObserver);
        }
    }

    /** Supplies whether browser controls are visible. */
    public NonNullObservableSupplier<Boolean> getControlsVisibleSupplier() {
        return mControlsVisibleSupplier;
    }

    private void updateBrowserControlsState() {
        @BrowserControlsState
        int newBrowserControlsState = computeBrowserControlsState(mTabProvider.getTab());
        mControlsVisibleSupplier.set(
                mInAppMode && newBrowserControlsState != BrowserControlsState.HIDDEN);
        if (mBrowserControlsState == newBrowserControlsState) return;

        mBrowserControlsState = newBrowserControlsState;
        mToolbarCoordinator.setBrowserControlsState(mBrowserControlsState);

        if (mBrowserControlsState == BrowserControlsState.BOTH) {
            // Force showing the controls for a bit when leaving Trusted Web Activity
            // mode.
            mToolbarCoordinator.showToolbarTemporarily();
        }
    }

    private void updateCloseButtonVisibility() {
        // Show close button if toolbar is not visible, so that during the in and off-scope
        // transitions we avoid button flickering when toolbar is appearing/disappearing.
        boolean closeButtonVisibility =
                shouldShowBrowserControlsAndCloseButton(mTabProvider.getTab())
                        || (mBrowserControlsState == BrowserControlsState.HIDDEN);

        mCloseButtonVisibilityManager.setVisibility(closeButtonVisibility);
    }

    private boolean shouldShowBrowserControlsAndCloseButton(@Nullable Tab tab) {
        return !mInAppMode || (isChildTab(tab) && mShowBrowserControlsForChildTab);
    }

    private @BrowserControlsState int computeBrowserControlsState(@Nullable Tab tab) {
        // Force browser controls to show when the security level is dangerous or warning.
        if (tab != null) {
            int securityLevel = getSecurityLevel(tab);
            if (securityLevel == ConnectionSecurityLevel.DANGEROUS
                    || securityLevel == ConnectionSecurityLevel.WARNING) {
                return BrowserControlsState.SHOWN;
            }
        }

        // Fallback to browser controls in non-desktop windowing mode (e.g. phone or non-windowed
        // tablet) when running a minimal-ui WebAPK or shortcut web app, except on desktop devices
        // (where non-desktop windowing only occurs when fullscreened) or in fullscreen mode.
        if (mIntentDataProvider.isWebappOrWebApkActivity()
                && shouldShowWebAppControls()
                && !mIsInDesktopWindow
                && !DeviceInfo.isDesktop()
                && !mFullscreenManager.getPersistentFullscreenMode()) {
            return BrowserControlsState.BOTH;
        }

        return shouldShowBrowserControlsAndCloseButton(tab)
                ? BrowserControlsState.BOTH
                : BrowserControlsState.HIDDEN;
    }

    private boolean isChildTab(@Nullable Tab tab) {
        return tab != null && tab.getParentId() != Tab.INVALID_TAB_ID;
    }

    /** Clears up current instance. Can't be used after this method is called. */
    public void destroy() {
        if (mDesktopWindowStateManager != null) {
            mDesktopWindowStateManager.removeObserver(this);
        }
    }

    @ConnectionSecurityLevel
    @VisibleForTesting
    int getSecurityLevel(Tab tab) {
        int securityLevel = SecurityStateModel.getSecurityLevelForWebContents(tab.getWebContents());
        return securityLevel;
    }
}
