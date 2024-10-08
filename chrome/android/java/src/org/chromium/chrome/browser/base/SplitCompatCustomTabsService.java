// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;

import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.EngagementSignalsCallback;
import androidx.browser.customtabs.ExperimentalPrefetch;
import androidx.browser.customtabs.PrefetchOptions;

import org.chromium.base.BundleUtils;
import org.chromium.base.metrics.RecordHistogram;

import java.util.List;

/**
 * CustomTabsService base class which will call through to the given {@link Impl}. This class must
 * be present in the base module, while the Impl can be in the chrome module.
 */
public class SplitCompatCustomTabsService extends CustomTabsService {
    private String mServiceClassName;
    private Impl mImpl;

    public SplitCompatCustomTabsService(String serviceClassName) {
        mServiceClassName = serviceClassName;
    }

    @Override
    protected void attachBaseContext(Context context) {
        context = SplitCompatApplication.createChromeContext(context);
        mImpl = (Impl) BundleUtils.newInstance(context, mServiceClassName);
        mImpl.setService(this);
        super.attachBaseContext(context);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        mImpl.onCreate();
    }

    @Override
    public IBinder onBind(Intent intent) {
        mImpl.onBind(intent);
        return super.onBind(intent);
    }

    @Override
    public boolean onUnbind(Intent intent) {
        return mImpl.onUnbind(intent);
    }

    @Override
    protected boolean warmup(long flags) {
        return mImpl.warmup(flags);
    }

    @Override
    protected boolean newSession(CustomTabsSessionToken sessionToken) {
        return mImpl.newSession(sessionToken);
    }

    @Override
    protected boolean mayLaunchUrl(
            CustomTabsSessionToken sessionToken,
            Uri url,
            Bundle extras,
            List<Bundle> otherLikelyBundles) {
        return mImpl.mayLaunchUrl(sessionToken, url, extras, otherLikelyBundles);
    }

    @Override
    @ExperimentalPrefetch
    protected void prefetch(CustomTabsSessionToken sessionToken, Uri url, PrefetchOptions options) {
        mImpl.prefetch(sessionToken, List.of(url), options);
    }

    @Override
    @ExperimentalPrefetch
    protected void prefetch(
            CustomTabsSessionToken sessionToken, List<Uri> urls, PrefetchOptions options) {
        mImpl.prefetch(sessionToken, urls, options);
    }

    @Override
    protected Bundle extraCommand(String commandName, Bundle args) {
        return mImpl.extraCommand(commandName, args);
    }

    @Override
    protected boolean updateVisuals(CustomTabsSessionToken sessionToken, Bundle bundle) {
        return mImpl.updateVisuals(sessionToken, bundle);
    }

    @Override
    protected boolean requestPostMessageChannel(
            CustomTabsSessionToken sessionToken, Uri postMessageOrigin) {
        RecordHistogram.recordBooleanHistogram(
                "CustomTabs.PostMessage.RequestPostMessageChannelWithTargetOrigin", false);
        return mImpl.requestPostMessageChannel(sessionToken, postMessageOrigin, null);
    }

    @Override
    protected boolean requestPostMessageChannel(
            CustomTabsSessionToken sessionToken,
            Uri postMessageSourceOrigin,
            Uri postMessageTargetOrigin,
            Bundle extras) {
        RecordHistogram.recordBooleanHistogram(
                "CustomTabs.PostMessage.RequestPostMessageChannelWithTargetOrigin", true);
        return mImpl.requestPostMessageChannel(
                sessionToken, postMessageSourceOrigin, postMessageTargetOrigin);
    }

    @Override
    protected int postMessage(CustomTabsSessionToken sessionToken, String message, Bundle extras) {
        return mImpl.postMessage(sessionToken, message, extras);
    }

    @Override
    protected boolean validateRelationship(
            CustomTabsSessionToken sessionToken, int relation, Uri originAsUri, Bundle extras) {
        return mImpl.validateRelationship(sessionToken, relation, originAsUri, extras);
    }

    @Override
    protected boolean cleanUpSession(CustomTabsSessionToken sessionToken) {
        mImpl.cleanUpSession(sessionToken);
        return super.cleanUpSession(sessionToken);
    }

    @Override
    protected boolean receiveFile(
            CustomTabsSessionToken sessionToken, Uri uri, int purpose, Bundle extras) {
        return mImpl.receiveFile(sessionToken, uri, purpose, extras);
    }

    @Override
    protected boolean isEngagementSignalsApiAvailable(
            CustomTabsSessionToken sessionToken, Bundle extras) {
        return mImpl.isEngagementSignalsApiAvailable(sessionToken, extras);
    }

    @Override
    protected boolean setEngagementSignalsCallback(
            CustomTabsSessionToken sessionToken,
            EngagementSignalsCallback callback,
            Bundle extras) {
        return mImpl.setEngagementSignalsCallback(sessionToken, callback, extras);
    }

    /**
     * Holds the implementation of service logic. Will be called by {@link
     * SplitCompatCustomTabsService}.
     */
    public abstract static class Impl {
        private SplitCompatCustomTabsService mService;

        protected final void setService(SplitCompatCustomTabsService service) {
            mService = service;
        }

        protected final SplitCompatCustomTabsService getService() {
            return mService;
        }

        public void onCreate() {}

        public void onBind(Intent intent) {}

        public boolean onUnbind(Intent intent) {
            return false;
        }

        protected abstract void cleanUpSession(CustomTabsSessionToken sessionToken);

        protected abstract boolean warmup(long flags);

        protected abstract boolean newSession(CustomTabsSessionToken sessionToken);

        protected abstract boolean mayLaunchUrl(
                CustomTabsSessionToken sessionToken,
                Uri url,
                Bundle extras,
                List<Bundle> otherLikelyBundles);

        @ExperimentalPrefetch
        protected abstract void prefetch(
                CustomTabsSessionToken sessionToken, List<Uri> urls, PrefetchOptions options);

        protected abstract Bundle extraCommand(String commandName, Bundle args);

        protected abstract boolean updateVisuals(
                CustomTabsSessionToken sessionToken, Bundle bundle);

        protected abstract boolean requestPostMessageChannel(
                CustomTabsSessionToken sessionToken,
                Uri postMessageOrigin,
                Uri postMessageTargetOrigin);

        protected abstract int postMessage(
                CustomTabsSessionToken sessionToken, String message, Bundle extras);

        protected abstract boolean validateRelationship(
                CustomTabsSessionToken sessionToken, int relation, Uri originAsUri, Bundle extras);

        protected abstract boolean receiveFile(
                CustomTabsSessionToken sessionToken, Uri uri, int purpose, Bundle extras);

        protected abstract boolean isEngagementSignalsApiAvailable(
                CustomTabsSessionToken sessionToken, Bundle extras);

        protected abstract boolean setEngagementSignalsCallback(
                CustomTabsSessionToken sessionToken,
                EngagementSignalsCallback callback,
                Bundle extras);
    }
}
