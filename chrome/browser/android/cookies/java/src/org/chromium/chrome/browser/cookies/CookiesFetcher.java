// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cookies;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.ImportantFileWriterAndroid;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;

import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import javax.crypto.Cipher;
import javax.crypto.CipherInputStream;
import javax.crypto.CipherOutputStream;

/**
 * Responsible for fetching, (de)serializing, and restoring cookies between the CookieJar and an
 * encrypted file storage.
 *
 * <p>These methods are used for incognito only. Incognito cookies are kept separately from other
 * profiles' cookies, and are normally not persisted (on most platforms). However, on Android we
 * serialize and store them to avoid logging the user out of their accounts being used in incognito,
 * if the app is killed e.g. while in the background.
 */
public class CookiesFetcher implements Destroyable {
    /** The default file name for the encrypted cookies storage for the primary OTR profile. */
    @VisibleForTesting public static final String DEFAULT_COOKIE_FILE_NAME = "COOKIES.DAT";

    /** Used for logging. */
    private static final String TAG = "CookiesFetcher";

    private final ProfileProvider mProfileProvider;
    private final CipherFactory mCipherFactory;
    private final ProfileManager.Observer mProfileManagerObserver;
    private final String mCookieDirPath;

    /**
     * Constructs a CookiesFetcher for a given {@link ProfileProvider}.
     *
     * <p>Consumers must call {@link #destroy()} on this object.
     */
    public CookiesFetcher(ProfileProvider profileProvider, CipherFactory cipherFactory) {
        mProfileProvider = profileProvider;
        mCipherFactory = cipherFactory;

        mProfileManagerObserver =
                new ProfileManager.Observer() {
                    @Override
                    public void onProfileAdded(Profile profile) {}

                    @Override
                    public void onProfileDestroyed(Profile profile) {
                        if (profile.isOffTheRecord()
                                && mProfileProvider.getOffTheRecordProfile(false) == profile) {
                            scheduleDeleteCookies();
                        }
                    }
                };
        ProfileManager.addObserver(mProfileManagerObserver);

        mCookieDirPath =
                CookiesFetcherJni.get()
                        .getCookieFileDirectory(mProfileProvider.getOriginalProfile());
    }

    @Override
    public void destroy() {
        ProfileManager.removeObserver(mProfileManagerObserver);
    }

    /** Return the directory for cookie files for the appropriate Profile. */
    protected File getCookieDir() {
        return new File(mCookieDirPath);
    }

    /** Return the cookie file path for the appropriate Profile. */
    @VisibleForTesting
    String fetchAbsoluteFilePath() {
        ThreadUtils.assertOnBackgroundThread();
        File directory = getCookieDir();
        if (!directory.exists() && !directory.mkdir()) {
            Log.e(TAG, "Failed to create cookie directory");
            return null;
        }
        return new File(directory, fetchFileName()).getAbsolutePath();
    }

    /** Return the cookie file name for the appropriate Profile. */
    protected String fetchFileName() {
        return DEFAULT_COOKIE_FILE_NAME;
    }

    /**
     * Return the legacy cookie file path, and this should only be used for the initial Profile for
     * migration purposes.
     */
    @VisibleForTesting
    static String fetchLegacyFileName() {
        ThreadUtils.assertOnBackgroundThread();
        return ContextUtils.getApplicationContext()
                .getFileStreamPath(DEFAULT_COOKIE_FILE_NAME)
                .getAbsolutePath();
    }

    /** Return whether the legacy cookie file is applicable for the associated Profile. */
    protected boolean isLegacyFileApplicable() {
        ThreadUtils.assertOnUiThread();
        return mProfileProvider.getOriginalProfile().isInitialProfile();
    }

    /** Asynchronously fetches cookies from the incognito profile and saves them to a file. */
    public void persistCookies() {
        ThreadUtils.assertOnUiThread();
        if (!mProfileProvider.hasOffTheRecordProfile()) {
            return;
        }
        Profile offTheRecordProfile = mProfileProvider.getOffTheRecordProfile(false);
        CookiesFetcherJni.get().persistCookies(offTheRecordProfile, this);
    }

