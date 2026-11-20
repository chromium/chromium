// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static androidx.browser.trusted.LaunchHandlerClientMode.AUTO;
import static androidx.browser.trusted.LaunchHandlerClientMode.FOCUS_EXISTING;
import static androidx.browser.trusted.LaunchHandlerClientMode.NAVIGATE_EXISTING;
import static androidx.browser.trusted.LaunchHandlerClientMode.NAVIGATE_NEW;
import static androidx.browser.trusted.TrustedWebActivityIntentBuilder.EXTRA_FILE_HANDLING_DATA;
import static androidx.browser.trusted.TrustedWebActivityIntentBuilder.EXTRA_SHARE_DATA;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ComponentCaller;
import android.content.ActivityNotFoundException;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.os.UserHandle;
import android.text.TextUtils;

import androidx.browser.customtabs.TrustedWebUtils;
import androidx.browser.trusted.FileHandlingData;
import androidx.browser.trusted.LaunchHandlerClientMode.ClientMode;
import androidx.browser.trusted.sharing.ShareData;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContentUriOwner;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.WebAppLaunchHandlerHistogram.ClientModeAction;
import org.chromium.chrome.browser.customtabs.content.WebAppLaunchHandlerHistogram.FailureReasonAction;
import org.chromium.chrome.browser.customtabs.content.WebAppLaunchHandlerHistogram.FileHandlingAction;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.renderer_host.ChromeNavigationUiData;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;

/**
 * Manages web application launch configurations based on client mode. Provides methods to process
 * client mode and work with launch queue.
 */
@NullMarked
@JNINamespace("webapps")
public class WebAppLaunchHandler {
    private static final String TAG = "WebAppLaunchHandler";
    private static final @ClientMode int DEFAULT_CLIENT_MODE = NAVIGATE_EXISTING;
    // Pre-Android 14 UID constants matching AOSP android.os.UserHandle / android.os.Process.
    private static final int PER_USER_RANGE = 100000;
    private static final int FIRST_APP_ZYGOTE_ISOLATED_UID = 90000;
    private static final int LAST_ISOLATED_UID = 99999;
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

