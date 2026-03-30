// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.finds;

import android.app.NotificationManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;

/** Java bridge to the C++ FindsService. */
@JNINamespace("finds")
@NullMarked
public class FindsService {
    /** Observer for the FindsService. */
    public interface Observer {
        /** Called when the opt-in criteria is fulfilled. */
        default void onOptInCriteriaFulfilled() {}
    }

    /**
     * Retrieves the FindsService associated with the given Profile. Returns null for incognito
     * profiles.
     */
    public static @Nullable FindsService getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        return FindsServiceFactoryJni.get().getForProfile(profile);
    }

    // Called by native to create a Java object representing the C++ service.
    @CalledByNative
    private static FindsService create(long nativeFindsServiceAndroid) {
        return new FindsService(nativeFindsServiceAndroid);
    }

    @SuppressWarnings("unused")
    private long mNativeFindsServiceAndroid;

    private final ObserverList<Observer> mObservers = new ObserverList<>();

    // Use FindsServiceFactory to get a FindsService instance.
    private FindsService(long nativeFindsServiceAndroid) {
        mNativeFindsServiceAndroid = nativeFindsServiceAndroid;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeFindsServiceAndroid = 0;
        mObservers.clear();
    }

    /**
     * Adds an observer.
     *
     * @param observer The observer to add.
     */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer.
     *
     * @param observer The observer to remove.
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @CalledByNative
    private void onOptInCriteriaFulfilled() {
        for (Observer observer : mObservers) {
            observer.onOptInCriteriaFulfilled();
        }
    }

    @CalledByNative
    private static void checkAreFindsNotificationsEnabled(long callbackId) {
        if (!NotificationProxyUtils.areNotificationsEnabled()) {
            FindsServiceJni.get().onCheckAreFindsNotificationsEnabled(callbackId, false);
            return;
        }
        BaseNotificationManagerProxyFactory.create()
                .getNotificationChannel(
                        ChromeChannelDefinitions.ChannelId.CHROME_FINDS,
                        (channel) -> {
                            boolean enabled =
                                    (channel != null
                                            && channel.getImportance()
                                                    != NotificationManager.IMPORTANCE_NONE);
                            FindsServiceJni.get()
                                    .onCheckAreFindsNotificationsEnabled(callbackId, enabled);
                        });
    }

    @NativeMethods
    interface Natives {
        void onCheckAreFindsNotificationsEnabled(long callbackId, boolean result);
    }
}
