// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

/**
 * The JNI bridge that deal with theme collections for the NTP. This class is used to store the most
 * up-to-date theme collection information, which can originate from the current manual settings, a
 * daily update, or a profile sync. The stored value will be used when Chrome is relaunched.
 *
 * <p>The C++ NtpSyncedThemeBridge creates and owns this object, and
 * NtpAndroidCustomBackgroundService owns the C++ bridge for the lifetime of the profile. Java
 * doesn't hold a native pointer: calls into native are keyed by the {@link Profile}, so there is
 * nothing to destroy on the Java side, which the process-wide {@link NtpSyncedThemeManager}
 * couldn't do in time anyway.
 */
@NullMarked
public class NtpSyncedThemeBridge {
    /** Observer interface for synced theme updates. */
    public interface Observer {
        /**
         * Dispatched when a theme collection background arrives from sync, or when the local daily
         * refresh has pre-fetched the next image.
         *
         * @param info The {@link CustomBackgroundInfo} of the background.
         * @param primaryColor The synced primary color, or null if none was provided.
         * @param isImageFromSync True if the background arrived from sync; false if it is the next
         *     image pre-fetched for the local daily refresh.
         */
        void onThemeCollectionImageAvailable(
                @Nullable CustomBackgroundInfo info,
                @Nullable @ColorInt Integer primaryColor,
                boolean isImageFromSync);

        /**
         * Dispatched when a Chrome color arrives from sync, or is replayed from native preferences.
         *
         * @param colorId The synced Chrome color ID.
         * @param isDailyRefreshEnabled Whether daily refresh is enabled for the color.
         */
        void onChromeColorSynced(@NtpThemeColorId int colorId, boolean isDailyRefreshEnabled);

        /** Dispatched when the theme is reset to default from sync. */
        void onDefaultThemeSynced();

        /** Dispatched when the underlying Android theme sync bridge reaches readiness. */
        default void onThemeSyncReady() {}
    }

    /** Holds a {@link CustomBackgroundInfo} along with its optional synced primary color. */
    @VisibleForTesting
    static class SyncedBackgroundInfo {
        public final CustomBackgroundInfo info;
        public final @Nullable @ColorInt Integer primaryColor;

        SyncedBackgroundInfo(CustomBackgroundInfo info, @Nullable @ColorInt Integer primaryColor) {
            this.info = info;
            this.primaryColor = primaryColor;
        }
    }

    private final Profile mProfile;
    private @Nullable Observer mObserver;

