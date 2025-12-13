// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;

/** Manages the NTP's theme state and notifies observers of changes. */
@NullMarked
public class NtpThemeStateProvider {
    /** An interface to get NTP theme state updates. */
    @FunctionalInterface
    public interface Observer {
        /** Notify observers to apply a new theme. */
        void applyThemeChanges();
    }

    private static @Nullable NtpThemeStateProvider sInstanceForTesting;

    /** Static class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static final NtpThemeStateProvider sInstance = new NtpThemeStateProvider();
    }

    private final ObserverList<Observer> mObservers;

    /** Returns the singleton instance of NtpThemeStateProvider. */
    public static NtpThemeStateProvider getInstance() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return NtpThemeStateProvider.LazyHolder.sInstance;
    }

    private NtpThemeStateProvider() {
        mObservers = new ObserverList<>();
    }

    /** Adds an {@link Observer} to receive updates when the NTP theme state changes. */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes the given observer from the observer list.
     *
     * @param observer The observer to remove.
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /** Notifies observers to apply a new theme. */
    public void notifyApplyThemeChanges() {
        for (Observer observer : mObservers) {
            observer.applyThemeChanges();
        }
    }

    /**
     * Sets a NtpThemeStateProvider instance for testing.
     *
     * @param instance The instance to set.
     */
    public static void setInstanceForTesting(@Nullable NtpThemeStateProvider instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }
}
