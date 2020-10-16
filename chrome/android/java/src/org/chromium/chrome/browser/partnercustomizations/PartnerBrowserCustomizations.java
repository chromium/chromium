// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.ProviderInfo;
import android.database.Cursor;
import android.net.Uri;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.version.ChromeVersionInfo;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.ArrayList;
import java.util.List;

/**
 * Reads and caches partner browser customizations information if it exists.
 */
public class PartnerBrowserCustomizations {
    private static final String TAG = "PartnerCustomize";
    private static final String PROVIDER_AUTHORITY = "com.android.partnerbrowsercustomizations";

    /** Default timeout in ms for reading PartnerBrowserCustomizations provider. */
    private static final int DEFAULT_TIMEOUT_MS = 10_000;

    private static final int HOMEPAGE_URL_MAX_LENGTH = 1000;
    // Private homepage structure.
    @VisibleForTesting
    static final String PARTNER_HOMEPAGE_PATH = "homepage";
    @VisibleForTesting
    static final String PARTNER_DISABLE_BOOKMARKS_EDITING_PATH = "disablebookmarksediting";
    @VisibleForTesting
    static final String PARTNER_DISABLE_INCOGNITO_MODE_PATH = "disableincognitomode";

    private static String sProviderAuthority = PROVIDER_AUTHORITY;
    private static Boolean sIgnoreSystemPackageCheck;

    private static volatile PartnerBrowserCustomizations sInstance;

    private volatile String mHomepage;
    private volatile boolean mIncognitoModeDisabled;
    private volatile boolean mBookmarksEditingDisabled;
    private boolean mIsInitialized;

    private final List<Runnable> mInitializeAsyncCallbacks;
    private PartnerHomepageListener mListener;

    /**
     * Interface that listen to homepage URI updates from provider packages.
     */
    public interface PartnerHomepageListener {
        /**
         * Will be called if homepage have any update after {@link #initializeAsync(Context, long)}.
         */
        void onHomepageUpdate();
    }

    /**
     * Provider of partner customizations.
     */
    public interface Provider {
        @Nullable
        String getHomepage();

        boolean isIncognitoModeDisabled();
        boolean isBookmarksEditingDisabled();
    }

    /**
     * Partner customizations provided by ContentProvider package.
     */
    public static class ProviderPackage implements Provider {
        private static Boolean sValid;

        private boolean isValidInternal() {
            ProviderInfo providerInfo =
                    ContextUtils.getApplicationContext().getPackageManager().resolveContentProvider(
                            sProviderAuthority, 0);
            if (providerInfo == null) {
                return false;
            }
            if ((providerInfo.applicationInfo.flags & ApplicationInfo.FLAG_SYSTEM) != 0) {
                return true;
            }

            // In prod mode (sIgnoreBrowserProviderSystemPackageCheck == null), non-system package
            // is rejected unless Chrome Android is a local build.
            // When sIgnoreBrowserProviderSystemPackageCheck is true, accept non-system package.
            // When sIgnoreBrowserProviderSystemPackageCheck is false, reject non-system package.
            if (sIgnoreSystemPackageCheck != null && sIgnoreSystemPackageCheck) {
                return true;
            }

            Log.w(TAG,
                    "Browser Customizations content provider package, " + providerInfo.packageName
                            + ", is not a system package. "
                            + "This could be a malicious attempt from a third party "
                            + "app, so skip reading the browser content provider.");
            if (sIgnoreSystemPackageCheck != null && !sIgnoreSystemPackageCheck) {
                return false;
            }
            if (ChromeVersionInfo.isLocalBuild()) {
                Log.w(TAG,
                        "This is a local build of Chrome Android, "
                                + "so keep reading the browser content provider, "
                                + "to make debugging customization easier.");
                return true;
            }
            return false;
        }

        private boolean isValid() {
            if (sValid == null) {
                sValid = isValidInternal();
            }
            return sValid;
        }

        @Override
        public String getHomepage() {
            if (!isValid()) {
                return null;
            }
            String homepage = null;
            Cursor cursor = ContextUtils.getApplicationContext().getContentResolver().query(
                    buildQueryUri(PARTNER_HOMEPAGE_PATH), null, null, null, null);
            if (cursor != null && cursor.moveToFirst() && cursor.getColumnCount() == 1) {
                homepage = cursor.getString(0);
            }
            if (cursor != null) {
                cursor.close();
            }
            return homepage;
        }

