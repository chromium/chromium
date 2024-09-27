// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview.services;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.paintpreview.browser.NativePaintPreviewServiceProvider;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;

import java.io.File;

/**
 * The Java-side implementations of paint_preview_tab_service.cc. The C++ side owns and controls
 * the lifecycle of the Java implementation.
 * This class provides the required functionalities for capturing the Paint Preview representation
 * of a tab.
 */
@JNINamespace("paint_preview")
public class PaintPreviewTabService implements NativePaintPreviewServiceProvider {
    private static final long AUDIT_START_DELAY_MS = 2 * 60 * 1000; // Two minutes;
    private static boolean sIsAccessibilityEnabledForTesting;

    private Runnable mAuditRunnable;
    private long mNativePaintPreviewBaseService;
    private long mNativePaintPreviewTabService;

    /**
     * Whether the tab qualifies for capture or display of the paint preview.
     * @param tab The tab to check.
     */
    public static boolean tabAllowedForPaintPreview(Tab tab) {
        return !tab.isIncognito()
                && !tab.isNativePage()
                && !tab.isShowingErrorPage()
                && UrlUtilities.isHttpOrHttps(tab.getUrl())
                && !UrlUtilitiesJni.get().isGoogleSearchUrl(tab.getUrl().getSpec());
    }

    private class CaptureTriggerListener extends TabModelSelectorTabObserver
            implements ApplicationStatus.ApplicationStateListener {
        private @ApplicationState int mCurrentApplicationState;

        private CaptureTriggerListener(TabModelSelector tabModelSelector) {
            super(tabModelSelector);
            ApplicationStatus.registerApplicationStateListener(this);
        }

        @Override
        public void onApplicationStateChange(int newState) {
            mCurrentApplicationState = newState;
            if (newState == ApplicationState.HAS_DESTROYED_ACTIVITIES) {
                ApplicationStatus.unregisterApplicationStateListener(this);
            }
        }

        @Override
        public void onHidden(Tab tab, int reason) {
            // Only attempt to capture when all activities are stopped.
            // We don't need to worry about race conditions between #onHidden and
            // #onApplicationStateChange when ChromeActivity is stopped.
            // Activity lifecycle callbacks (that run #onApplicationStateChange) are dispatched in
            // Activity#onStop, so they are executed before the call to #onHidden in
            // ChromeActivity#onStop.
            if (mCurrentApplicationState == ApplicationState.HAS_STOPPED_ACTIVITIES
                    && qualifiesForCapture(tab)) {
                captureTab(
                        tab,
                        success -> {
                            if (!success) {
                                // Treat the tab as if it was closed to cleanup any partial capture
                                // data.
                                tabClosed(tab);
                            }
                        });
            }
        }

        @Override
        public void onTabUnregistered(Tab tab) {
            tabClosed(tab);
        }

        private boolean qualifiesForCapture(Tab tab) {
            // Check the usual parameters and ensure the page is actually alive and loaded.
            return PaintPreviewTabService.tabAllowedForPaintPreview(tab)
                    && tab.getWebContents() != null
                    && !tab.isLoading();
        }
    }

    @CalledByNative
    private PaintPreviewTabService(
            long nativePaintPreviewTabService, long nativePaintPreviewBaseService) {
        mNativePaintPreviewTabService = nativePaintPreviewTabService;
        mNativePaintPreviewBaseService = nativePaintPreviewBaseService;
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativePaintPreviewTabService = 0;
        mNativePaintPreviewBaseService = 0;
    }

    @Override
    public long getNativeBaseService() {
        return mNativePaintPreviewBaseService;
    }

    public boolean hasNativeServiceForTesting() {
        return mNativePaintPreviewTabService != 0;
    }

    /**
     * Returns whether there exists a capture for the tab ID.
     * @param tabId the id for the tab requested.
     * @return Will be true if there is a capture for the tab.
     */
    public boolean hasCaptureForTab(int tabId) {
        if (mNativePaintPreviewTabService == 0) return false;

        if (!isNativeCacheInitialized()) {
            return previewExistsPreNative(getPath(), tabId);
        }

        return PaintPreviewTabServiceJni.get()
                .hasCaptureForTabAndroid(mNativePaintPreviewTabService, tabId);
    }