    /**
     * If an incognito profile exists, synchronously fetch cookies from the file specified and
     * populate the incognito profile with it. Otherwise deletes the file and does not restore the
     * cookies.
     *
     * @param restoreCompletedAction Called when the restore action has been completed (regardless
     *     of whether any cookies were in fact restored).
     */
    public void restoreCookies(@NonNull Runnable restoreCompletedAction) {
        ThreadUtils.assertOnUiThread();
        if (!mProfileProvider.hasOffTheRecordProfile()) {
            scheduleDeleteCookies();
            restoreCompletedAction.run();
            return;
        }
        Profile offTheRecordProfile = mProfileProvider.getOffTheRecordProfile(false);
        new AsyncTask<List<CanonicalCookie>>() {
            private File getCookieFile() {
                String fileName = fetchAbsoluteFilePath();
                if (fileName == null) {
                    Log.e(TAG, "Failed to load cookie file, skipping restore.");
                    return null;
                }
                File fileIn = new File(fileName);
                return fileIn.exists() ? fileIn : null;
            }

            @Override
            protected List<CanonicalCookie> doInBackground() {
                // Read cookies from disk on a background thread to avoid strict mode violations.
                List<CanonicalCookie> cookies = new ArrayList<CanonicalCookie>();
                DataInputStream in = null;
                try {
                    Cipher cipher = mCipherFactory.getCipher(Cipher.DECRYPT_MODE);
                    if (cipher == null) {
                        // Something is wrong. Can't encrypt, don't restore cookies.
                        return cookies;
                    }
                    File fileIn = getCookieFile();
                    if (fileIn == null) return cookies; // Nothing to read

                    FileInputStream streamIn = new FileInputStream(fileIn);
                    in = new DataInputStream(new CipherInputStream(streamIn, cipher));
                    cookies = CanonicalCookie.readListFromStream(in);
                } catch (Throwable t) {
                    Log.w(TAG, "Error restoring cookies.", t);
                } finally {
                    try {
                        if (in != null) in.close();
                    } catch (Throwable t) {
                        Log.w(TAG, "Error restoring cookies.", t);
                    }
                }
                return cookies;
            }

            @Override
            protected void onPostExecute(List<CanonicalCookie> cookies) {
                // We can only access cookies and profiles on the UI thread.
                if (offTheRecordProfile.shutdownStarted()) {
                    restoreCompletedAction.run();
                    return;
                }
                for (CanonicalCookie cookie : cookies) {
                    CookiesFetcherJni.get()
                            .restoreCookies(
                                    offTheRecordProfile,
                                    cookie.getName(),
                                    cookie.getValue(),
                                    cookie.getDomain(),
                                    cookie.getPath(),
                                    cookie.getCreationDate(),
                                    cookie.getExpirationDate(),
                                    cookie.getLastAccessDate(),
                                    cookie.getLastUpdateDate(),
                                    cookie.isSecure(),
                                    cookie.isHttpOnly(),
                                    cookie.getSameSite(),
                                    cookie.getPriority(),
                                    cookie.getPartitionKey(),
                                    cookie.sourceScheme(),
                                    cookie.sourcePort(),
                                    cookie.sourceType());
                }

                // The Cookie File should not be restored again. It'll be overwritten on the next
                // onPause.
                scheduleDeleteCookies();
                restoreCompletedAction.run();
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    /** Delete the cookies file. Called when we detect that all incognito tabs have been closed. */
    protected void scheduleDeleteCookies() {
        ThreadUtils.assertOnUiThread();
        boolean isLegacyFileApplicable = isLegacyFileApplicable();
        new BackgroundOnlyAsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                String fileName = fetchAbsoluteFilePath();
                if (fileName != null) {
                    deleteCookeFileInBackground(fileName);
                }

                if (isLegacyFileApplicable) {
                    deleteCookeFileInBackground(fetchLegacyFileName());
                }
                return null;
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    private static void deleteCookeFileInBackground(String fileName) {
        ThreadUtils.assertOnBackgroundThread();
        File cookiesFile = new File(fileName);
        if (cookiesFile.exists()) {
            if (!cookiesFile.delete()) {
                Log.e(TAG, "Failed to delete " + cookiesFile.getName());
            }
        }
    }

    @CalledByNative
    private static CanonicalCookie createCookie(
            String name,
            String value,
            String domain,
            String path,
            long creation,
            long expiration,
            long lastAccess,
            long lastUpdate,
            boolean secure,
            boolean httpOnly,
            int sameSite,
            int priority,
            String partitionKey,
            int sourceScheme,
            int sourcePort,
            int sourceType) {
        return new CanonicalCookie(
                name,
                value,
                domain,
                path,
                creation,
                expiration,
                lastAccess,
                lastUpdate,
                secure,
                httpOnly,
                sameSite,
                priority,
                partitionKey,
                sourceScheme,
                sourcePort,
                sourceType);
    }

    @VisibleForTesting
    @CalledByNative
    void onCookieFetchFinished(final CanonicalCookie[] cookies) {
        // Cookies fetching requires operations with the profile and must be
        // done in the main thread. Once that is done, do the save to disk
        // part in {@link BackgroundOnlyAsyncTask} to avoid strict mode violations.
        new BackgroundOnlyAsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                String fileName = fetchAbsoluteFilePath();
                if (fileName == null) {
                    Log.e(TAG, "Unable to save OTR cookies because file is null");
                    return null;
                }
                saveFetchedCookiesToDisk(fileName, mCipherFactory, cookies);
                return null;
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    @VisibleForTesting
    static void saveFetchedCookiesToDisk(
            String fileName, CipherFactory cipherFactory, CanonicalCookie[] cookies) {
        DataOutputStream out = null;
        try {
            Cipher cipher = cipherFactory.getCipher(Cipher.ENCRYPT_MODE);
            if (cipher == null) {
                // Something is wrong. Can't encrypt, don't save cookies.
                return;
            }

            ByteArrayOutputStream byteOut = new ByteArrayOutputStream();
            CipherOutputStream cipherOut = new CipherOutputStream(byteOut, cipher);
            out = new DataOutputStream(cipherOut);
            CanonicalCookie.saveListToStream(out, cookies);
            out.close();
            ImportantFileWriterAndroid.writeFileAtomically(fileName, byteOut.toByteArray());
            out = null;
        } catch (IOException e) {
            Log.w(TAG, "IOException during Cookie Fetch");
        } catch (Throwable t) {
            Log.w(TAG, "Error storing cookies.", t);
        } finally {
            try {
                if (out != null) out.close();
            } catch (IOException e) {
                Log.w(TAG, "IOException during Cookie Fetch");
            }
        }
    }

    @CalledByNative
    private static CanonicalCookie[] createCookiesArray(int size) {
        return new CanonicalCookie[size];
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        @JniType("std::string")
        String getCookieFileDirectory(@JniType("Profile*") Profile profile);

        void persistCookies(@JniType("Profile*") Profile profile, CookiesFetcher cookiesFetcher);

        void restoreCookies(
                @JniType("Profile*") Profile profile,
                String name,
                String value,
                String domain,
                String path,
                long creation,
                long expiration,
                long lastAccess,
                long lastUpdate,
                boolean secure,
                boolean httpOnly,
                int sameSite,
                int priority,
                String partitionKey,
                int sourceScheme,
                int sourcePort,
                int sourceType);
    }
}