        @Override
        public boolean isIncognitoModeDisabled() {
            if (!isValid()) {
                return false;
            }
            boolean disabled = false;
            Cursor cursor = ContextUtils.getApplicationContext().getContentResolver().query(
                    buildQueryUri(PARTNER_DISABLE_INCOGNITO_MODE_PATH), null, null, null, null);
            if (cursor != null && cursor.moveToFirst() && cursor.getColumnCount() == 1) {
                disabled = cursor.getInt(0) == 1;
            }
            if (cursor != null) {
                cursor.close();
            }
            return disabled;
        }

        @Override
        public boolean isBookmarksEditingDisabled() {
            if (!isValid()) {
                return false;
            }
            boolean disabled = false;
            Cursor cursor = ContextUtils.getApplicationContext().getContentResolver().query(
                    buildQueryUri(PARTNER_DISABLE_BOOKMARKS_EDITING_PATH), null, null, null, null);
            if (cursor != null && cursor.moveToFirst() && cursor.getColumnCount() == 1) {
                disabled = cursor.getInt(0) == 1;
            }
            if (cursor != null) {
                cursor.close();
            }
            return disabled;
        }
    }

    private PartnerBrowserCustomizations() {
        mInitializeAsyncCallbacks = new ArrayList<>();
    }

    /**
     * @return singleton instance of {@link PartnerBrowserCustomizations}.
     */
    public static PartnerBrowserCustomizations getInstance() {
        if (sInstance == null) {
            sInstance = new PartnerBrowserCustomizations();
        }
        return sInstance;
    }

    /**
     * @return True if the partner homepage content provider exists and enabled. Note that The data
     * this method reads is not initialized until the asynchronous initialization of this class has
     * been completed.
     */
    public boolean isHomepageProviderAvailableAndEnabled() {
        return !TextUtils.isEmpty(getHomePageUrl());
    }

    /**
     * @return Whether incognito mode is disabled by the partner.
     */
    @CalledByNative
    public static boolean isIncognitoDisabled() {
        return getInstance().mIncognitoModeDisabled;
    }

    /**
     * Set the listener that will receive updates when partner provided homepage changes.
     * @param listener {@link PartnerHomepageListener} that will receive update when partner
     *         provided homepage changes.
     */
    public void setPartnerHomepageListener(PartnerHomepageListener listener) {
        assert mListener == null;
        mListener = listener;
    }

    /**
     * @return Whether partner bookmarks editing is disabled by the partner.
     */
    public boolean isBookmarksEditingDisabled() {
        return mBookmarksEditingDisabled;
    }

    /**
     * @return True, if initialization is finished. Checking that there is no provider, or failing
     * to read provider is also considered initialization.
     */
    @VisibleForTesting
    public boolean isInitialized() {
        return mIsInitialized;
    }

    @VisibleForTesting
    static void setProviderAuthorityForTests(String providerAuthority) {
        sProviderAuthority = providerAuthority;
    }

    /**
     * For security, we only allow system package to be a browser customizations provider. However,
     * requiring root and installing system apk makes testing harder, so we decided to have this
     * hack for testing. This must not be called other than tests.
     *
     * @param ignore whether we should ignore browser provider system package checking.
     */
    @VisibleForTesting
    static void ignoreBrowserProviderSystemPackageCheckForTests(boolean ignore) {
        sIgnoreSystemPackageCheck = ignore;
    }

    @VisibleForTesting
    static Uri buildQueryUri(String path) {
        return new Uri.Builder()
                .scheme(UrlConstants.CONTENT_SCHEME)
                .authority(sProviderAuthority)
                .appendPath(path)
                .build();
    }

    /**
     * Constructs an async task that reads PartnerBrowserCustomization provider.
     *
     * @param context   The current application context.
     */
    public void initializeAsync(final Context context) {
        initializeAsync(context, DEFAULT_TIMEOUT_MS);
    }

