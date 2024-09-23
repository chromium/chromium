// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.variations;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.ByteString;

import org.chromium.android_webview.proto.AwVariationsSeedOuterClass.AwVariationsSeed;
import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedInfo;

import java.io.Closeable;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Date;
import java.util.concurrent.TimeUnit;

/** Utilities for manipulating variations seeds, used by both WebView and WebView's services. */
public class VariationsUtils {
    // Changes to the tag below must be accompanied with changes to WebView
    // finch smoke tests since they look for this tag in the logcat.
    private static final String TAG = "VariationsUtils";

    private static final String SEED_FILE_NAME = "variations_seed";
    private static final String NEW_SEED_FILE_NAME = "variations_seed_new";
    private static final String STAMP_FILE_NAME = "variations_stamp";

    public static void closeSafely(Closeable c) {
        if (c != null) {
            try {
                c.close();
            } catch (IOException e) {
                Log.e(TAG, "Failed to close " + c);
            }
        }
    }

    // Both the WebView variations service and apps using WebView keep a pair of seed files in their
    // data directory. New seeds are written to the new seed file, and then the old file is replaced
    // with the new file.
    public static File getSeedFile() {
        return new File(PathUtils.getDataDirectory(), SEED_FILE_NAME);
    }

    public static File getNewSeedFile() {
        return new File(PathUtils.getDataDirectory(), NEW_SEED_FILE_NAME);
    }

    public static void replaceOldWithNewSeed() {
        File oldSeedFile = getSeedFile();
        File newSeedFile = getNewSeedFile();
        if (!newSeedFile.renameTo(oldSeedFile)) {
            Log.e(
                    TAG,
                    "Failed to replace old seed " + oldSeedFile + " with new seed " + newSeedFile);
        }
    }

    // There's a 3rd timestamp file whose modification time is the time of the last seed request. In
    // the app, this is used to rate-limit seed requests. In the service, this is used to cancel the
    // periodic seed fetch if no app requests the seed for a long time.
    public static File getStampFile() {
        return new File(PathUtils.getDataDirectory(), STAMP_FILE_NAME);
    }

    // Get the timestamp, in milliseconds since epoch, or 0 if the file doesn't exist.
    public static long getStampTime() {
        return getStampFile().lastModified();
    }

    // Creates/updates the timestamp with the current time.
    public static void updateStampTime() {
        updateStampTime(new Date().getTime());
    }

    // Creates/updates the timestamp with the specified time.
    @VisibleForTesting
    public static void updateStampTime(long now) {
        File file = getStampFile();
        try {
            if (!file.createNewFile()) {
                file.setLastModified(now);
            }
        } catch (IOException e) {
            Log.e(TAG, "Failed to write " + file);
        }
    }

    // Silently returns null in case of a missing or truncated seed, which is expected in case
    // of an incomplete download or copy. Other IO problems are actual errors, and are logged.
    @Nullable
    public static SeedInfo readSeedFile(File inFile) {
        if (!inFile.exists()) return null;
        FileInputStream in = null;
        try {
            in = new FileInputStream(inFile);

            AwVariationsSeed proto = null;
            try {
                proto = AwVariationsSeed.parseFrom(in);
            } catch (Exception e) {
                // Could be InvalidProtocolBufferException or InvalidStateException (b/324851449).
                return null;
            }

            if (!proto.hasSignature()
                    || !proto.hasCountry()
                    || !proto.hasDate()
                    || !proto.hasIsGzipCompressed()
                    || !proto.hasSeedData()) {
                return null;
            }

            SeedInfo info = new SeedInfo();
            info.signature = proto.getSignature();
            info.country = proto.getCountry();
            info.isGzipCompressed = proto.getIsGzipCompressed();
            info.seedData = proto.getSeedData().toByteArray();
            info.date = proto.getDate();

            return info;
        } catch (IOException e) {
            Log.e(TAG, "Failed reading seed file \"" + inFile + "\": " + e.getMessage());
            return null;
        } finally {
            closeSafely(in);
        }
    }

    // Returns true on success. "out" will always be closed, regardless of success.
    public static boolean writeSeed(FileOutputStream out, SeedInfo info) {
        try {
            AwVariationsSeed proto =
                    AwVariationsSeed.newBuilder()
                            .setSignature(info.signature)
                            .setCountry(info.country)
                            .setDate(info.date)
                            .setIsGzipCompressed(info.isGzipCompressed)
                            .setSeedData(ByteString.copyFrom(info.seedData))
                            .build();
            proto.writeTo(out);
            return true;
        } catch (IOException e) {
            Log.e(TAG, "Failed writing seed file: " + e.getMessage());
            return false;
        } finally {
            closeSafely(out);
        }
    }

    // Returns the value of the |switchName| flag converted from seconds to milliseconds. If the
    // |switchName| flag isn't present, or contains an invalid value, |defaultValueMillis| will be
    // returned.
    public static long getDurationSwitchValueInMillis(String switchName, long defaultValueMillis) {
        CommandLine cli = CommandLine.getInstance();
        if (!cli.hasSwitch(switchName)) {
            return defaultValueMillis;
        }
        try {
            return TimeUnit.SECONDS.toMillis(Long.parseLong(cli.getSwitchValue(switchName)));
        } catch (NumberFormatException e) {
            Log.e(TAG, "Invalid value for flag " + switchName, e);
            return defaultValueMillis;
        }
    }

    // Logs an INFO message if running in a debug build of Android.
    public static void debugLog(String message) {
        if (BuildInfo.isDebugAndroid()) {
            Log.i(TAG, message);
        }
    }
}