    /**
     * Should be called when all tabs are restored. Registers a {@link TabModelSelectorTabObserver}
     * for the regular to capture and delete paint previews as needed. Audits restored tabs to
     * remove any failed deletions.
     * @param tabModelSelector the TabModelSelector for the activity.
     * @param runAudit whether to delete tabs not in the tabModelSelector.
     */
    public void onRestoreCompleted(TabModelSelector tabModelSelector, boolean runAudit) {
        new CaptureTriggerListener(tabModelSelector);

        if (!runAudit || mAuditRunnable != null) return;

        // Delay actually performing the audit by a bit to avoid contention with the native task
        // runner that handles IO when showing at startup.
        int id = tabModelSelector.getCurrentTabId();
        int[] ids;
        if (id == Tab.INVALID_TAB_ID || tabModelSelector.isIncognitoSelected()) {
            // Delete all previews.
            ids = new int[0];
        } else {
            // Delete all previews keeping the current tab.
            ids = new int[] {id};
        }
        mAuditRunnable = () -> auditArtifacts(ids);
        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mAuditRunnable.run();
                    mAuditRunnable = null;
                },
                AUDIT_START_DELAY_MS);
    }

    @VisibleForTesting
    public boolean isNativeCacheInitialized() {
        if (mNativePaintPreviewTabService == 0) return false;

        return PaintPreviewTabServiceJni.get()
                .isCacheInitializedAndroid(mNativePaintPreviewTabService);
    }

    private String getPath() {
        if (mNativePaintPreviewTabService == 0) return "";

        return PaintPreviewTabServiceJni.get().getPathAndroid(mNativePaintPreviewTabService);
    }

    @VisibleForTesting
    boolean previewExistsPreNative(String rootPath, int tabId) {
        assert rootPath != null;
        assert !rootPath.isEmpty();

        File zipPath = new File(rootPath, tabId + ".zip");
        return zipPath.exists();
    }

    public void captureTab(Tab tab, Callback<Boolean> successCallback) {
        if (mNativePaintPreviewTabService == 0) {
            successCallback.onResult(false);
            return;
        }

        boolean isAccessibilityEnabled =
                sIsAccessibilityEnabledForTesting
                        || ChromeAccessibilityUtil.get().isAccessibilityEnabled();
        RenderCoordinates coords = RenderCoordinates.fromWebContents(tab.getWebContents());
        PaintPreviewTabServiceJni.get()
                .captureTabAndroid(
                        mNativePaintPreviewTabService,
                        tab.getId(),
                        tab.getWebContents(),
                        isAccessibilityEnabled,
                        coords.getPageScaleFactor(),
                        coords.getScrollXPixInt(),
                        coords.getScrollYPixInt(),
                        successCallback);
    }

    private void tabClosed(Tab tab) {
        if (mNativePaintPreviewTabService == 0) return;

        PaintPreviewTabServiceJni.get()
                .tabClosedAndroid(mNativePaintPreviewTabService, tab.getId());
    }

    @VisibleForTesting
    void auditArtifacts(int[] activeTabIds) {
        if (mNativePaintPreviewTabService == 0) return;

        PaintPreviewTabServiceJni.get()
                .auditArtifactsAndroid(mNativePaintPreviewTabService, activeTabIds);
    }

    public static void setAccessibilityEnabledForTesting(boolean isAccessibilityEnabled) {
        sIsAccessibilityEnabledForTesting = isAccessibilityEnabled;
        ResettersForTesting.register(() -> sIsAccessibilityEnabledForTesting = false);
    }

    @NativeMethods
    interface Natives {
        void captureTabAndroid(
                long nativePaintPreviewTabService,
                int tabId,
                WebContents webContents,
                boolean accessibilityEnabled,
                float pageScaleFactor,
                int scrollOffsetX,
                int scrollOffsetY,
                Callback<Boolean> successCallback);

        void tabClosedAndroid(long nativePaintPreviewTabService, int tabId);

        boolean hasCaptureForTabAndroid(long nativePaintPreviewTabService, int tabId);

        void auditArtifactsAndroid(long nativePaintPreviewTabService, int[] activeTabIds);

        boolean isCacheInitializedAndroid(long nativePaintPreviewTabService);

        String getPathAndroid(long nativePaintPreviewTabService);
    }
}