    /**
     * Returns the bridge of the given profile, creating its C++ side on first use.
     *
     * @param profile The original (non-incognito) profile.
     */
    public static NtpSyncedThemeBridge getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        return NtpSyncedThemeBridgeJni.get().getForProfile(profile);
    }

    /**
     * Called by the C++ bridge to create its Java peer.
     *
     * @param profile The profile the bridge belongs to.
     */
    @CalledByNative
    @VisibleForTesting
    static NtpSyncedThemeBridge create(@JniType("Profile*") Profile profile) {
        return new NtpSyncedThemeBridge(profile);
    }

    private NtpSyncedThemeBridge(Profile profile) {
        mProfile = profile;
    }

    /**
     * Sets the observer which receives the synced theme changes, replacing the previous one.
     *
     * @param observer The observer to receive synced theme changes.
     */
    public void setObserver(Observer observer) {
        mObserver = observer;
    }

    /** Clears the observer so synced theme changes are no longer delivered. */
    public void resetObserver() {
        mObserver = null;
    }

    /** Fetches the next image for a theme collection with daily refresh enabled. */
    public void fetchNextThemeCollectionImage() {
        NtpSyncedThemeBridgeJni.get().fetchNextThemeCollectionImage(mProfile);
    }

    /**
     * Delivers the theme stored in native preferences to the observer as if it had just arrived
     * from sync. Sync doesn't re-emit a theme that is already stored, e.g. one delivered in a
     * previous session or before the observer was attached, so the observer requests it once after
     * attaching. The observer drops a theme that this device already shows.
     */
    public void getStoredContinuousThemeAndNotify() {
        NtpSyncedThemeBridgeJni.get().getStoredContinuousThemeAndNotify(mProfile);
    }

    /**
     * Sets the New Tab Page theme to a specific Chrome color and notifies the sync bridge.
     *
     * @param colorData The Chrome color and its daily refresh setting.
     */
    public void setChromeColor(NtpBackgroundDataColor colorData) {
        NtpSyncedThemeBridgeJni.get()
                .setChromeColor(
                        mProfile,
                        colorData.getThemeColorId(),
                        colorData.isChromeColorDailyRefreshEnabled());
    }

    /** Resets the New Tab Page theme to default and notifies the sync bridge. */
    public void resetCustomBackgroundInfo() {
        NtpSyncedThemeBridgeJni.get().resetCustomBackgroundInfo(mProfile);
    }

    /** Sets the user-uploaded background image and marks it local to the device. */
    public void selectLocalBackgroundImage() {
        NtpSyncedThemeBridgeJni.get().selectLocalBackgroundImage(mProfile);
    }

    /**
     * Updates the theme collection background with collection ID, primary color, and daily refresh
     * state, and notifies the sync bridge.
     *
     * @param backgroundUrl The URL of the background image.
     * @param collectionId The ID of the theme collection.
     * @param attributionLine1 The first attribution line of the background image.
     * @param attributionLine2 The second attribution line of the background image.
     * @param attributionActionUrl The URL the attribution links to, or an empty GURL.
     * @param primaryColor The primary color extracted from the theme collection image.
     * @param isDailyRefresh Whether daily refresh is enabled for this theme.
     */
    public void updateCustomBackgroundPrefsWithColor(
            GURL backgroundUrl,
            String collectionId,
            @Nullable String attributionLine1,
            @Nullable String attributionLine2,
            GURL attributionActionUrl,
            @Nullable @ColorInt Integer primaryColor,
            boolean isDailyRefresh) {
        int color = primaryColor != null ? primaryColor : 0;
        NtpSyncedThemeBridgeJni.get()
                .updateCustomBackgroundPrefsWithColor(
                        mProfile,
                        backgroundUrl,
                        collectionId,
                        attributionLine1,
                        attributionLine2,
                        attributionActionUrl,
                        color,
                        isDailyRefresh);
    }

    /** Exposes whether the underlying Android theme sync bridge is ready. */
    public boolean isThemeSyncReady() {
        return NtpSyncedThemeBridgeJni.get().isThemeSyncReady(mProfile);
    }

    /**
     * Callback from native code, triggered when the theme collection background in preferences was
     * updated by sync, or by the local daily refresh pre-fetching the next image.
     *
     * @param isImageFromSync True if the update came from sync; false if it is the pre-fetched next
     *     image.
     */
    @CalledByNative
    @VisibleForTesting
    void onThemeCollectionUpdated(boolean isImageFromSync) {
        if (mObserver == null) return;

        SyncedBackgroundInfo syncedBackgroundInfo =
                NtpSyncedThemeBridgeJni.get().getCustomBackgroundInfo(mProfile);
        mObserver.onThemeCollectionImageAvailable(
                syncedBackgroundInfo != null ? syncedBackgroundInfo.info : null,
                syncedBackgroundInfo != null ? syncedBackgroundInfo.primaryColor : null,
                isImageFromSync);
    }

    /**
     * Called by native code when a Chrome color theme has been received from Chrome Sync, or is
     * replayed from native preferences. Dispatches the event to the registered observer.
     *
     * @param colorId The synced Chrome color ID.
     * @param isDailyRefreshEnabled Whether daily refresh is enabled for the color.
     */
    @CalledByNative
    @VisibleForTesting
    void onChromeColorSynced(@NtpThemeColorId int colorId, boolean isDailyRefreshEnabled) {
        if (mObserver == null) return;

        mObserver.onChromeColorSynced(colorId, isDailyRefreshEnabled);
    }

    /**
     * Called by native code when the NTP theme has been reset to default from Chrome Sync.
     * Dispatches the event to the registered observer.
     */
    @CalledByNative
    @VisibleForTesting
    void onDefaultThemeSynced() {
        if (mObserver == null) return;

        mObserver.onDefaultThemeSynced();
    }

    /**
     * Called by native code when the underlying Android theme sync bridge reaches readiness.
     * Dispatches the event to the registered observer.
     */
    @CalledByNative
    @VisibleForTesting
    void onThemeSyncReady() {
        if (mObserver == null) return;

        mObserver.onThemeSyncReady();
    }

    /**
     * Factory method called by native code to construct a {@link SyncedBackgroundInfo} object.
     *
     * @param backgroundUrl The URL of the currently set background image.
     * @param collectionId The identifier for the theme collection, if the image is from one.
     * @param isUploadedImage True if the image was uploaded by the user from their local device.
     * @param isDailyRefreshEnabled True if the "Refresh daily" option is enabled for the
     *     collection.
     * @param attributionLine1 The first attribution line of the background image.
     * @param attributionLine2 The second attribution line of the background image.
     * @param attributionActionUrl The URL the attribution links to, or an empty GURL.
     * @param mainColor The main color stored with the background in prefs (for a sync update,
     *     copied from {@code NtpCustomBackground.main_color}). It becomes the primary (seed) color
     *     of the theme. It is 0 only in the edge case where no color is set, e.g. the sending
     *     device couldn't extract one; 0 is converted to null.
     */
    @CalledByNative
    @VisibleForTesting
    static SyncedBackgroundInfo createCustomBackgroundInfo(
            @JniType("GURL") GURL backgroundUrl,
            @JniType("std::string") String collectionId,
            boolean isUploadedImage,
            boolean isDailyRefreshEnabled,
            @JniType("std::string") String attributionLine1,
            @JniType("std::string") String attributionLine2,
            @JniType("GURL") GURL attributionActionUrl,
            @ColorInt int mainColor) {
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        backgroundUrl,
                        collectionId,
                        isUploadedImage,
                        isDailyRefreshEnabled,
                        attributionLine1,
                        attributionLine2,
                        attributionActionUrl);
        return new SyncedBackgroundInfo(info, mainColor != 0 ? mainColor : null);
    }

    @NativeMethods
    public interface Natives {
        NtpSyncedThemeBridge getForProfile(@JniType("Profile*") Profile profile);

        void fetchNextThemeCollectionImage(@JniType("Profile*") Profile profile);

        @Nullable SyncedBackgroundInfo getCustomBackgroundInfo(
                @JniType("Profile*") Profile profile);

        void getStoredContinuousThemeAndNotify(@JniType("Profile*") Profile profile);

        boolean isThemeSyncReady(@JniType("Profile*") Profile profile);

        void setChromeColor(
                @JniType("Profile*") Profile profile,
                @NtpThemeColorId int colorId,
                boolean isDailyRefreshEnabled);

        void resetCustomBackgroundInfo(@JniType("Profile*") Profile profile);

        void selectLocalBackgroundImage(@JniType("Profile*") Profile profile);

        void updateCustomBackgroundPrefsWithColor(
                @JniType("Profile*") Profile profile,
                @JniType("GURL") GURL backgroundUrl,
                @JniType("std::string") String collectionId,
                @JniType("std::string") @Nullable String attributionLine1,
                @JniType("std::string") @Nullable String attributionLine2,
                @JniType("GURL") GURL attributionActionUrl,
                int primaryColor,
                boolean isDailyRefresh);
    }
}
