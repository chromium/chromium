// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.init.ProcessInitializationHandler;

import java.util.List;

/**
 * Custom tabs connection service, used by the embedded Chrome activities.
 */
public class CustomTabsConnectionService extends CustomTabsService {
    private CustomTabsConnection mConnection;
    private Intent mBindIntent;

    @Override
    public void onCreate() {
        ProcessInitializationHandler.getInstance().initializePreNative();
        // Kick off the first access to avoid random StrictMode violations in clients.
        RequestThrottler.loadInBackground();
        super.onCreate();
    }

    @Override
    public IBinder onBind(Intent intent) {
        mBindIntent = intent;
        mConnection = CustomTabsConnection.getInstance();
        mConnection.logCall("Service#onBind()", true);
        return super.onBind(intent);
    }

    @Override
    public boolean onUnbind(Intent intent) {
        super.onUnbind(intent);
        if (mConnection != null) mConnection.logCall("Service#onUnbind()", true);
        return false; // No support for onRebind().
    }

    @Override
    protected boolean warmup(long flags) {
        if (!isFirstRunDone()) return false;
        return mConnection.warmup(flags);
    }

    @Override
    protected boolean newSession(CustomTabsSessionToken sessionToken) {
        return mConnection.newSession(sessionToken);
    }

    @Override
    protected boolean mayLaunchUrl(CustomTabsSessionToken sessionToken, Uri url, Bundle extras,
            List<Bundle> otherLikelyBundles) {
        if (!isFirstRunDone()) return false;
        return mConnection.mayLaunchUrl(sessionToken, url, extras, otherLikelyBundles);
    }

    @Override
    protected Bundle extraCommand(String commandName, Bundle args) {
        return mConnection.extraCommand(commandName, args);
    }

    @Override
    protected boolean updateVisuals(CustomTabsSessionToken sessionToken, Bundle bundle) {
        if (!isFirstRunDone()) return false;
        return mConnection.updateVisuals(sessionToken, bundle);
    }

    @Override
    protected boolean requestPostMessageChannel(CustomTabsSessionToken sessionToken,
            Uri postMessageOrigin) {
        Origin origin = Origin.create(postMessageOrigin);
        if (origin == null) return false;
        return mConnection.requestPostMessageChannel(sessionToken, origin);
    }

    @Override
    protected int postMessage(CustomTabsSessionToken sessionToken, String message,
            Bundle extras) {
        if (!isFirstRunDone()) return CustomTabsService.RESULT_FAILURE_DISALLOWED;
        return mConnection.postMessage(sessionToken, message, extras);
    }

    @Override
    protected boolean validateRelationship(
            CustomTabsSessionToken sessionToken, int relation, Uri originAsUri, Bundle extras) {
        Origin origin = Origin.create(originAsUri);
        if (origin == null) return false;
        return mConnection.validateRelationship(sessionToken, relation, origin, extras);
    }

    @Override
    protected boolean cleanUpSession(CustomTabsSessionToken sessionToken) {
        mConnection.cleanUpSession(sessionToken);
        return super.cleanUpSession(sessionToken);
    }


    @Override
    protected boolean receiveFile(@NonNull CustomTabsSessionToken sessionToken, @NonNull Uri uri,
            int purpose, @Nullable Bundle extras) {
        return mConnection.receiveFile(sessionToken, uri, purpose, extras);
    }

    private boolean isFirstRunDone() {
        if (mBindIntent == null) return true;
        boolean firstRunNecessary =
                FirstRunFlowSequencer.checkIfFirstRunIsNecessary(mBindIntent, false);
        if (!firstRunNecessary) {
            mBindIntent = null;
            return true;
        }
        return false;
    }
}
