// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.EngagementSignalsCallback;
import androidx.browser.customtabs.PrefetchOptions;

import org.chromium.chrome.browser.firstrun.FirstRunFlowSequencer;
import org.chromium.chrome.browser.init.ProcessInitializationHandler;
import org.chromium.components.embedder_support.util.Origin;

import java.util.List;

/** Custom tabs connection service, used by the embedded Chrome activities. */
public class CustomTabsConnectionServiceImpl extends CustomTabsConnectionService.Impl {
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
    public void onBind(Intent intent) {
        mBindIntent = intent;
        mConnection = CustomTabsConnection.getInstance();
        mConnection.logCall("Service#onBind()", true);
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
    protected boolean mayLaunchUrl(
            CustomTabsSessionToken sessionToken,
            Uri url,
            Bundle extras,
            List<Bundle> otherLikelyBundles) {
        if (!isFirstRunDone()) return false;
        return mConnection.mayLaunchUrl(sessionToken, url, extras, otherLikelyBundles);
    }

    @Override
    @androidx.browser.customtabs.ExperimentalPrefetch
    protected void prefetch(
            CustomTabsSessionToken sessionToken, List<Uri> urls, PrefetchOptions options) {
        if (!isFirstRunDone()) return;
        mConnection.prefetch(sessionToken, urls, options);
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
    protected boolean requestPostMessageChannel(
            CustomTabsSessionToken sessionToken,
            Uri postMessageSourceOrigin,
            @Nullable Uri postMessageTargetOrigin) {
        Origin sourceOrigin = Origin.create(postMessageSourceOrigin);
        if (sourceOrigin == null) return false;
        return mConnection.requestPostMessageChannel(
                sessionToken, sourceOrigin, Origin.create(postMessageTargetOrigin));
    }

    @Override
    protected int postMessage(CustomTabsSessionToken sessionToken, String message, Bundle extras) {
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
    protected void cleanUpSession(CustomTabsSessionToken sessionToken) {
        mConnection.cleanUpSession(sessionToken);
    }

    @Override
    protected boolean receiveFile(
            @NonNull CustomTabsSessionToken sessionToken,
            @NonNull Uri uri,
            int purpose,
            @Nullable Bundle extras) {
        return mConnection.receiveFile(sessionToken, uri, purpose, extras);
    }

    @Override
    protected boolean isEngagementSignalsApiAvailable(
            CustomTabsSessionToken sessionToken, Bundle extras) {
        return mConnection.isEngagementSignalsApiAvailable(sessionToken, extras);
    }

    @Override
    protected boolean setEngagementSignalsCallback(
            CustomTabsSessionToken sessionToken,
            EngagementSignalsCallback callback,
            Bundle extras) {
        return mConnection.setEngagementSignalsCallback(sessionToken, callback, extras);
    }

    private boolean isFirstRunDone() {
        if (mBindIntent == null) return true;
        boolean firstRunNecessary = FirstRunFlowSequencer.checkIfFirstRunIsNecessary(false, true);
        if (!firstRunNecessary) {
            mBindIntent = null;
            return true;
        }
        return false;
    }
}
