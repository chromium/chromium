// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync;

import android.app.Activity;
import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorUtils;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataCustomizedColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataManager;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataThemeCollection;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.Collections;
import java.util.List;

/**
 * Java counterpart to C++ {@code CrossDeviceThemeTrackerAndroid}.
 *
 * <p>This class provides the Java UI layer with access to synced theme configurations from other
 * devices in the user's account. It receives raw theme updates from C++ and translates them into
 * UI-ready {@link NtpBackgroundDataBase} objects.
 *
 * <h3>Ownership and Lifecycle</h3>
 *
 * <ul>
 *   <li><b>C++ owns Java:</b> The C++ class {@code CrossDeviceThemeTrackerAndroid} creates this
 *       instance and holds a strong JNI global reference to it.
 *   <li><b>Java holds weak ref to C++:</b> This class references the C++ counterpart via {@code
 *       mNativePtr}. This pointer is only valid while the C++ object is alive.
 *   <li><b>Destruction:</b> When the C++ object is destroyed, it calls {@link #clearNativePtr()} to
 *       null out {@code mNativePtr}. This prevents Java from calling back into destroyed C++
 *       memory. Once the C++ object releases its global JNI reference, this Java object will be
 *       garbage collected.
 * </ul>
 */
@JNINamespace("themes")
@NullMarked
public class CrossDeviceThemeTracker {
    public interface Observer {
        void onThemesChanged();

        void onStatusChanged(@ServiceStatus int status);
    }

    private long mNativePtr;
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private @Nullable WeakReference<Activity> mActivityRef;
    private boolean mHasPendingSyncData;