    public static boolean isValidLaunchUri(Uri uri) {
        if (uri == null) return false;

        // Only content URIs are allowed. Legitimate file launching on Android should
        // use Content URIs.
        if (!ContentResolver.SCHEME_CONTENT.equalsIgnoreCase(uri.getScheme())) {
            return false;
        }

        // Block Chrome's own Content URIs.
        @ContentUriOwner int owner = ContentUriUtils.resolveContentUriOwner(uri);
        if (owner == ContentUriOwner.EXPORTED_FROM_CURRENT_APP
                || owner == ContentUriOwner.NON_EXPORTED_FROM_CURRENT_APP) {
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
            @Nullable SessionHolder session,
            @Nullable Intent intent,
            @Nullable Object caller) {
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
        boolean[] canWrite = null;
        if (fileUris != null) {
            if (intent != null) {
                canWrite =
                        intent.getBooleanArrayExtra(
                                CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_CAN_WRITE);
            }
            if (canWrite == null || canWrite.length != fileUris.size()) {
                canWrite = new boolean[fileUris.size()];
                for (int i = 0; i < fileUris.size(); i++) {
                    canWrite[i] =
                            doesCallerHavePermissionForUri(
                                    mActivity,
                                    caller,
                                    session,
                                    fileUris.get(i),
                                    Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                }
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

        Object caller = null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            try {
                caller = mActivity.getInitialCaller();
            } catch (Exception e) {
                Log.w(TAG, "Failed to get initial caller. Falling back.", e);
            }
        }

        FileHandlingData filteredData = filterFileHandlingData(intentDataProvider, caller);
        String urlToLoad = assertNonNull(intentDataProvider.getUrlToLoad());
        WebAppLaunchParams launchParams =
                getLaunchParams(
                        /* newNavigationStarted= */ true,
                        urlToLoad,
                        assertNonNull(intentDataProvider.getClientPackageName()),
                        filteredData,
                        intentDataProvider.getSession(),
                        intentDataProvider.getIntent(),
                        caller);

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

        Object caller = null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            try {
                caller = mActivity.getCurrentCaller();
            } catch (Exception e) {
                Log.w(TAG, "Failed to get current caller. Falling back.", e);
            }
        }

        FileHandlingData filteredData = filterFileHandlingData(intentDataProvider, caller);

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
                            intentDataProvider.getSession(),
                            intentDataProvider.getIntent(),
                            caller);

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
        // Asynchronous lifecycle events can destroy the WebContents before the intent
        // is fully processed and delivered to the launch queue.
        if (mWebContents.isDestroyed()) {
            return null;
        }
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
            BrowserServicesIntentDataProvider intentDataProvider, @Nullable Object caller) {
        Intent intent = intentDataProvider.getIntent();
        if (intent != null) {
            Bundle verifiedBundle =
                    IntentUtils.safeGetBundleExtra(
                            intent, CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_HANDLING_DATA);
            if (verifiedBundle != null) {
                try {
                    return FileHandlingData.fromBundle(verifiedBundle);
                } catch (Throwable e) {
                    Log.w(TAG, "Failed to unparcel verified file handling data", e);
                }
            }
        }

        FileHandlingData fileHandlingData = intentDataProvider.getFileHandlingData();
        if (fileHandlingData == null || fileHandlingData.uris.isEmpty()) {
            return null;
        }

        List<Uri> filteredUris = new ArrayList<>();
        for (Uri uri : fileHandlingData.uris) {
            if (!isValidLaunchUri(uri)) {
                Log.w(TAG, "Invalid launch URI: %s", uri);
                continue;
            }
            if (!doesCallerHavePermissionForUri(
                    mActivity,
                    caller,
                    intentDataProvider.getSession(),
                    uri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION)) {
                Log.w(TAG, "Caller does not have read permission for URI: %s", uri);
                continue;
            }
            filteredUris.add(uri);
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
     * <p>On Android 15+ (API 35+), checks caller identity via {@link ComponentCaller}. On older
     * Android versions or when ComponentCaller is unavailable, falls back to verifying URI
     * permissions against the client UID.
     *
     * @param activity The activity context.
     * @param caller The caller object (ComponentCaller on Android 15+).
     * @param clientUid The UID of the launching client application (-1 if unknown).
     * @param clientPid The PID of the launching client application (0 if unknown).
     * @param uri The Content URI to verify.
     * @param requestedPermission The permission flag (e.g. FLAG_GRANT_READ_URI_PERMISSION).
     * @return True if the caller has permission for uri, false otherwise.
     */
    @SuppressLint("NewApi")
    public static boolean doesCallerHavePermissionForUri(
            Activity activity,
            @Nullable Object caller,
            int clientUid,
            int clientPid,
            @Nullable Uri uri,
            int requestedPermission) {
        if (uri == null || !ContentResolver.SCHEME_CONTENT.equals(uri.getScheme())) {
            return false;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM && caller != null) {
            try {
                ComponentCaller componentCaller = (ComponentCaller) caller;
                int callerUid = componentCaller.getUid();
                if (callerUid == Process.myUid()) {
                    Log.d(
                            TAG,
                            "Caller is ourselves (trampoline launch). Falling back to UID check.");
                } else {
                    if (callerUid != Process.INVALID_UID) {
                        if (clientUid != -1 && clientUid != callerUid) {
                            Log.w(
                                    TAG,
                                    "Session UID (%d) does not match caller UID (%d).",
                                    clientUid,
                                    callerUid);
                            return false;
                        }
                        // Update fallback identity in case checkContentUriPermission below throws
                        // IllegalArgumentException (for custom Bundle URIs) and falls through to
                        // checkContentUriPermissionFull; -1 is the UID-only sentinel when PID is
                        // unknown.
                        clientUid = callerUid;
                        clientPid = -1;
                    }
                    return componentCaller.checkContentUriPermission(uri, requestedPermission)
                            == PackageManager.PERMISSION_GRANTED;
                }
            } catch (SecurityException e) {
                // Thrown when Chrome itself lacks access to the URI; deny immediately rather than
                // falling back to a potentially different package/session UID.
                Log.w(TAG, "Chrome lacks permission to check caller URI access.", e);
                return false;
            } catch (Exception e) {
                // ComponentCaller#checkContentUriPermission throws IllegalArgumentException when
                // the URI was passed in a custom Bundle extra (such as EXTRA_FILE_HANDLING_DATA or
                // EXTRA_SHARE_DATA) rather than Intent#getData, EXTRA_STREAM, or ClipData.
                Log.w(
                        TAG,
                        "Failed to check caller's permission via ComponentCaller. Falling back.",
                        e);
            }
        }

        if (clientUid >= 0) {
            try {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
                    return activity.checkContentUriPermissionFull(
                                    uri, clientPid, clientUid, requestedPermission)
                            == PackageManager.PERMISSION_GRANTED;
                }
                if (activity.checkUriPermission(uri, clientPid, clientUid, requestedPermission)
                        == PackageManager.PERMISSION_GRANTED) {
                    return true;
                }
                return doesCallerHoldProviderPermission(
                        activity, uri, clientPid, clientUid, requestedPermission);
            } catch (Exception e) {
                Log.w(TAG, "Failed to check URI permission for UID: %d", clientUid, e);
            }
        }
        return false;
    }

    /**
     * Pre-Android 15 fallback that checks whether the calling application inherently holds
     * permission to access the URI's {@link android.content.ContentProvider} without requiring an
     * explicit dynamic {@code UriPermission} grant record (for example, when the provider is
     * exported with no {@code readPermission}, or when the caller owns the provider).
     */
    private static boolean doesCallerHoldProviderPermission(
            Context context, Uri uri, int pid, int uid, int modeFlags) {
        int accessModes =
                modeFlags
                        & (Intent.FLAG_GRANT_READ_URI_PERMISSION
                                | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        if (accessModes == 0 || (modeFlags & ~accessModes) != 0) {
            return false;
        }
        if (!UserHandle.getUserHandleForUid(uid).equals(Process.myUserHandle())
                || isIsolatedUid(uid)) {
            return false;
        }

        String authority = uri.getAuthority();
        // Reject missing authorities, cross-user authority prefixes (e.g. "10@authority", since
        // resolveContentProvider only resolves providers in the current user), and crafted URIs
        // whose authority changes when reparsed from string form (e.g. OpaqueUri).
        if (TextUtils.isEmpty(authority)
                || authority.indexOf('@') != -1
                || !authority.equals(Uri.parse(uri.toString()).getAuthority())) {
            return false;
        }

        ProviderInfo pi = context.getPackageManager().resolveContentProvider(authority, 0);
        if (pi == null || pi.applicationInfo == null) return false;
        if (pi.applicationInfo.uid == Process.myUid()) return false;
        if (pi.applicationInfo.uid == uid) return true;
        if (!pi.exported || pi.forceUriPermissions || pi.pathPermissions != null) {
            return false;
        }
        if ((accessModes & Intent.FLAG_GRANT_READ_URI_PERMISSION) != 0
                && pi.readPermission != null
                && context.checkPermission(pi.readPermission, pid, uid)
                        != PackageManager.PERMISSION_GRANTED) {
            return false;
        }
        if ((accessModes & Intent.FLAG_GRANT_WRITE_URI_PERMISSION) != 0
                && pi.writePermission != null
                && context.checkPermission(pi.writePermission, pid, uid)
                        != PackageManager.PERMISSION_GRANTED) {
            return false;
        }
        return true;
    }

    private static boolean isIsolatedUid(int uid) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            return Process.isIsolatedUid(uid);
        }
        int appId = uid % PER_USER_RANGE;
        return appId >= FIRST_APP_ZYGOTE_ISOLATED_UID && appId <= LAST_ISOLATED_UID;
    }

    /**
     * Convenience overload of {@link #doesCallerHavePermissionForUri(Activity, Object, int, int,
     * Uri, int)} for callers with a {@link SessionHolder}. Resolves the client UID and PID from the
     * session.
     *
     * @param activity The activity context.
     * @param caller The caller object (ComponentCaller on Android 15+).
     * @param session The session holder associated with the launching client app.
     * @param uri The Content URI to verify.
     * @param requestedPermission The permission flag (e.g. FLAG_GRANT_READ_URI_PERMISSION).
     * @return True if the caller has permission for uri, false otherwise.
     */
    @SuppressLint("NewApi")
    public static boolean doesCallerHavePermissionForUri(
            Activity activity,
            @Nullable Object caller,
            @Nullable SessionHolder session,
            @Nullable Uri uri,
            int requestedPermission) {
        int uid = -1;
        int pid = 0;
        if (session != null) {
            uid = CustomTabsConnection.getInstance().getClientUidForSession(session);
            pid = CustomTabsConnection.getInstance().getClientPidForSession(session);
        }
        return doesCallerHavePermissionForUri(activity, caller, uid, pid, uri, requestedPermission);
    }

    /**
     * Checks caller permissions for any file URIs in sourceIntent and stashes the verified results
     * in targetIntent.
     *
     * @param activity The launcher activity.
     * @param sourceIntent The incoming intent containing client extras.
     * @param targetIntent The launch intent being prepared for CustomTabActivity.
     */
    public static void copyFilePermissions(
            Activity activity, Intent sourceIntent, Intent targetIntent) {
        // Strip EXTRA_VERIFIED_FILE_HANDLING_DATA/EXTRA_VERIFIED_FILE_CAN_WRITE if present on
        // targetIntent so that they cannot be set by external client apps.
        IntentUtils.safeRemoveExtra(
                targetIntent, CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_HANDLING_DATA);
        IntentUtils.safeRemoveExtra(
                targetIntent, CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_CAN_WRITE);

        Bundle fileHandlingBundle =
                IntentUtils.safeGetBundleExtra(sourceIntent, EXTRA_FILE_HANDLING_DATA);
        if (fileHandlingBundle == null) {
            return;
        }

        FileHandlingData fileHandlingData;
        try {
            fileHandlingData = FileHandlingData.fromBundle(fileHandlingBundle);
        } catch (Throwable e) {
            Log.w(TAG, "Failed to parse file handling data", e);
            return;
        }
        if (fileHandlingData == null || fileHandlingData.uris.isEmpty()) {
            return;
        }

        Object caller = null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            try {
                caller = activity.getInitialCaller();
            } catch (Exception e) {
                Log.w(TAG, "Failed to get initial caller. Falling back.", e);
            }
        }

        SessionHolder session = SessionHolder.getSessionHolderFromIntent(sourceIntent);
        List<Uri> verifiedUris = new ArrayList<>();
        List<Boolean> canWriteList = new ArrayList<>();
        for (Uri uri : fileHandlingData.uris) {
            if (!isValidLaunchUri(uri)) {
                Log.w(TAG, "Invalid launch URI: %s", uri);
                continue;
            }
            if (!doesCallerHavePermissionForUri(
                    activity, caller, session, uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)) {
                Log.w(TAG, "Caller does not have read permission for URI: %s", uri);
                continue;
            }
            verifiedUris.add(uri);
            boolean canWrite =
                    doesCallerHavePermissionForUri(
                            activity, caller, session, uri, Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            canWriteList.add(canWrite);
        }

        if (verifiedUris.isEmpty()) {
            // All filtered out, put empty data to indicate we checked but none allowed.
            FileHandlingData verifiedData = new FileHandlingData(new ArrayList<>());
            targetIntent.putExtra(
                    CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_HANDLING_DATA,
                    verifiedData.toBundle());
            return;
        }

        FileHandlingData verifiedData = new FileHandlingData(verifiedUris);
        targetIntent.putExtra(
                CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_HANDLING_DATA,
                verifiedData.toBundle());
        boolean[] canWriteArray = new boolean[canWriteList.size()];
        for (int i = 0; i < canWriteList.size(); i++) {
            canWriteArray[i] = canWriteList.get(i);
        }
        targetIntent.putExtra(
                CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_CAN_WRITE, canWriteArray);
    }

    /**
     * A single-use token for share data verified against the original caller before the First Run
     * Experience; see {@link #verifyShareDataBeforeFirstRun}.
     */
    static final String EXTRA_PRE_FRE_SHARE_DATA_TOKEN =
            "org.chromium.chrome.browser.customtabs.content.PRE_FRE_SHARE_DATA_TOKEN";

    /**
     * Share data verified before the First Run Experience, by token. Kept in process rather than in
     * the intent, so that an external app cannot supply it, and the intent does not need to be
     * marked as trusted. Only accessed on the UI thread.
     */
    private static final Map<String, Bundle> sPreFreVerifiedShareData = new HashMap<>();

    /**
     * Verifies the caller's permissions for any URIs in a TWA intent's share data before the First
     * Run Experience (FRE) intercepts the intent, and keeps the verified share data, for {@link
     * #copyShareDataPermissions} to use when Chrome relaunches the intent after the FRE.
     *
     * <p>After the FRE, Chrome itself is the caller, and the client app's own access to the shared
     * URIs (e.g. granted to it by the app the user shared from) may have lapsed, so the URIs could
     * no longer be verified and would be dropped from the share.
     *
     * @param activity The launcher activity, which received the intent from the original caller.
     * @param intent The intent that will be relaunched after the FRE.
     */
    public static void verifyShareDataBeforeFirstRun(Activity activity, Intent intent) {
        // Already verified, before Chrome relaunched the intent on the way to the FRE.
        String token = IntentUtils.safeGetStringExtra(intent, EXTRA_PRE_FRE_SHARE_DATA_TOKEN);
        if (token != null && sPreFreVerifiedShareData.containsKey(token)) {
            return;
        }
        IntentUtils.safeRemoveExtra(intent, EXTRA_PRE_FRE_SHARE_DATA_TOKEN);
        if (!IntentUtils.safeGetBooleanExtra(
                        intent, TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false)
                || IntentUtils.safeGetBundleExtra(intent, EXTRA_SHARE_DATA) == null) {
            return;
        }
        Intent verifiedIntent = new Intent();
        copyShareDataPermissions(activity, intent, verifiedIntent);
        Bundle verifiedBundle =
                IntentUtils.safeGetBundleExtra(
                        verifiedIntent, CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        ShareData verifiedShareData =
                verifiedBundle == null ? null : ShareData.fromBundle(verifiedBundle);
        // Only needed if a URI was verified. Otherwise, verifying again after the FRE gives the
        // same result.
        if (verifiedShareData == null
                || verifiedShareData.uris == null
                || verifiedShareData.uris.isEmpty()) {
            return;
        }
        token = UUID.randomUUID().toString();
        sPreFreVerifiedShareData.put(token, verifiedBundle);
        intent.putExtra(EXTRA_PRE_FRE_SHARE_DATA_TOKEN, token);
    }

    /**
     * Checks caller permissions for any file URIs in sourceIntent's share data and stashes the
     * verified results in targetIntent.
     *
     * @param activity The launcher activity.
     * @param sourceIntent The incoming intent containing client extras.
     * @param targetIntent The launch intent being prepared for CustomTabActivity.
     */
    public static void copyShareDataPermissions(
            Activity activity, Intent sourceIntent, Intent targetIntent) {
        // Strip EXTRA_VERIFIED_SHARE_DATA if present on targetIntent so that it cannot be set
        // by external client apps.
        IntentUtils.safeRemoveExtra(
                targetIntent, CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        IntentUtils.safeRemoveExtra(targetIntent, EXTRA_PRE_FRE_SHARE_DATA_TOKEN);

        // If the intent carries a token from verifyShareDataBeforeFirstRun(), its share data was
        // verified against the original caller before the First Run Experience. The token is only
        // valid in this process and only once, so an external app cannot supply one. After the
        // FRE, Chrome is the caller, and the client app's own access to the shared URIs may have
        // lapsed, so they cannot be verified again.
        String token = IntentUtils.safeGetStringExtra(sourceIntent, EXTRA_PRE_FRE_SHARE_DATA_TOKEN);
        Bundle preVerifiedShareData = token == null ? null : sPreFreVerifiedShareData.remove(token);
        if (preVerifiedShareData != null) {
            targetIntent.putExtra(
                    CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA, preVerifiedShareData);
            return;
        }

        Bundle shareDataBundle = IntentUtils.safeGetBundleExtra(sourceIntent, EXTRA_SHARE_DATA);
        if (shareDataBundle == null) {
            return;
        }

        ShareData shareData;
        try {
            shareData = ShareData.fromBundle(shareDataBundle);
        } catch (Throwable e) {
            Log.w(TAG, "Failed to parse share data", e);
            return;
        }
        if (shareData == null) {
            return;
        }
        if (shareData.uris == null || shareData.uris.isEmpty()) {
            targetIntent.putExtra(
                    CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA, shareData.toBundle());
            return;
        }

        Object caller = null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            try {
                caller = activity.getInitialCaller();
            } catch (Exception e) {
                Log.w(TAG, "Failed to get initial caller. Falling back.", e);
            }
        }

        SessionHolder session = SessionHolder.getSessionHolderFromIntent(sourceIntent);
        List<Uri> verifiedUris = new ArrayList<>();
        for (Uri uri : shareData.uris) {
            if (!isValidLaunchUri(uri)) {
                Log.w(TAG, "Invalid launch URI: %s", uri);
                continue;
            }
            if (!doesCallerHavePermissionForUri(
                    activity, caller, session, uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)) {
                Log.w(TAG, "Caller does not have read permission for share URI: %s", uri);
                continue;
            }
            verifiedUris.add(uri);
        }

        ShareData verifiedShareData = new ShareData(shareData.title, shareData.text, verifiedUris);
        targetIntent.putExtra(
                CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA,
                verifiedShareData.toBundle());
    }

    /**
     * Checks caller permissions for any file URIs in a WebAPK share intent and stashes the verified
     * results in targetIntent.
     *
     * @param activity The launcher activity.
     * @param sourceIntent The incoming intent containing client extras.
     * @param targetIntent The launch intent being prepared for WebappActivity.
     * @param webApkPackageName The package name of the WebAPK.
     */
    @SuppressLint("NewApi")
    public static void copyShareDataPermissionsForWebApk(
            Activity activity,
            Intent sourceIntent,
            Intent targetIntent,
            @Nullable String webApkPackageName) {
        // Strip EXTRA_VERIFIED_SHARE_DATA if present on targetIntent so that it cannot be set
        // by external client apps.
        IntentUtils.safeRemoveExtra(
                targetIntent, CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);

        ShareData shareData = extractWebApkShareData(sourceIntent);
        if (shareData == null) {
            return;
        }

        if (shareData.uris == null || shareData.uris.isEmpty()) {
            targetIntent.putExtra(
                    CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA, shareData.toBundle());
            return;
        }

        Object caller = null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            try {
                caller = activity.getInitialCaller();
            } catch (Exception e) {
                Log.w(TAG, "Failed to get initial caller. Falling back.", e);
            }
        }

        int webApkUid = -1;
        if (!TextUtils.isEmpty(webApkPackageName)) {
            try {
                webApkUid = activity.getPackageManager().getPackageUid(webApkPackageName, 0);
            } catch (PackageManager.NameNotFoundException e) {
                Log.w(TAG, "WebAPK package not found: %s", webApkPackageName, e);
            }
        }

        List<Uri> verifiedUris = new ArrayList<>();
        for (Uri uri : shareData.uris) {
            if (!isValidLaunchUri(uri)) {
                Log.w(TAG, "Invalid launch URI: %s", uri);
                continue;
            }
            if (!doesCallerHavePermissionForUri(
                    activity,
                    caller,
                    webApkUid,
                    /* clientPid= */ 0,
                    uri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION)) {
                Log.w(TAG, "Caller does not have read permission for share URI: %s", uri);
                continue;
            }
            verifiedUris.add(uri);
        }

        ShareData verifiedShareData = new ShareData(shareData.title, shareData.text, verifiedUris);
        targetIntent.putExtra(
                CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA,
                verifiedShareData.toBundle());
    }

    private static @Nullable ShareData extractWebApkShareData(Intent intent) {
        String subject = IntentUtils.safeGetStringExtra(intent, Intent.EXTRA_SUBJECT);
        String text = IntentUtils.safeGetStringExtra(intent, Intent.EXTRA_TEXT);
        List<Uri> files = IntentUtils.getParcelableArrayListExtra(intent, Intent.EXTRA_STREAM);
        if (files == null) {
            Uri file = IntentUtils.safeGetParcelableExtra(intent, Intent.EXTRA_STREAM);
            if (file != null) {
                files = new ArrayList<>();
                files.add(file);
            }
        }
        if (subject == null && text == null && files == null) {
            return null;
        }
        return new ShareData(subject, text, files);
    }

    /**
     * Filters incoming share data to retain only URIs that the launching client app has permission
     * to access.
     *
     * @param intentDataProvider Provides incoming intent and session customization data.
     * @param activity The activity context, if available.
     * @param caller The caller object (e.g. ComponentCaller on Android 15+), if available.
     * @return The filtered ShareData object containing authorized URIs, or null if input was null.
     */
    public static @Nullable ShareData filterShareData(
            BrowserServicesIntentDataProvider intentDataProvider,
            @Nullable Activity activity,
            @Nullable Object caller) {
        Intent intent = intentDataProvider.getIntent();
        if (intent != null) {
            Bundle verifiedBundle =
                    IntentUtils.safeGetBundleExtra(
                            intent, CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
            if (verifiedBundle != null) {
                try {
                    return ShareData.fromBundle(verifiedBundle);
                } catch (Throwable e) {
                    Log.w(TAG, "Failed to unparcel verified share data", e);
                }
            }
        }

        ShareData shareData = intentDataProvider.getShareData();
        if (shareData == null || shareData.uris == null || shareData.uris.isEmpty()) {
            return shareData;
        }

        if (activity == null) {
            return new ShareData(shareData.title, shareData.text, new ArrayList<>());
        }

        // Resolve client UID: from CCT session for TWAs, or from WebAPK package for WebAPKs.
        // For WebAPKs, this is a defense-in-depth fallback for cases where
        // EXTRA_VERIFIED_SHARE_DATA was not stashed by WebappLauncherActivity.
        // Note: caller is null when invoked by TwaSharingController, so on Android 15+ this falls
        // back to checkUriPermission against clientUid.
        int clientUid = -1;
        int clientPid = 0;
        SessionHolder session = intentDataProvider.getSession();
        if (session != null) {
            clientUid = CustomTabsConnection.getInstance().getClientUidForSession(session);
            clientPid = CustomTabsConnection.getInstance().getClientPidForSession(session);
        } else if (intentDataProvider.getActivityType() == ActivityType.WEB_APK
                && intentDataProvider.getWebApkExtras() != null) {
            String webApkPackage = intentDataProvider.getWebApkExtras().webApkPackageName;
            if (webApkPackage != null) {
                try {
                    clientUid = activity.getPackageManager().getPackageUid(webApkPackage, 0);
                } catch (PackageManager.NameNotFoundException e) {
                    Log.w(TAG, "WebAPK package not found: %s", webApkPackage, e);
                }
            }
        }

        List<Uri> filteredUris = new ArrayList<>();
        for (Uri uri : shareData.uris) {
            if (!isValidLaunchUri(uri)) {
                Log.w(TAG, "Invalid launch URI: %s", uri);
                continue;
            }
            if (!doesCallerHavePermissionForUri(
                    activity,
                    caller,
                    clientUid,
                    clientPid,
                    uri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION)) {
                Log.w(TAG, "Caller does not have read permission for share URI: %s", uri);
                continue;
            }
            filteredUris.add(uri);
        }

        return new ShareData(shareData.title, shareData.text, filteredUris);
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
     * with this object and associated launch parameters. WebContents parameters must not be null.
     * Callers must ensure they are valid.
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