    /**
     * Constructs an async task that reads PartnerBrowserCustomization provider.
     *
     * @param context   The current application context.
     * @param timeoutMs If initializing takes more than this time, cancels it. The unit is ms.
     */
    @VisibleForTesting
    void initializeAsync(final Context context, long timeoutMs) {
        mIsInitialized = false;
        // Setup an initializing async task.
        final AsyncTask<Void> initializeAsyncTask = new AsyncTask<Void>() {
            private boolean mHomepageUriChanged;

            private void refreshHomepage(Provider provider) {
                try {
                    String homepage = provider.getHomepage();
                    if (!isValidHomepage(homepage)) {
                        homepage = null;
                    }
                    if (!TextUtils.equals(mHomepage, homepage)) {
                        mHomepageUriChanged = true;
                    }
                    mHomepage = homepage;
                } catch (Exception e) {
                    Log.w(TAG, "Partner homepage provider URL read failed : ", e);
                }
            }

            private void refreshIncognitoModeDisabled(Provider provider) {
                try {
                    mIncognitoModeDisabled = provider.isIncognitoModeDisabled();
                } catch (Exception e) {
                    Log.w(TAG, "Partner disable incognito mode read failed : ", e);
                }
            }

            private void refreshBookmarksEditingDisabled(Provider provider) {
                try {
                    mBookmarksEditingDisabled = provider.isBookmarksEditingDisabled();
                } catch (Exception e) {
                    Log.w(TAG, "Partner disable bookmarks editing read failed : ", e);
                }
            }

            @Override
            protected Void doInBackground() {
                try {
                    boolean systemOrPreStable =
                            (context.getApplicationInfo().flags & ApplicationInfo.FLAG_SYSTEM) == 1
                            || !ChromeVersionInfo.isStableBuild();
                    if (!systemOrPreStable) {
                        // Only allow partner customization if this browser is a system package, or
                        // is in pre-stable channels.
                        return null;
                    }

                    if (isCancelled()) return null;
                    Provider provider = AppHooks.get().getCustomizationProvider();

                    if (isCancelled()) return null;
                    refreshIncognitoModeDisabled(provider);

                    if (isCancelled()) return null;
                    refreshBookmarksEditingDisabled(provider);

                    if (isCancelled()) return null;
                    refreshHomepage(provider);
                } catch (Exception e) {
                    Log.w(TAG, "Fetching partner customizations failed", e);
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void result) {
                onFinalized();
            }

            @Override
            protected void onCancelled(Void result) {
                onFinalized();
            }

            private void onFinalized() {
                mIsInitialized = true;

                for (Runnable callback : mInitializeAsyncCallbacks) {
                    callback.run();
                }
                mInitializeAsyncCallbacks.clear();

                if (mHomepageUriChanged && mListener != null) {
                    mListener.onHomepageUpdate();
                }
            }
        };

        initializeAsyncTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        // Cancel the initialization if it reaches timeout.
        PostTask.postDelayedTask(
                UiThreadTaskTraits.DEFAULT, () -> initializeAsyncTask.cancel(true), timeoutMs);
    }

    /**
     * Sets a callback that will be executed when the initialization is done.
     *
     * @param callback  This is called when the initialization is done.
     */
    public void setOnInitializeAsyncFinished(final Runnable callback) {
        if (mIsInitialized) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, callback);
        } else {
            mInitializeAsyncCallbacks.add(callback);
        }
    }

    /**
     * Sets a callback that will be executed when the initialization is done.
     *
     * @param callback This is called when the initialization is done.
     * @param timeoutMs If initializing takes more than this time since this function is called,
     * force run |callback| early. The unit is ms.
     */
    public void setOnInitializeAsyncFinished(final Runnable callback, long timeoutMs) {
        mInitializeAsyncCallbacks.add(callback);

        PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, () -> {
            if (mInitializeAsyncCallbacks.remove(callback)) {
                callback.run();
            }
        }, mIsInitialized ? 0 : timeoutMs);
    }

    public static void destroy() {
        getInstance().destroyInternal();
        sInstance = null;
        sIgnoreSystemPackageCheck = null;
    }

    private void destroyInternal() {
        mInitializeAsyncCallbacks.clear();
        mListener = null;
    }

    /**
     * @return Home page URL from Android provider. If null, that means either there is no homepage
     * provider or provider set it to null to disable homepage.
     */
    public String getHomePageUrl() {
        CommandLine commandLine = CommandLine.getInstance();
        if (commandLine.hasSwitch(ChromeSwitches.PARTNER_HOMEPAGE_FOR_TESTING)) {
            return commandLine.getSwitchValue(ChromeSwitches.PARTNER_HOMEPAGE_FOR_TESTING);
        }
        return mHomepage;
    }

    @VisibleForTesting
    static boolean isValidHomepage(String url) {
        if (url == null) {
            return false;
        }
        if (!UrlUtilities.isHttpOrHttps(url) && !UrlUtilities.isNTPUrl(url)) {
            Log.w(TAG,
                    "Partner homepage must be HTTP(S) or NewTabPage. "
                            + "Got invalid URL \"%s\"",
                    url);
            return false;
        }
        if (url.length() > HOMEPAGE_URL_MAX_LENGTH) {
            Log.w(TAG, "The homepage URL \"%s\" is too long.", url);
            return false;
        }
        return true;
    }

    @VisibleForTesting
    public void setHomepageForTests(String homepage) {
        mHomepage = homepage;
    }
}
