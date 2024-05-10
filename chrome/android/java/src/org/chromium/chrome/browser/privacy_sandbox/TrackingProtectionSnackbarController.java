// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.content_settings.CookieBlocking3pcdStatus;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.locks.ReentrantLock;

/**
 * Displays the {@link Snackbar} with provided actions for WebApk (aka PWA - Progressive Web Apps).
 *
 * <p>It checks whether the Tracking Protection {@link Snackbar} logic should be executed by looking
 * into {@link ActivityType}. If the provided {@link ActivityType} does not identify the caller as
 * WebApk, the logic won't be executed.
 */
public class TrackingProtectionSnackbarController implements CookieControlsObserver {
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
    private final Runnable mSnakcbarOnAction;
    private final CookieControlsBridge mCookieControlsBridge;
    private final @ActivityType int mActivityType;
    private final ReentrantLock mLock = new ReentrantLock();
    private SnackbarController mSnackbarController =
            new SnackbarController() {
                @Override
                public void onDismissNoAction(Object actionData) {}

                @Override
                public void onAction(Object actionData) {
                    mSnakcbarOnAction.run();
                }
            };
    private boolean mTrackingProtectionControlsVisible;
    private boolean mTrackingProtectionBlocked;
    private int mBlockingStatus3pcd;

    /**
     * Creates the {@link TrackingProtectionSnackbarController} object.
     *
     * @param snackbarOnAction logic to be executed when action button was clicked.
     * @param snackbarManagerSupplier supplier of {@link SnackbarManager} used to generate {@link
     *     Snackbar} object when requested.
     * @param webContents The WebContents instance to observe.
     * @param originalBrowserContext The "original" browser context. In Chrome, this corresponds to
     *     the regular profile when webContents is incognito.
     */
    public TrackingProtectionSnackbarController(
            Runnable snackbarOnAction,
            Supplier<SnackbarManager> snackbarManagerSupplier,
            WebContents webContents,
            @Nullable BrowserContextHandle originalBrowserContext,
            @ActivityType int activityType) {
        mSnakcbarOnAction = snackbarOnAction;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mCookieControlsBridge = new CookieControlsBridge(this, webContents, originalBrowserContext);
        mActivityType = activityType;
    }

    @Override
    public void onHighlightCookieControl(boolean shouldHighlight) {
        if (mBlockingStatus3pcd == CookieBlocking3pcdStatus.NOT_IN3PCD) {
            return;
        }

        if (mTrackingProtectionControlsVisible && !mTrackingProtectionBlocked && shouldHighlight) {
            showSnackbar();
        }
    }

    @Override
    public void onStatusChanged(
            boolean controlsVisible,
            boolean protectionsOn,
            @CookieControlsEnforcement int enforcement,
            @CookieBlocking3pcdStatus int blockingStatus,
            long expiration) {
        mTrackingProtectionControlsVisible = controlsVisible;
        mTrackingProtectionBlocked = protectionsOn;
        mBlockingStatus3pcd = blockingStatus;
    }

    /**
     * Show {@link Snackbar} for TrackingProtection if the provided {@link ActivityType} is correct.
     */
    public void showSnackbar() {
        boolean locked = mLock.tryLock();
        try {
            if (!locked) {
                return;
            }
            if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA)
                    || mActivityType != ActivityType.WEB_APK) {
                return;
            }

            Context context = ContextUtils.getApplicationContext();
            Snackbar snackbar =
                    Snackbar.make(
                            context.getString(
                                    R.string.privacy_sandbox_tracking_protection_snackbar_title),
                            mSnackbarController,
                            Snackbar.TYPE_NOTIFICATION,
                            Snackbar.UMA_SPECIAL_LOCALE);
            snackbar.setDuration(SnackbarManager.DEFAULT_SNACKBAR_DURATION_LONG_MS);
            snackbar.setAction(
                    context.getString(R.string.privacy_sandbox_tracking_protection_snackbar_action),
                    null);
            mSnackbarManagerSupplier.get().showSnackbar(snackbar);
            mCookieControlsBridge.onEntryPointAnimated();
        } finally {
            mLock.unlock();
        }
    }
}
