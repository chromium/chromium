// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.variations;

import android.content.ContentResolver;
import android.content.Context;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.os.ParcelFileDescriptor.AutoCloseInputStream;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeActionIds;
import org.chromium.android_webview.common.VariationsFastFetchModeUtils;
import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.variations.LoadSeedResult;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Date;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

/**
 * A {@link SafeModeAction} to ensure the variations seed is distributed on an app's first run.
 * This is the browser-process counterpart to {@link
 * org.chromium.android_webview.services.NonEmbeddedFastVariationsSeedSafeModeAction}.
 */
@Lifetime.Singleton
public class FastVariationsSeedSafeModeAction implements SafeModeAction {
    private static final String TAG = "FastVariationsSeed";
    // This ID should not be reused.
    private static final String ID = SafeModeActionIds.FAST_VARIATIONS_SEED;
    private final String mWebViewPackageName;
    private static boolean sHasRun;
    private static File sSeedFile = VariationsUtils.getSeedFile();

    @VisibleForTesting
    public FastVariationsSeedSafeModeAction(String webViewPackageName) {
        mWebViewPackageName = webViewPackageName;
    }

    public FastVariationsSeedSafeModeAction() {
        mWebViewPackageName = AwBrowserProcess.getWebViewPackageName();
    }

    @VisibleForTesting
    public static void setAlternateSeedFilePath(File seedFile) {
        sSeedFile = seedFile;
    }

    /**
     * Determine whether a Fast Variations mitigation action is enabled.
     * Determined when the safemode action runs or does not run.
     */
    public static boolean hasRun() {
        return sHasRun;
    }

    @Override
    @NonNull
    public String getId() {
        return ID;
    }

    @Override
    public boolean execute() {
        sHasRun = true;
        long currDateTime = new Date().getTime();
        SeedParser parser = new SeedParser();
        long stampTime = sSeedFile.lastModified();
        long ageInMillis = currDateTime - stampTime;

        if (sSeedFile.exists() && ageInMillis > 0) {
            logSeedFileAge(ageInMillis);
        }
        // If we see that the local seed file has not exceeded the
        // maximum seed age of 15 minutes, parse the local seed instead
        // of requesting a new one from the ContentProvider
        if ((currDateTime - stampTime <= VariationsFastFetchModeUtils.MAX_ALLOWABLE_SEED_AGE_MS)
                && stampTime > 0) {
            return parser.parseAndSaveSeedFile();
        }
        byte[] protoAsByteArray = getProtoFromServiceBlocking();
        if (protoAsByteArray != null
                && protoAsByteArray.length > 0
                && parser.parseSeedAsByteArray(protoAsByteArray)) {
            PostTask.postTask(TaskTraits.BEST_EFFORT, new SeedWriterTask(protoAsByteArray));
            return true;
        } else {
            Log.e(TAG, "Failed to fetch seed from ContentProvider.");
            return false;
        }
    }

    private void logSeedFileAge(long ageInMillis) {
        int seconds = (int) (ageInMillis / 1000) % 60;
        int minutes = (int) (ageInMillis / TimeUnit.MINUTES.toMillis(1)) % 60;
        int hrs = (int) (ageInMillis / TimeUnit.HOURS.toMillis(1));

        String formattedAge =
                String.format(Locale.US, "%02d:%02d:%02d (hh:mm:ss)", hrs, minutes, seconds);
        Log.i(TAG, "Seed file age - " + formattedAge);
    }

    /**
     * This class queries {@link SafeModeVariationsSeedContentProvider} for the
     * latest variations seed.
     *
     * @return Byte array representation of a variations seed
     */
    private byte[] getProtoFromServiceBlocking() {
        return new ContentProviderQuery(mWebViewPackageName)
                .querySafeModeVariationsSeedContentProvider();
    }

    // TODO(crbug.com/40259816): Update this to include timeout capability.
    private static class ContentProviderQuery {
        private static final String URI_SUFFIX = ".SafeModeVariationsSeedContentProvider";
        private static final String URI_PATH = VariationsFastFetchModeUtils.URI_PATH;
        private final String mWebViewPackageName;

