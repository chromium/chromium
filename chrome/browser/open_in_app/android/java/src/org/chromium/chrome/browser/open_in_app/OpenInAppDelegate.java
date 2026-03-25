// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.open_in_app;

import android.graphics.drawable.Drawable;

import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.external_intents.ExternalNavigationHelper;
import org.chromium.url.GURL;

/** A delegate for handling the open in app action for a {@link Tab}. */
@NullMarked
public class OpenInAppDelegate implements UserData {
    /** Info needed to display open in app action UI. */
    public static class OpenInAppInfo {
        /** The {@link Runnable} to run to open in app. */
        public final Runnable action;

        /**
         * App name to display for the open in app action. Null if the URL can be opened in more
         * than one app.
         */
        public final @Nullable CharSequence appName;

        /**
         * App icon to display for the open in app action. Null if the URL can be opened in more
         * than one app.
         */
        public final @Nullable Drawable appIcon;

        public OpenInAppInfo(
                Runnable action, @Nullable CharSequence appName, @Nullable Drawable appIcon) {
            this.action = action;
            this.appName = appName;
            this.appIcon = appIcon;
        }
    }

    private final Tab mTab;
    private @Nullable OpenInAppInfo mCurrentOpenInAppInfo;
    private @Nullable ExternalNavigationHelper mExternalNavigationHelper;
    private @Nullable GURL mLastNavigatedUrl;

    public void updateOpenInAppInfo(@Nullable OpenInAppInfo openInAppInfo) {
        mCurrentOpenInAppInfo = openInAppInfo;
    }

    /** Returns the current {@link OpenInAppInfo}. */
    public @Nullable OpenInAppInfo getCurrentOpenInAppInfo() {
        return mCurrentOpenInAppInfo;
    }

    /** Sets the last navigated {@link GURL}. */
    public void setLastNavigatedUrl(@Nullable GURL url) {
        mLastNavigatedUrl = url;
    }

    /** Returns the last navigated {@link GURL}. */
    public @Nullable GURL getLastNavigatedUrl() {
        return mLastNavigatedUrl;
    }

    /** Sets a {@link ExternalNavigationHelper}. */
    public void setExternalNavigationHelper(ExternalNavigationHelper helper) {
        mExternalNavigationHelper = helper;
    }

    /** Returns the {@link ExternalNavigationHelper}. */
    public @Nullable ExternalNavigationHelper getExternalNavigationHelper() {
        return mExternalNavigationHelper;
    }

    /** Returns the {@link Tab} that hosts this {@link OpenInAppDelegate}. */
    public Tab getTab() {
        return mTab;
    }

    private static final Class<OpenInAppDelegate> USER_DATA_KEY = OpenInAppDelegate.class;

    /**
     * Returns the {@link OpenInAppDelegate} for a given {@link Tab}.
     *
     * @param tab The {@link Tab} that hosts the {@link OpenInAppDelegate}.
     * @return The {@link OpenInAppDelegate} for a given {@link Tab}. Creates one if not currently
     *     present.
     */
    public static OpenInAppDelegate from(Tab tab) {
        OpenInAppDelegate delegate = get(tab);
        if (delegate == null) {
            delegate = tab.getUserDataHost().setUserData(USER_DATA_KEY, new OpenInAppDelegate(tab));
        }
        return delegate;
    }

    private static @Nullable OpenInAppDelegate get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private OpenInAppDelegate(Tab tab) {
        mTab = tab;
    }
}
