// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static androidx.browser.trusted.LaunchHandlerClientMode.AUTO;
import static androidx.browser.trusted.LaunchHandlerClientMode.FOCUS_EXISTING;
import static androidx.browser.trusted.LaunchHandlerClientMode.NAVIGATE_EXISTING;
import static androidx.browser.trusted.LaunchHandlerClientMode.NAVIGATE_NEW;
import static androidx.browser.trusted.TrustedWebActivityIntentBuilder.EXTRA_FILE_HANDLING_DATA;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ContentResolver;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;

import androidx.browser.trusted.FileHandlingData;
import androidx.browser.trusted.LaunchHandlerClientMode.ClientMode;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.WebAppLaunchHandlerHistogram.ClientModeAction;
import org.chromium.chrome.browser.customtabs.content.WebAppLaunchHandlerHistogram.FailureReasonAction;
import org.chromium.chrome.browser.customtabs.content.WebAppLaunchHandlerHistogram.FileHandlingAction;
import org.chromium.chrome.browser.renderer_host.ChromeNavigationUiData;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Manages web application launch configurations based on client mode. Provides methods to process
 * client mode and work with launch queue.
 */
@NullMarked
@JNINamespace("webapps")
public class WebAppLaunchHandler {
    private static final String TAG = "WebAppLaunchHandler";
    private static final @ClientMode int DEFAULT_CLIENT_MODE = NAVIGATE_EXISTING;
    private final WebContents mWebContents;
    private final CustomTabActivityNavigationController mNavigationController;
    private final Verifier mVerifier;
    private final CurrentPageVerifier mCurrentPageVerifier;
    private final Activity mActivity;
    private final CustomTabActivityTabProvider mTabProvider;

    private static long sNextLaunchToken;

    /**
     * Retrieves the ClientMode enum value from a given AndroidX enum. Defaults to
     * DEFAULT_CLIENT_MODE if the value is invalid or AUTO.
     *
     * @param clientMode The AndroidX representation of the client mode.
     * @return The corresponding ClientMode enum value.
     */
    public static @ClientMode int getClientMode(@ClientMode int clientMode) {
        if (Arrays.asList(NAVIGATE_EXISTING, FOCUS_EXISTING, NAVIGATE_NEW).contains(clientMode)) {
            return clientMode;
        } else {
            return DEFAULT_CLIENT_MODE;
        }
    }

    /**
     * Creates a new instance of {@link WebAppLaunchHandler}.
     *
     * @param verifier The {@link Verifier} to use for verifying the target url.
     * @param currentPageVerifier The {@link CurrentPageVerifier} to use for verifying the current
     *     page.
     * @param navigationController The {@link CustomTabActivityNavigationController} to handle
     *     navigation within the Custom Tab.
     * @param webContents The {@link WebContents} associated with the tab.
     * @param activity The {@link Activity} associated with the tab.
     * @return A new {@link WebAppLaunchHandler} instance.
     */
    public static WebAppLaunchHandler create(
            Verifier verifier,
            CurrentPageVerifier currentPageVerifier,
            CustomTabActivityNavigationController navigationController,
            WebContents webContents,
            Activity activity,
            CustomTabActivityTabProvider tabProvider) {

        return new WebAppLaunchHandler(
                verifier,
                currentPageVerifier,
                navigationController,
                webContents,
                activity,
                tabProvider);
    }

    private WebAppLaunchHandler(
            Verifier verifier,
            CurrentPageVerifier currentPageVerifier,
            CustomTabActivityNavigationController navigationController,
            WebContents webContents,
            Activity activity,
            CustomTabActivityTabProvider tabProvider) {
        mWebContents = webContents;
        mNavigationController = navigationController;
        mVerifier = verifier;
        mCurrentPageVerifier = currentPageVerifier;
        mActivity = activity;
        mTabProvider = tabProvider;
    }

    private boolean isValidFileHandlingData(FileHandlingData fileHandlingData) {
        for (Uri uri : fileHandlingData.uris) {
            if (!isValidLaunchUri(uri)) {
                return false;
            }
        }
        return true;
    }

