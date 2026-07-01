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

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.WebAppLaunchHandlerHistogram.ClientModeAction;
import org.chromium.chrome.browser.customtabs.content.WebAppLaunchHandlerHistogram.FailureReasonAction;
import org.chromium.chrome.browser.customtabs.content.WebAppLaunchHandlerHistogram.FileHandlingAction;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

/**
 * Manages web application launch configurations based on client mode. Provides methods to process
 * client mode and work with launch queue.
 */
@NullMarked
@JNINamespace("webapps")
public class WebAppLaunchHandler extends WebContentsObserver {
    private static final String TAG = "WebAppLaunchHandler";
    private static final @ClientMode int DEFAULT_CLIENT_MODE = NAVIGATE_EXISTING;
    private final WebContents mWebContents;
    private final CustomTabActivityNavigationController mNavigationController;
    private final Verifier mVerifier;
    private final CurrentPageVerifier mCurrentPageVerifier;
    private final Activity mActivity;

    // Tracks the WebContents top-level frame loading state to resolve a race condition between URL
    // verification and navigation completion. LaunchParams are stashed if verification finishes
    // before the page has loaded. They are dispatched to the JS LaunchQueue once loading is
    // complete.
    private boolean mIsPageLoading;

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
            Activity activity) {

        return new WebAppLaunchHandler(
                verifier, currentPageVerifier, navigationController, webContents, activity);
    }

    private WebAppLaunchHandler(
            Verifier verifier,
            CurrentPageVerifier currentPageVerifier,
            CustomTabActivityNavigationController navigationController,
            WebContents webContents,
            Activity activity) {
        mWebContents = webContents;
        mNavigationController = navigationController;
        mVerifier = verifier;
        mCurrentPageVerifier = currentPageVerifier;
        mActivity = activity;
    }

    private boolean isValidFileHandlingData(FileHandlingData fileHandlingData) {
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        for (Uri uri : fileHandlingData.uris) {
            if (!isValidLaunchUri(uri, packageName)) {
                return false;
            }
        }
        return true;
    }

    private static boolean isValidLaunchUri(Uri uri, String packageName) {
        if (uri == null) return false;

        // Only content URIs are allowed. Legitimate file launching on Android should
        // use Content URIs.
        if (!ContentResolver.SCHEME_CONTENT.equalsIgnoreCase(uri.getScheme())) {
            return false;
        }

        // Block Chrome's own Content URIs.
        String authority = uri.getAuthority();
        if (authority != null) {
            String chromeAuthorityPrefix = packageName + ".";
            if (authority
                    .toLowerCase(Locale.US)
                    .startsWith(chromeAuthorityPrefix.toLowerCase(Locale.US))) {
                return false;
            }
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
            @Nullable FileHandlingData fileHandlingData) {
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
        return new WebAppLaunchParams(newNavigationStarted, targetUrl, packageName, fileUris);
    }

    /**
     * Handles an intent that triggers a TWA creation. It doesn't trigger a url loading because in
     * the case of initial intent it has been triggered earlier. Passes launch params to a launch
     * queue. Always uses startNewNavigation = true because opening new custom tab is always url
     * loading. In case if the target url provided in the intentDataProvider is out of scope of the
     * TWA a launch queue will not be notified.
     *
     * @param intentDataProvider Provides incoming intent with Custom Tabs specific customization
     *     data.
     */
    public void handleInitialIntent(BrowserServicesIntentDataProvider intentDataProvider) {
        WebAppLaunchHandlerHistogram.logClientMode(ClientModeAction.INITIAL_INTENT);

        FileHandlingData filteredData = filterFileHandlingData(intentDataProvider);
        WebAppLaunchParams launchParams =
                getLaunchParams(
                        /* newNavigationStarted= */ true,
                        assertNonNull(intentDataProvider.getUrlToLoad()),
                        assertNonNull(intentDataProvider.getClientPackageName()),
                        filteredData);

        maybeNotifyLaunchQueue(launchParams);
    }

    /**
     * Handles an intent that comes after a TWA creation. It triggers a url loading in case of
     * navigate-existing mode. It opens a new TWA in a new task in a case of navigate-new mode. It
     * Passes launch params to a launch queue. In case if the target url provided in the
     * intentDataProvider is out of scope of the TWA a launch queue will not be notified. If
     * currently open page is out of scope of the TWA and it's not going to be reloaded a launch
     * queue will not be notified as well.
     *
     * @param intentDataProvider Provides incoming intent with Custom Tabs specific customization
     *     data.
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
        if (clientMode == NAVIGATE_NEW
                || state == null
                || state.status != CurrentPageVerifier.VerificationStatus.SUCCESS) {
            launchNewIntent(urlToLoad, packageName, filteredData);
        } else {
            boolean startNavigation =
                    clientMode == NAVIGATE_EXISTING && !TextUtils.isEmpty(urlToLoad);

            if (startNavigation) {
                LoadUrlParams params = new LoadUrlParams(urlToLoad);
                mNavigationController.navigate(
                        params, assumeNonNull(intentDataProvider.getIntent()));
            }

            assert packageName != null;
            WebAppLaunchParams launchParams =
                    getLaunchParams(startNavigation, urlToLoad, packageName, filteredData);

            maybeNotifyLaunchQueue(launchParams);
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

    private void maybeNotifyLaunchQueue(WebAppLaunchParams launchParams) {

        if (!launchParams.newNavigationStarted) {
            // Check if the URL of the current page is in the web app scope.
            // Launch params should not be sent to a not verified origin.
            CurrentPageVerifier.VerificationState state = mCurrentPageVerifier.getState();
            if (state == null || state.status != CurrentPageVerifier.VerificationStatus.SUCCESS) {
                WebAppLaunchHandlerHistogram.logFailureReason(
                        FailureReasonAction.CURRENT_PAGE_VERIFICATION_FAILED);
                Log.w(TAG, "Current page verification has been failed.");
                return;
            }

            WebAppLaunchHandlerJni.get()
                    .notifyLaunchQueue(
                            mWebContents,
                            false,
                            launchParams.targetUrl,
                            launchParams.packageName,
                            launchParams.fileUris);
            return;
        }

        observe(mWebContents);
        mVerifier
                .verify(launchParams.targetUrl)
                .then(
                        (verified) -> {
                            observe(null);

                            if (!verified) {
                                WebAppLaunchHandlerHistogram.logFailureReason(
                                        FailureReasonAction.TARGET_URL_VERIFICATION_FAILED);
                                Log.w(TAG, "Target url verification has been failed.");
                                return;
                            }

                            if (mWebContents == null || mWebContents.isDestroyed()) {
                                Log.w(TAG, "Web contents was destroyed.");
                                return;
                            }

                            WebAppLaunchHandlerJni.get()
                                    .notifyLaunchQueue(
                                            mWebContents,
                                            mIsPageLoading,
                                            launchParams.targetUrl,
                                            launchParams.packageName,
                                            launchParams.fileUris);
                        });
    }

    @Override
    public void didStartNavigationInPrimaryMainFrame(NavigationHandle navigationHandle) {
        mIsPageLoading = true;
    }

    @Override
    public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigationHandle) {
        mIsPageLoading = false;
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
            if (doesCallerHavePermissionForUri(intentDataProvider.getSession(), uri)) {
                filteredUris.add(uri);
            } else {
                Log.w(TAG, "Caller does not have permission for URI: " + uri);
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
    private boolean doesCallerHavePermissionForUri(@Nullable SessionHolder<?> session, Uri uri) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            try {
                var caller = mActivity.getCurrentCaller();
                if (caller != null) {
                    return caller.checkContentUriPermission(
                                    uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
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
                    return mActivity.checkUriPermission(
                                    uri, pid, uid, Intent.FLAG_GRANT_READ_URI_PERMISSION)
                            == PackageManager.PERMISSION_GRANTED;
                } catch (Exception e) {
                    Log.w(TAG, "Failed to check URI permission for UID: " + uid, e);
                }
            }
        }
        return false;
    }

    /**
     * Takes the WebContents object of the tab that is being launched and notifies the launch queue
     * with this object and associated launch parameters.
     */
    @NativeMethods
    public interface Natives {
        void notifyLaunchQueue(
                @JniType("content::WebContents*") WebContents webContents,
                @JniType("bool") boolean startNewNavigation,
                @JniType("std::string") String startUrl,
                @JniType("std::string") String packageName,
                @JniType("std::vector<std::string>") String[] fileUris);
    }
}