    /**
     * Returns the {@link CrossDeviceThemeTracker} instance for the given {@link Profile}, or null
     * if native is not initialized or the tracker does not exist for the profile.
     *
     * @param profile The profile for which to retrieve the tracker.
     */
    public static @Nullable CrossDeviceThemeTracker getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        return CrossDeviceThemeTrackerJni.get().getForProfile(profile);
    }

    /** Called by C++ to instantiate this Java counterpart. */
    @CalledByNative
    private static CrossDeviceThemeTracker create(long nativePtr) {
        return new CrossDeviceThemeTracker(nativePtr);
    }

    private CrossDeviceThemeTracker(long nativePtr) {
        mNativePtr = nativePtr;
        mHasPendingSyncData = true;
    }

    /**
     * Sets the Activity used to resolve theme colors and drawables for synced themes. Uses a
     * WeakReference internally to prevent leaking the Activity.
     *
     * @param activity The Activity, or null to clear.
     */
    public void setActivity(@Nullable Activity activity) {
        if (activity == null) {
            mActivityRef = null;
            return;
        }
        mActivityRef = new WeakReference<>(activity);
        if (mHasPendingSyncData) {
            syncRemoteThemesToSharedPreference();
        }
    }

    private @Nullable Activity getValidActivity() {
        if (mActivityRef == null) return null;
        Activity activity = mActivityRef.get();
        if (activity == null) return null;
        if (activity.isFinishing() || activity.isDestroyed()) {
            mActivityRef = null;
            return null;
        }
        return activity;
    }

    /** Called by C++ when it is being destroyed to invalidate the weak native pointer. */
    @CalledByNative
    private void clearNativePtr() {
        mNativePtr = 0;
        mActivityRef = null;
    }

    /** Adds an observer to be notified of cross-device theme changes and status updates. */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /** Removes a previously added observer. */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @CalledByNative
    private void notifyThemesChanged() {
        Activity activity = getValidActivity();
        if (activity != null) {
            syncRemoteThemesToSharedPreference();
        } else {
            mHasPendingSyncData = true;
        }
        for (Observer observer : mObservers) {
            observer.onThemesChanged();
        }
    }

    private void syncRemoteThemesToSharedPreference() {
        Activity activity = getValidActivity();
        if (activity == null) {
            mHasPendingSyncData = true;
            return;
        }
        List<NtpBackgroundDataBase> remoteThemes = getThemes(activity);
        if (remoteThemes.isEmpty()) return;
        NtpBackgroundDataManager dataManager = new NtpBackgroundDataManager(activity);
        for (NtpBackgroundDataBase theme : remoteThemes) {
            dataManager.saveRemoteSyncDataToSharedPreference(theme);
        }
        mHasPendingSyncData = false;
    }

    @CalledByNative
    private void notifyStatusChanged(@JniType("ServiceStatus") @ServiceStatus int status) {
        for (Observer observer : mObservers) {
            observer.onStatusChanged(status);
        }
    }

    /** Returns the list of synced NTP background data from other devices. */
    public List<NtpBackgroundDataBase> getThemes(Context context) {
        if (mNativePtr == 0) return Collections.emptyList();
        return CrossDeviceThemeTrackerJni.get().getThemes(mNativePtr, context);
    }

    /** Returns the current service status of the cross-device theme tracker. */
    public @ServiceStatus int getServiceStatus() {
        if (mNativePtr == 0) return ServiceStatus.SYNC_DISABLED;
        return CrossDeviceThemeTrackerJni.get().getServiceStatus(mNativePtr);
    }

    @CalledByNative
    private static NtpBackgroundDataThemeCollection createThemeCollectionData(
            Context context,
            @PlatformType int platformType,
            @JniType("std::string") String url,
            @JniType("std::string") String collectionId,
            boolean isDailyRefresh,
            boolean hasChromeColor,
            int chromeColorId,
            boolean hasUserColor,
            int userPrimaryColor) {
        GURL gurl = new GURL(url);
        CustomBackgroundInfo customBgInfo =
                new CustomBackgroundInfo(
                        gurl, collectionId, /* isUploadedImage= */ false, isDailyRefresh);

        Integer primaryColor = null;
        if (hasChromeColor) {
            NtpThemeColorInfo colorInfo =
                    NtpThemeColorUtils.createNtpThemeColorInfo(context, chromeColorId);
            if (colorInfo != null && colorInfo.primaryColorResId != 0) {
                primaryColor = context.getColor(colorInfo.primaryColorResId);
            }
        } else if (hasUserColor) {
            primaryColor = userPrimaryColor;
        }

        return new NtpBackgroundDataThemeCollection(
                platformType,
                customBgInfo,
                /* backgroundImageInfo= */ null,
                /* bitmap= */ null,
                primaryColor,
                /* fileIdHash= */ null);
    }

    @CalledByNative
    private static NtpBackgroundDataColor createColorData(
            Context context,
            @PlatformType int platformType,
            int themeColorId,
            boolean isDailyRefresh) {
        return new NtpBackgroundDataColor(context, platformType, themeColorId, isDailyRefresh);
    }

    @CalledByNative
    private static NtpBackgroundDataCustomizedColor createCustomizedColorData(
            Context context,
            @PlatformType int platformType,
            int primaryColorLight,
            int primaryColorDark,
            int backgroundColorLight,
            int backgroundColorDark) {
        return new NtpBackgroundDataCustomizedColor(
                context,
                platformType,
                primaryColorLight,
                primaryColorDark,
                backgroundColorLight,
                backgroundColorDark);
    }

    public static void setInstanceForTesting(Natives instance) {
        CrossDeviceThemeTrackerJni.setInstanceForTesting(instance); // IN-TEST
        ResettersForTesting.register(
                () -> CrossDeviceThemeTrackerJni.setInstanceForTesting(null)); // IN-TEST
    }

    boolean getHasPendingSyncDataForTesting() {
        return mHasPendingSyncData;
    }

    @NativeMethods
    public interface Natives {
        @Nullable CrossDeviceThemeTracker getForProfile(@JniType("Profile*") Profile profile);

        List<NtpBackgroundDataBase> getThemes(
                long nativeCrossDeviceThemeTrackerAndroid, Context context);

        @JniType("ServiceStatus")
        @ServiceStatus
        int getServiceStatus(long nativeCrossDeviceThemeTrackerAndroid);
    }
}