    private static boolean isValidLaunchUri(Uri uri) {
        if (uri == null) return false;

        // Only content URIs are allowed. Legitimate file launching on Android should
        // use Content URIs.
        if (!ContentResolver.SCHEME_CONTENT.equalsIgnoreCase(uri.getScheme())) {
            return false;
        }

        // Block Chrome's own Content URIs.
        if (ContentUriUtils.isUriFromThisApp(uri)) {
            return false;
        }

        return true;
    }

    /**
     * Generates WebAppLaunchParams based on the AndroidX representation of the client mode.
     *
     * @param newNavigationStarted Whether this launch triggered a navigation.
     * @param targetUrl The URL to launch.
     * @param packageName Android package name of the web app is being launched.
     * @param fileHandlingData Files to be opened for a case it's file open launch. Optional param.
     * @return The generated WebAppLaunchParams object.
     */
    private WebAppLaunchParams getLaunchParams(
            boolean newNavigationStarted,
            String targetUrl,
            String packageName,
            @Nullable FileHandlingData fileHandlingData,
            @Nullable SessionHolder<?> session) {
        List<Uri> fileUris = null;
        @FileHandlingAction int action = FileHandlingAction.NO_FILES;

        if (fileHandlingData != null
                && !fileHandlingData.uris.isEmpty()
                && isValidFileHandlingData(fileHandlingData)) {
            fileUris = fileHandlingData.uris;
            action =
                    fileUris.size() == 1
                            ? FileHandlingAction.SINGLE_FILE
                            : FileHandlingAction.MULTIPLE_FILES;
        }

        WebAppLaunchHandlerHistogram.logFileHandling(action);
        boolean[] canWrite;
        if (fileUris != null) {
            canWrite = new boolean[fileUris.size()];
            for (int i = 0; i < fileUris.size(); i++) {
                canWrite[i] =
                        doesCallerHavePermissionForUri(
                                session, fileUris.get(i), Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            }
        } else {
            canWrite = new boolean[0];
        }
        return new WebAppLaunchParams(
                newNavigationStarted, targetUrl, packageName, fileUris, canWrite);
    }

    /**
     * Handles an intent that triggers a TWA creation (cold start). It doesn't trigger a url loading
     * because the initial navigation is initiated elsewhere in CCT startup. Generates a token to
     * correlate this launch with C++ navigation events.
     *
     * <p>Detects if this launch reused a speculative navigation (started via mayLaunchUrl) and
     * passes this status to C++.
     *
     * @param intentDataProvider Provides incoming intent data.
     * @return The launch token if a navigation correlation is needed, or null if the launch was
     *     invalid or did not require navigation.
     */
    public @Nullable Long handleInitialIntent(
            BrowserServicesIntentDataProvider intentDataProvider) {
        WebAppLaunchHandlerHistogram.logClientMode(ClientModeAction.INITIAL_INTENT);

        FileHandlingData filteredData = filterFileHandlingData(intentDataProvider);
        String urlToLoad = assertNonNull(intentDataProvider.getUrlToLoad());
        WebAppLaunchParams launchParams =
                getLaunchParams(
                        /* newNavigationStarted= */ true,
                        urlToLoad,
                        assertNonNull(intentDataProvider.getClientPackageName()),
                        filteredData,
                        intentDataProvider.getSession());

        boolean isHidden = mTabProvider.getInitialTabCreationMode() == TabCreationMode.HIDDEN;
        boolean hasSpeculativeNavigation = false;
        if (isHidden) {
            String speculatedUrl = mTabProvider.getSpeculatedUrl();
            hasSpeculativeNavigation = TextUtils.equals(speculatedUrl, urlToLoad);
        }

        return maybeNotifyLaunchQueue(launchParams, hasSpeculativeNavigation);
    }

    /**
     * Handles an intent that comes after TWA creation (warm start, reusing existing tab).
     *
     * <p>Triggers a navigation in the existing tab if the client mode is NAVIGATE_EXISTING, or if
     * it is FOCUS_EXISTING but the current page is out of scope (fallback to navigate-existing
     * behavior). Otherwise, delivers the parameters directly to the current page without
     * navigating.
     *
     * <p>Detects if there is a speculative navigation in progress in the existing tab (via
     * mayLaunchUrl) and passes this status to C++.
     *
     * @param intentDataProvider Provides incoming intent data.
     */
    public void handleNewIntent(BrowserServicesIntentDataProvider intentDataProvider) {
        @ClientMode int clientModeFromIntent = intentDataProvider.getLaunchHandlerClientMode();
        recordClientMode(clientModeFromIntent);
        @ClientMode int clientMode = getClientMode(clientModeFromIntent);

        String urlToLoad = intentDataProvider.getUrlToLoad();
        assert urlToLoad != null;
        String packageName = intentDataProvider.getClientPackageName();

        FileHandlingData filteredData = filterFileHandlingData(intentDataProvider);

        CurrentPageVerifier.VerificationState state = mCurrentPageVerifier.getState();
        // If the current page is not fully verified (including if verification is still PENDING),
        // we fallback to starting a new navigation. We choose not to wait for PENDING verification
        // here to keep intent delivery simple and avoid stashing intents. Since the page is
        // already loaded, verification should usually be complete and cached; if it is still
        // pending, navigating is the safer fallback.
        if (clientMode == NAVIGATE_NEW
                || state == null
                || state.status != CurrentPageVerifier.VerificationStatus.SUCCESS) {
            launchNewIntent(urlToLoad, packageName, filteredData);
        } else {
            String currentUrl = mWebContents.getLastCommittedUrl().getSpec();
            String scopeUrl = getScopeUrl(urlToLoad);
            boolean isInScope = UrlUtilities.isUrlWithinScope(currentUrl, scopeUrl);

            // Note: This models the default behavior for Android, which is to default to
            // navigate-existing rather than navigate-new when launching an app with a url.
            // This behavior might change in the future.
            boolean startNavigation =
                    (clientMode == NAVIGATE_EXISTING
                                    || (clientMode == FOCUS_EXISTING && !isInScope))
                            && !TextUtils.isEmpty(urlToLoad);

            assert packageName != null;
            WebAppLaunchParams launchParams =
                    getLaunchParams(
                            startNavigation,
                            urlToLoad,
                            packageName,
                            filteredData,
                            intentDataProvider.getSession());

            String speculatedUrl = mTabProvider.getSpeculatedUrl();
            boolean hasSpeculativeNavigation = TextUtils.equals(speculatedUrl, urlToLoad);
            Long token = maybeNotifyLaunchQueue(launchParams, hasSpeculativeNavigation);

            if (startNavigation) {
                LoadUrlParams params = new LoadUrlParams(urlToLoad);
                if (token != null) {
                    ChromeNavigationUiData.getOrCreate(params).setTwaLaunchToken(token);
                }
                mNavigationController.navigate(
                        params, assumeNonNull(intentDataProvider.getIntent()));
            }
        }
    }

    private void recordClientMode(@ClientMode int clientMode) {
        switch (clientMode) {
            case NAVIGATE_EXISTING:
                WebAppLaunchHandlerHistogram.logClientMode(ClientModeAction.MODE_NAVIGATE_EXISTING);
                break;
            case FOCUS_EXISTING:
                WebAppLaunchHandlerHistogram.logClientMode(ClientModeAction.MODE_FOCUS_EXISTING);
                break;
            case NAVIGATE_NEW:
                WebAppLaunchHandlerHistogram.logClientMode(ClientModeAction.MODE_NAVIGATE_NEW);
                break;
            case AUTO:
                WebAppLaunchHandlerHistogram.logClientMode(ClientModeAction.MODE_AUTO);
                break;
        }
    }

    /**
     * Launches a new instance of TWA in a separate task. In order to support navigate-new client
     * mode we need to support several running instances of the same TWA app simultaneously in
     * separate tasks. If client_mode is navigate-new we will resend an intent with action VIEW to
     * create one more running instance of the TWA app. We achieve it adding FLAG_ACTIVITY_NEW_TASK
     * and FLAG_ACTIVITY_MULTIPLE_TASK to the new intent
     *
     * @param targetUrl The URL the web app was launched with
     * @param packageName Chrome will take a package name from the TWA session to ensure the intent
     *     is sent to the application it is received from
     * @param fileData The list of file URIs, if the web app was launched by opening one or multiple
     *     files
     */
    private void launchNewIntent(
            String targetUrl, @Nullable String packageName, @Nullable FileHandlingData fileData) {
        if (packageName == null) {
            return;
        }

        Intent newIntent = new Intent();
        newIntent.setAction(Intent.ACTION_VIEW);
        newIntent.setData(Uri.parse(targetUrl));
        newIntent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        newIntent.setPackage(packageName);

        /* This method can be called for file handling intent as well. In this case we need to send
        a file data extras as well. Also we need to grant file permissions */
        if (fileData != null && !fileData.uris.isEmpty()) {
            newIntent.putExtra(EXTRA_FILE_HANDLING_DATA, fileData.toBundle());
        }

        try {
            mActivity.startActivity(newIntent);
        } catch (ActivityNotFoundException exception) {
            Log.w(TAG, "Couldn't start new activity in a separate task.");
        }
    }

    private @Nullable Long maybeNotifyLaunchQueue(
            WebAppLaunchParams launchParams, boolean hasSpeculativeNavigation) {
        String scopeUrl = getScopeUrl(launchParams.targetUrl);

        if (!launchParams.newNavigationStarted) {
            CurrentPageVerifier.VerificationState state = mCurrentPageVerifier.getState();
            if (state == null || state.status != CurrentPageVerifier.VerificationStatus.SUCCESS) {
                WebAppLaunchHandlerHistogram.logFailureReason(
                        FailureReasonAction.CURRENT_PAGE_VERIFICATION_FAILED);
                Log.w(TAG, "Current page verification has been failed.");
                return null;
            }

            // We cannot guarantee that another navigation won't occur between this
            // Java-side verification check and the actual delivery in C++. As a
            // safeguard, C++ will perform a final scope check against the last
            // committed URL before enqueuing.
            WebAppLaunchHandlerJni.get()
                    .enqueueNonNavigating(
                            mWebContents,
                            launchParams.targetUrl,
                            launchParams.packageName,
                            launchParams.fileUris,
                            launchParams.canWrite,
                            scopeUrl);
            return null;
        }

        long token = ++sNextLaunchToken;
        WebAppLaunchHandlerJni.get()
                .prepareForLaunch(
                        mWebContents,
                        token,
                        launchParams.targetUrl,
                        launchParams.packageName,
                        launchParams.fileUris,
                        launchParams.canWrite,
                        scopeUrl,
                        hasSpeculativeNavigation);

        mVerifier
                .verify(launchParams.targetUrl)
                .then(
                        (verified) -> {
                            if (!verified) {
                                WebAppLaunchHandlerHistogram.logFailureReason(
                                        FailureReasonAction.TARGET_URL_VERIFICATION_FAILED);
                                Log.w(TAG, "Target url verification has been failed.");
                            }

                            if (mWebContents == null || mWebContents.isDestroyed()) {
                                Log.w(TAG, "Web contents was destroyed.");
                                return;
                            }

                            WebAppLaunchHandlerJni.get()
                                    .onLaunchVerified(mWebContents, token, verified);
                        });
        return token;
    }

    /**
     * Filters incoming file handling data to retain only URIs that the launching client app has
     * permission to access.
     *
     * @param intentDataProvider Provides incoming intent and session customization data.
     * @return The filtered FileHandlingData object containing authorized URIs, or null if all URIs
     *     were denied or no file data was provided.
     */
    private @Nullable FileHandlingData filterFileHandlingData(
            BrowserServicesIntentDataProvider intentDataProvider) {
        FileHandlingData fileHandlingData = intentDataProvider.getFileHandlingData();
        if (fileHandlingData == null || fileHandlingData.uris.isEmpty()) {
            return null;
        }

        List<Uri> filteredUris = new ArrayList<>();
        for (Uri uri : fileHandlingData.uris) {
            if (doesCallerHavePermissionForUri(
                    intentDataProvider.getSession(), uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)) {
                filteredUris.add(uri);
            } else {
                Log.w(TAG, "Caller does not have read permission for URI: " + uri);
            }
        }

        if (filteredUris.isEmpty()) {
            return null;
        }
        if (filteredUris.size() == fileHandlingData.uris.size()) {
            return fileHandlingData;
        }
        return new FileHandlingData(filteredUris);
    }

    /**
     * Verifies whether the calling application holds read permission for the specified URI.
     *
     * <p>On Android 15+ (API 35+), checks caller identity via {@link Activity#getCurrentCaller()}.
     * On older Android versions, falls back to verifying URI permissions against the session UID.
     *
     * @param session The session holder associated with the launching client app.
     * @param uri The Content URI to verify.
     * @return True if the caller has explicit read permission for uri, false otherwise.
     */
    @SuppressLint("NewApi")
    private boolean doesCallerHavePermissionForUri(
            @Nullable SessionHolder<?> session, Uri uri, int requestedPermission) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            try {
                var caller = mActivity.getCurrentCaller();
                if (caller != null) {
                    return caller.checkContentUriPermission(uri, requestedPermission)
                            == PackageManager.PERMISSION_GRANTED;
                }
            } catch (Exception e) {
                Log.w(TAG, "Failed to check caller's permission via getCurrentCaller.", e);
                return false;
            }
        }

