// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.graphics.Bitmap;
import android.text.TextUtils;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.sync.protocol.WebApkIconInfo;
import org.chromium.components.sync.protocol.WebApkSpecifics;

import java.util.ArrayList;
import java.util.List;

/** Static class to update WebAPK data to sync. */
@JNINamespace("webapk")
public class WebApkSyncService {
    private static final long UNIX_OFFSET_MICROS = 11644473600000000L;

    /** Called with update result. */
    public static interface PwaRestorableListCallback {
        @CalledByNative("PwaRestorableListCallback")
        public void onRestorableAppsAvailable(
                boolean success,
                @NonNull String[] appIds,
                @NonNull String[] appNames,
                @NonNull int[] lastUsedInDays,
                @NonNull List<Bitmap> icons);
    }

    static void onWebApkUsed(
            BrowserServicesIntentDataProvider intendDataProvider,
            WebappDataStorage storage,
            boolean isInstall) {
        WebApkSpecifics specifics =
                getWebApkSpecifics(WebappInfo.create(intendDataProvider), storage);
        if (specifics != null) {
            WebApkSyncServiceJni.get().onWebApkUsed(specifics.toByteArray(), isInstall);
        }
    }

    static void onWebApkUninstalled(String manifestId) {
        WebApkSyncServiceJni.get().onWebApkUninstalled(manifestId);
    }

    static WebApkSpecifics getWebApkSpecifics(WebappInfo webApkInfo, WebappDataStorage storage) {
        if (webApkInfo == null || !webApkInfo.isForWebApk()) {
            return null;
        }

        WebApkSpecifics.Builder webApkSpecificsBuilder = WebApkSpecifics.newBuilder();

        if (TextUtils.isEmpty(webApkInfo.manifestId())) {
            return null;
        }
        webApkSpecificsBuilder.setManifestId(webApkInfo.manifestId());

        if (!TextUtils.isEmpty(webApkInfo.manifestStartUrl())) {
            webApkSpecificsBuilder.setStartUrl(webApkInfo.manifestStartUrl());
        }

        if (!TextUtils.isEmpty(webApkInfo.name())) {
            webApkSpecificsBuilder.setName(webApkInfo.name());
        } else if (!TextUtils.isEmpty(webApkInfo.shortName())) {
            webApkSpecificsBuilder.setName(webApkInfo.shortName());
        }

        if (webApkInfo.hasValidToolbarColor()) {
            webApkSpecificsBuilder.setThemeColor((int) webApkInfo.toolbarColor());
        }

        if (!TextUtils.isEmpty(webApkInfo.scopeUrl())) {
            webApkSpecificsBuilder.setScope(webApkInfo.scopeUrl());
        }

        if (webApkInfo.shellApkVersion() >= WebappIcon.ICON_WITH_URL_AND_HASH_SHELL_VERSION) {
            String iconUrl = webApkInfo.icon().iconUrl();
            if (!TextUtils.isEmpty(iconUrl)) {
                WebApkIconInfo iconInfo =
                        WebApkIconInfo.newBuilder()
                                .setUrl(iconUrl)
                                .setPurpose(
                                        webApkInfo.isIconAdaptive()
                                                ? WebApkIconInfo.Purpose.MASKABLE
                                                : WebApkIconInfo.Purpose.ANY)
                                .build();
                webApkSpecificsBuilder.addIconInfos(iconInfo);
            }
        }
        for (String iconUrl : webApkInfo.iconUrlToMurmur2HashMap().keySet()) {
            if (!TextUtils.isEmpty(iconUrl)) {
                webApkSpecificsBuilder.addIconInfos(
                        WebApkIconInfo.newBuilder().setUrl(iconUrl).build());
            }
        }

        webApkSpecificsBuilder.setLastUsedTimeWindowsEpochMicros(
                toMicrosecondsSinceWindowsEpoch(storage.getLastUsedTimeMs()));

        return webApkSpecificsBuilder.build();
    }

    static void removeOldWebAPKsFromSync(long currentTimeMsSinceUnixEpoch) {
        WebApkSyncServiceJni.get().removeOldWebAPKsFromSync(currentTimeMsSinceUnixEpoch);
    }

    private static long toMicrosecondsSinceWindowsEpoch(long timeInMills) {
        return timeInMills * 1000 + UNIX_OFFSET_MICROS;
    }

    public static void fetchRestorableApps(Profile profile, PwaRestorableListCallback callback) {
        WebApkSyncServiceJni.get().fetchRestorableApps(profile, callback);
    }

    @CalledByNative
    private static List<Bitmap> createBitmapList() {
        return new ArrayList<Bitmap>();
    }

    @CalledByNative
    private static void addToBitmapList(List<Bitmap> bitmaps, Bitmap bitmap) {
        bitmaps.add(bitmap);
    }

    @NativeMethods
    interface Natives {
        void onWebApkUsed(byte[] webApkSpecifics, boolean isInstall);

        void onWebApkUninstalled(@JniType("std::string") String manifestId);

        void removeOldWebAPKsFromSync(long currentTimeMsSinceUnixEpoch);

        void fetchRestorableApps(
                @JniType("Profile*") Profile profile, PwaRestorableListCallback callback);
    }
}
