// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;

import androidx.browser.auth.AuthTabSessionToken;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.EngagementSignalsCallback;
import androidx.browser.customtabs.ExperimentalPrefetch;
import androidx.browser.customtabs.PrefetchOptions;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;

/**
 * CustomTabsService base class which will call through to the given {@link Impl}. This class must
 * be present in the base module, while the Impl can be in the chrome module.
 */
@NullMarked
public class SplitCompatCustomTabsService extends CustomTabsService {
    private final String mServiceClassName;
    private Impl mImpl;

    public SplitCompatCustomTabsService(String serviceClassName) {
        mServiceClassName = serviceClassName;
    }

    @Override
    protected void attachBaseContext(Context baseContext) {
        mImpl =
                (Impl)
                        SplitCompatUtils.loadClassAndAdjustContextChrome(
                                baseContext, mServiceClassName);
        mImpl.setService(this);
        super.attachBaseContext(baseContext);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        mImpl.onCreate();
    }

    @Override
    public IBinder onBind(@Nullable Intent intent) {
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
            @Nullable Uri url,
            @Nullable Bundle extras,
            @Nullable List<Bundle> otherLikelyBundles) {
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
    protected @Nullable Bundle extraCommand(String commandName, @Nullable Bundle args) {
        return mImpl.extraCommand(commandName, args);
    }

    @Override
    protected boolean updateVisuals(CustomTabsSessionToken sessionToken, @Nullable Bundle bundle) {
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
            @Nullable Uri postMessageTargetOrigin,
            Bundle extras) {
        RecordHistogram.recordBooleanHistogram(
                "CustomTabs.PostMessage.RequestPostMessageChannelWithTargetOrigin", true);
        return mImpl.requestPostMessageChannel(
                sessionToken, postMessageSourceOrigin, postMessageTargetOrigin);
    }

    @Override
    protected int postMessage(
            CustomTabsSessionToken sessionToken, String message, @Nullable Bundle extras) {
        return mImpl.postMessage(sessionToken, message, extras);
    }

    @Override
    protected boolean validateRelationship(
            CustomTabsSessionToken sessionToken,
            int relation,
            Uri originAsUri,
            @Nullable Bundle extras) {
        return mImpl.validateRelationship(sessionToken, relation, originAsUri, extras);
    }

    @Override
    protected boolean cleanUpSession(CustomTabsSessionToken sessionToken) {
        mImpl.cleanUpSession(sessionToken);
        return super.cleanUpSession(sessionToken);
    }

    @Override
    protected boolean receiveFile(
            CustomTabsSessionToken sessionToken, Uri uri, int purpose, @Nullable Bundle extras) {
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

    @Override
    protected boolean cleanUpSession(AuthTabSessionToken sessionToken) {
        mImpl.cleanUpSession(sessionToken);
        return super.cleanUpSession(sessionToken);
    }

    @Override
    protected boolean registerAuthTabSession(AuthTabSessionToken sessionToken) {
        return mImpl.newAuthTabSession(sessionToken);
    }

    /**
     * Holds the implementation of service logic. Will be called by {@link
     * SplitCompatCustomTabsService}.
     */
    public abstract static class Impl {
        private @Nullable SplitCompatCustomTabsService mService;

        protected final void setService(SplitCompatCustomTabsService service) {
            mService = service;
        }

        protected final @Nullable SplitCompatCustomTabsService getService() {
            return mService;
        }

        public void onCreate() {}

        public void onBind(@Nullable Intent intent) {}

        public boolean onUnbind(Intent intent) {
            return false;
        }

        protected abstract void cleanUpSession(CustomTabsSessionToken sessionToken);

        protected abstract boolean warmup(long flags);

        protected abstract boolean newSession(CustomTabsSessionToken sessionToken);

        protected abstract boolean mayLaunchUrl(
                CustomTabsSessionToken sessionToken,
                @Nullable Uri url,
                @Nullable Bundle extras,
                @Nullable List<Bundle> otherLikelyBundles);

        @ExperimentalPrefetch
        protected abstract void prefetch(
                CustomTabsSessionToken sessionToken, List<Uri> urls, PrefetchOptions options);

        protected abstract @Nullable Bundle extraCommand(String commandName, @Nullable Bundle args);

        protected abstract boolean updateVisuals(
                CustomTabsSessionToken sessionToken, @Nullable Bundle bundle);

        protected abstract boolean requestPostMessageChannel(
                CustomTabsSessionToken sessionToken,
                Uri postMessageOrigin,
                @Nullable Uri postMessageTargetOrigin);

        protected abstract int postMessage(
                CustomTabsSessionToken sessionToken, String message, @Nullable Bundle extras);

        protected abstract boolean validateRelationship(
                CustomTabsSessionToken sessionToken,
                int relation,
                Uri originAsUri,
                @Nullable Bundle extras);

        protected abstract boolean receiveFile(
                CustomTabsSessionToken sessionToken, Uri uri, int purpose, @Nullable Bundle extras);

        protected abstract boolean isEngagementSignalsApiAvailable(
                CustomTabsSessionToken sessionToken, Bundle extras);

        protected abstract boolean setEngagementSignalsCallback(
                CustomTabsSessionToken sessionToken,
                EngagementSignalsCallback callback,
                Bundle extras);

        protected abstract void cleanUpSession(AuthTabSessionToken sessionToken);

        protected abstract boolean newAuthTabSession(AuthTabSessionToken sessionToken);
    }
}
