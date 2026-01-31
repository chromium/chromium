// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.open_in_app;

import android.graphics.drawable.Drawable;

import org.chromium.base.ObserverList;
import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;

/** A delegate for handling the open in app action for a {@link Tab}. */
@NullMarked
public class OpenInAppDelegate implements UserData {
    /** Observer for changes to the open in app state of the current tab. */
    public interface Observer {
        // TODO(crbug.com/450253146): OpenInAppInfo changes before the URL is loaded, so the
        // implementing class should wait for the navigation to be committed before updating the UI.
        /**
         * Called when the open in app info changes.
         *
         * @param openInAppInfo The new {@link OpenInAppInfo}. Null if the new URL in the tab is
         *     ineligible to open in app.
         */
        void onOpenInAppInfoChanged(@Nullable OpenInAppInfo openInAppInfo);
    }

    /** Info needed to display open in app action UI. */
    public static class OpenInAppInfo {
        /** The {@link Runnable} to run to open in app. */
        public final Runnable action;

        /**
         * App name to display for the open in app action. Null if the URL can be opened in more
         * than one app.
         */
        public final @Nullable String appName;

        /**
         * App icon to display for the open in app action. Null if the URL can be opened in more
         * than one app.
         */
        public final @Nullable Drawable appIcon;

        public OpenInAppInfo(
                Runnable action, @Nullable String appName, @Nullable Drawable appIcon) {
            this.action = action;
            this.appName = appName;
            this.appIcon = appIcon;
        }
    }

    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private @Nullable OpenInAppInfo mCurrentOpenInAppInfo;

    /**
     * Adds an {@link Observer} to be notified when the open in app info changes.
     *
     * @param observer The {@link Observer} to notify.
     */
    public void addOpenInAppInfoObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an {@link Observer}.
     *
     * @param observer The {@link Observer} to notify.
     */
    public void removeOpenInAppInfoObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    public void updateOpenInAppInfo(@Nullable OpenInAppInfo openInAppInfo) {
        mCurrentOpenInAppInfo = openInAppInfo;
        for (Observer observer : mObservers) {
            observer.onOpenInAppInfoChanged(openInAppInfo);
        }
    }

    /** Returns the current {@link OpenInAppInfo}. */
    public @Nullable OpenInAppInfo getCurrentOpenInAppInfo() {
        return mCurrentOpenInAppInfo;
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
            delegate = tab.getUserDataHost().setUserData(USER_DATA_KEY, new OpenInAppDelegate());
        }
        return delegate;
    }

    private static @Nullable OpenInAppDelegate get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private OpenInAppDelegate() {}
}