        // Fallback for Android versions prior to Android 15 (API < 35) or when getCurrentCaller()
        // is unavailable. We check URI read permissions against the client UID and PID recorded
        // when the TWA session was established.
        if (session != null) {
            int uid = CustomTabsConnection.getInstance().getClientUidForSession(session);
            int pid = CustomTabsConnection.getInstance().getClientPidForSession(session);
            if (uid != -1) {
                try {
                    return mActivity.checkUriPermission(uri, pid, uid, requestedPermission)
                            == PackageManager.PERMISSION_GRANTED;
                } catch (Exception e) {
                    Log.w(TAG, "Failed to check URI permission for UID: " + uid, e);
                }
            }
        }
        return false;
    }

    private String getScopeUrl(String url) {
        String scopeUrl = ShortcutHelper.getScopeFromUrl(url);
        if (TextUtils.isEmpty(scopeUrl)) {
            scopeUrl =
                    Uri.parse(url)
                            .buildUpon()
                            .path("")
                            .clearQuery()
                            .fragment(null)
                            .build()
                            .toString();
        }
        return scopeUrl;
    }

    /**
     * Takes the WebContents object of the tab that is being launched and notifies the launch queue
     * with this object and associated launch parameters.
     */
    @NativeMethods
    public interface Natives {
        void prepareForLaunch(
                @JniType("content::WebContents*") WebContents webContents,
                long launchToken,
                @JniType("std::string") String startUrl,
                @JniType("std::string") String packageName,
                @JniType("std::vector<std::string>") String[] fileUris,
                @JniType("std::vector<bool>") boolean[] canWrite,
                @JniType("std::string") String scopeUrl,
                boolean hasSpeculativeNavigation);

        void onLaunchVerified(
                @JniType("content::WebContents*") WebContents webContents,
                long launchToken,
                boolean success);

        void enqueueNonNavigating(
                @JniType("content::WebContents*") WebContents webContents,
                @JniType("std::string") String startUrl,
                @JniType("std::string") String packageName,
                @JniType("std::vector<std::string>") String[] fileUris,
                @JniType("std::vector<bool>") boolean[] canWrite,
                @JniType("std::string") String scopeUrl);
    }
}
