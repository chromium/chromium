// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf.remoting;

import android.support.v7.media.MediaRouter;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.chrome.browser.media.router.ChromeMediaRouter;
import org.chromium.chrome.browser.media.router.FlingingController;
import org.chromium.chrome.browser.media.router.MediaRouteManager;
import org.chromium.chrome.browser.media.router.MediaRouteProvider;
import org.chromium.chrome.browser.media.router.MediaSource;
import org.chromium.chrome.browser.media.router.caf.BaseSessionController;
import org.chromium.chrome.browser.media.router.caf.CafBaseMediaRouteProvider;

/** A {@link MediaRouteProvider} implementation for remoting, using Cast v3 API. */
public class CafRemotingMediaRouteProvider extends CafBaseMediaRouteProvider {
    private static final String TAG = "RmtMRP";

    // The session controller which is always attached to the current CastSession.
    private final RemotingSessionController mSessionController;

    public static CafRemotingMediaRouteProvider create(MediaRouteManager manager) {
        return new CafRemotingMediaRouteProvider(
                ChromeMediaRouter.getAndroidMediaRouter(), manager);
    }

    @Override
    protected MediaSource getSourceFromId(String sourceId) {
        return RemotingMediaSource.from(sourceId);
    }

    @Override
    public BaseSessionController sessionController() {
        return mSessionController;
    }

    @Override
    public void joinRoute(
            String sourceId, String presentationId, String origin, int tabId, int nativeRequestId) {
        mManager.onRouteRequestError(
                "Remote playback doesn't support joining routes", nativeRequestId);
    }

    @Override
    public void sendStringMessage(String routeId, String message) {
        Log.e(TAG, "Remote playback does not support sending messages");
    }

    private CafRemotingMediaRouteProvider(
            MediaRouter androidMediaRouter, MediaRouteManager manager) {
        super(androidMediaRouter, manager);
        mSessionController = new RemotingSessionController(this);
    }

    @Override
    @Nullable
    public FlingingController getFlingingController(String routeId) {
        if (!sessionController().isConnected()) {
            return null;
        }

        if (!mRoutes.containsKey(routeId)) return null;

        return sessionController().getFlingingController();
    }
}
