// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import android.support.v7.app.MediaRouteChooserDialogFragment;
import android.support.v7.app.MediaRouteControllerDialogFragment;
import android.support.v7.media.MediaRouteSelector;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.media.router.caf.CastMediaSource;
import org.chromium.chrome.browser.media.router.caf.remoting.RemotingMediaSource;

/**
 * Implements the JNI interface called from the C++ Media Router dialog controller implementation
 * on Android.
 */
@JNINamespace("media_router")
public class ChromeMediaRouterDialogController implements MediaRouteDialogDelegate {
    private static final String MEDIA_ROUTE_CONTROLLER_DIALOG_FRAGMENT =
            "android.support.v7.mediarouter:MediaRouteControllerDialogFragment";

    private final long mNativeDialogController;
    private MediaRouteDialogManager mDialogManager;

    /**
     * Returns a new initialized {@link ChromeMediaRouterDialogController}.
     * @param nativeDialogController the handle of the native object.
     * @return a new dialog controller to use from the native side.
     */
    @CalledByNative
    public static ChromeMediaRouterDialogController create(long nativeDialogController) {
        return new ChromeMediaRouterDialogController(nativeDialogController);
    }

    /**
     * Shows the {@link MediaRouteChooserDialogFragment} if it's not shown yet.
     * @param sourceUrns the URNs identifying the media sources to filter the devices with.
     */
    @CalledByNative
    public void openRouteChooserDialog(String[] sourceUrns) {
        if (isShowingDialog()) return;

        MediaSource source = null;
        for (String sourceUrn : sourceUrns) {
            source = CastMediaSource.from(sourceUrn);
            if (source == null) source = RemotingMediaSource.from(sourceUrn);

            if (source != null) break;
        }

        MediaRouteSelector routeSelector = source == null ? null : source.buildRouteSelector();

        if (routeSelector == null) {
            ChromeMediaRouterDialogControllerJni.get().onMediaSourceNotSupported(
                    mNativeDialogController, ChromeMediaRouterDialogController.this);
            return;
        }

        mDialogManager =
                new MediaRouteChooserDialogManager(source.getSourceId(), routeSelector, this);
        mDialogManager.openDialog();
    }

    /**
     * Shows the {@link MediaRouteControllerDialogFragment} if it's not shown yet.
     * @param sourceUrn the URN identifying the media source of the current media route.
     * @param mediaRouteId the identifier of the route to be controlled.
     */
    @CalledByNative
    public void openRouteControllerDialog(String sourceUrn, String mediaRouteId) {
        if (isShowingDialog()) return;

        MediaSource source = CastMediaSource.from(sourceUrn);
        if (source == null) source = RemotingMediaSource.from(sourceUrn);

        MediaRouteSelector routeSelector = source == null ? null : source.buildRouteSelector();

        if (routeSelector == null) {
            ChromeMediaRouterDialogControllerJni.get().onMediaSourceNotSupported(
                    mNativeDialogController, ChromeMediaRouterDialogController.this);
            return;
        }

        mDialogManager = new MediaRouteControllerDialogManager(
                source.getSourceId(), routeSelector, mediaRouteId, this);
        mDialogManager.openDialog();
    }

    /**
     * Closes the currently open dialog if it's open.
     */
    @CalledByNative
    public void closeDialog() {
        if (!isShowingDialog()) return;

        mDialogManager.closeDialog();
        mDialogManager = null;
    }

    /**
     * @return if any media route dialog is currently open.
     */
    @CalledByNative
    public boolean isShowingDialog() {
        return mDialogManager != null && mDialogManager.isShowingDialog();
    }

    @Override
    public void onSinkSelected(String sourceUrn, MediaSink sink) {
        mDialogManager = null;
        ChromeMediaRouterDialogControllerJni.get().onSinkSelected(mNativeDialogController,
                ChromeMediaRouterDialogController.this, sourceUrn, sink.getId());
    }

    @Override
    public void onRouteClosed(String mediaRouteId) {
        mDialogManager = null;
        ChromeMediaRouterDialogControllerJni.get().onRouteClosed(
                mNativeDialogController, ChromeMediaRouterDialogController.this, mediaRouteId);
    }

    @Override
    public void onDialogCancelled() {
        // For MediaRouteControllerDialog this method will be called in case the route is closed
        // since it only call onDismiss() and there's no way to distinguish between the two.
        // Here we can figure it out: if mDialogManager is null, onRouteClosed() was called and
        // there's no need to tell the native controller the dialog has been cancelled.
        if (mDialogManager == null) return;

        mDialogManager = null;
        ChromeMediaRouterDialogControllerJni.get().onDialogCancelled(
                mNativeDialogController, ChromeMediaRouterDialogController.this);
    }

    private ChromeMediaRouterDialogController(long nativeDialogController) {
        mNativeDialogController = nativeDialogController;
    }

    @NativeMethods
    interface Natives {
        void onDialogCancelled(long nativeMediaRouterDialogControllerAndroid,
                ChromeMediaRouterDialogController caller);
        void onSinkSelected(long nativeMediaRouterDialogControllerAndroid,
                ChromeMediaRouterDialogController caller, String sourceUrn, String sinkId);
        void onRouteClosed(long nativeMediaRouterDialogControllerAndroid,
                ChromeMediaRouterDialogController caller, String routeId);
        void onMediaSourceNotSupported(long nativeMediaRouterDialogControllerAndroid,
                ChromeMediaRouterDialogController caller);
    }
}