        ContentProviderQuery(String webViewPackageName) {
            mWebViewPackageName = webViewPackageName;
        }

        public byte[] querySafeModeVariationsSeedContentProvider() {
            try {
                Uri uri =
                        new Uri.Builder()
                                .scheme(ContentResolver.SCHEME_CONTENT)
                                .authority(this.mWebViewPackageName + URI_SUFFIX)
                                .path(URI_PATH)
                                .build();
                final Context appContext = ContextUtils.getApplicationContext();
                try (ParcelFileDescriptor pfd =
                        appContext
                                .getContentResolver()
                                .openFileDescriptor(uri, /* mode= */ "r", null)) {
                    if (pfd == null) {
                        Log.e(TAG, "Failed to query SafeMode seed from: " + "'" + uri + "'");
                        return null;
                    }
                    return readProtoFromFile(pfd);
                }
            } catch (IOException e) {
                // Should not crash safe mode.
                // Simply log the message and return null here
                Log.w(TAG, e.toString());
                return null;
            }
        }

        private byte[] readProtoFromFile(ParcelFileDescriptor pfd) throws IOException {
            try (AutoCloseInputStream is = new AutoCloseInputStream(pfd)) {
                byte[] buffer = new byte[2048];
                ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
                int bytesRead;

                while ((bytesRead = is.read(buffer)) != -1) {
                    byteArrayOutputStream.write(buffer, 0, bytesRead);
                }

                return byteArrayOutputStream.toByteArray();
            }
        }
    }

    private static class SeedParser {
        public boolean parseSeedAsByteArray(byte[] protoAsByteArray) {
            if (protoAsByteArray == null) {
                Log.w(TAG, "Seed String is empty");
                return false;
            }
            boolean success =
                    VariationsSeedLoader.parseAndSaveSeedProtoFromByteArray(protoAsByteArray);
            if (success) {
                Log.i(TAG, "Successfully parsed and loaded new seed!");
                recordLoadSeedResult(LoadSeedResult.SUCCESS);
                VariationsSeedLoader.maybeRecordSeedFileTime(sSeedFile.lastModified());
            } else {
                Log.i(TAG, "Failure parsing and loading seed!");
                recordLoadSeedResult(LoadSeedResult.LOAD_OTHER_FAILURE);
            }
            return success;
        }

        public boolean parseAndSaveSeedFile() {
            boolean success = VariationsSeedLoader.parseAndSaveSeedFile(sSeedFile);
            if (success) {
                Log.i(TAG, "Successfully parsed and loaded new seed!");
                recordLoadSeedResult(LoadSeedResult.SUCCESS);
                VariationsSeedLoader.maybeRecordSeedFileTime(sSeedFile.lastModified());
            } else {
                Log.i(TAG, "Seed fetch not successful.");
                recordLoadSeedResult(LoadSeedResult.LOAD_OTHER_FAILURE);
            }
            return success;
        }

        private void recordLoadSeedResult(@LoadSeedResult int result) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Variations.SafeMode.LoadSafeSeed.Result",
                    result,
                    LoadSeedResult.MAX_VALUE + 1);
        }
    }

    private static class SeedWriterTask implements Runnable {
        private byte[] mProtoAsByteArray;

        public SeedWriterTask(byte[] protoAsByteArray) {
            mProtoAsByteArray = protoAsByteArray;
        }

        @Override
        public void run() {
            if (writeToSeedFile()) {
                VariationsUtils.updateStampTime();
            }
        }

        private boolean writeToSeedFile() {
            String filePath = sSeedFile.getPath();
            try (FileOutputStream out = new FileOutputStream(filePath, false)) {
                out.write(mProtoAsByteArray);
                out.flush();
                return true;
            } catch (IOException e) {
                Log.e(TAG, "Failed writing seed file: " + e.getMessage());
                return false;
            }
        }
    }
}
