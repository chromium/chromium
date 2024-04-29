// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.SharedPreferences;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.net.ChromiumNetworkAdapter;
import org.chromium.net.NetworkTrafficAnnotationTag;

import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Date;

/**
 * Keeps tabs on the current state of Chrome, tracking if and when a request should be sent to the
 * Omaha Server.
 *
 * When Chrome is brought to the foreground, it will trigger a call to
 * {@link OmahaBase#onForegroundSessionStart}, which kicks off a series of scheduled events
 * that allow the class to run.  A single alarm is used to trigger the whole pipeline when needed.
 * - If Chrome isn't running when the alarm is fired, no pings or update checks will be performed.
 * - If Chrome doesn't have a pending request to POST, no POST will be performed.
 *
 * When a fresh install is detected (or the user clears their data), OmahaBase will send an XML
 * request saying that a new install was detected, then follow up with an XML request saying that
 * the user was active and that we need to check for Chrome updates.
 *
 * mevissen suggested being conservative with our timers for sending requests.
 * POST attempts that fail to be acknowledged by the server are re-attempted, with at least
 * one hour between each attempt.
 *
 * Status is saved directly to the the disk after every run of the pipeline.
 *
 * Implementation notes:
 * http://docs.google.com/a/google.com/document/d/1scTCovqASf5ktkOeVj8wFRkWTCeDYw2LrOBNn05CDB0/edit
 */
public class OmahaBase {
    // Used in various org.chromium.chrome.browser.omaha files.
    static final String TAG = "omaha";

    /** Version config data structure. */
    public static class VersionConfig {
        public final String latestVersion;
        public final String downloadUrl;
        public final int serverDate;
        public final String updateStatus;

        protected VersionConfig(
                String latestVersion, String downloadUrl, int serverDate, String updateStatus) {
            this.latestVersion = latestVersion;
            this.downloadUrl = downloadUrl;
            this.serverDate = serverDate;
            this.updateStatus = updateStatus;
        }
    }

    /** Represents the status of a manually-triggered update check. */
    @IntDef({
        UpdateStatus.UPDATED,
        UpdateStatus.OUTDATED,
        UpdateStatus.OFFLINE,
        UpdateStatus.FAILED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface UpdateStatus {
        int UPDATED = 0;
        int OUTDATED = 1;
        int OFFLINE = 2;
        int FAILED = 3;
    }

    private static final int UNKNOWN_DATE = -2;

    /** Whether or not the Omaha server should really be contacted. */
    private static boolean sDisabledForTesting;

    // Results of {@link #handlePostRequest()}.
    @IntDef({PostResult.NO_REQUEST, PostResult.SENT, PostResult.FAILED, PostResult.SCHEDULED})
    @Retention(RetentionPolicy.SOURCE)
    @interface PostResult {
        int NO_REQUEST = 0;
        int SENT = 1;
        int FAILED = 2;
        int SCHEDULED = 3;
    }

    /** Deprecated; kept around to cancel alarms set for OmahaClient pre-M58. */
    private static final String ACTION_REGISTER_REQUEST =
            "org.chromium.chrome.browser.omaha.ACTION_REGISTER_REQUEST";

    // Delays between events.
    static final long MS_POST_BASE_DELAY = DateUtils.HOUR_IN_MILLIS;
    static final long MS_POST_MAX_DELAY = DateUtils.HOUR_IN_MILLIS * 5;
    static final long MS_BETWEEN_REQUESTS = DateUtils.HOUR_IN_MILLIS * 5;
    static final int MS_CONNECTION_TIMEOUT = (int) DateUtils.MINUTE_IN_MILLIS;

    // Strings indicating how the Chrome APK arrived on the user's device. These values MUST NOT
    // be changed without updating the corresponding Omaha server strings.
    private static final String INSTALL_SOURCE_SYSTEM = "system_image";
    private static final String INSTALL_SOURCE_ORGANIC = "organic";

    private static final long INVALID_TIMESTAMP = -1;
    private static final String INVALID_REQUEST_ID = "invalid";

    // Member fields not persisted to disk.
    private final OmahaDelegate mDelegate;
    private boolean mStateHasBeenRestored;

    // State saved written to and read from disk.
    private RequestData mCurrentRequest;
    private long mTimestampOfInstall;
    private long mTimestampForNextPostAttempt;
    private long mTimestampForNewRequest;
    private int mServerDate;
    private String mInstallSource;
    protected VersionConfig mVersionConfig;
    protected boolean mSendInstallEvent;

    // Request failure error code.
    private int mRequestErrorCode;

    public static void setIsDisabledForTesting(boolean state) {
        sDisabledForTesting = state;
        ResettersForTesting.register(() -> sDisabledForTesting = false);
    }

    static boolean isDisabled() {
        return sDisabledForTesting;
    }

    /**
     * Constructs a new OmahaBase.
     * @param delegate The {@link OmahaDelegate} used to interact with the system.
     */
    OmahaBase(OmahaDelegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Synchronously checks for updates.
     * @return UpdateStatus enum value corresponding to the update state.
     */
    public @UpdateStatus int checkForUpdates() {
        // Since this update check is synchronous and blocking on the network
        // connection, it should not be run on the UI thread.
        ThreadUtils.assertOnBackgroundThread();
        // This is not available on developer builds.
        if (getRequestGenerator() == null) {
            Log.w(
                    TAG,
                    "OmahaBase::checkForUpdates(): Request generator is null. This is probably "
                            + "a developer build.");
            return UpdateStatus.FAILED;
        }
        // Create all the metadata needed for an Omaha request.
        long currentTimestamp = getBackoffScheduler().getCurrentTime();
        String installSource =
                mDelegate.isInSystemImage() ? INSTALL_SOURCE_SYSTEM : INSTALL_SOURCE_ORGANIC;
        RequestData currentRequest =
                createRequestData(false, currentTimestamp, null, installSource);
        String sessionID = mDelegate.generateUUID();
        long timestampOfInstall =
                OmahaPrefUtils.getSharedPreferences()
                        .getLong(OmahaPrefUtils.PREF_TIMESTAMP_OF_INSTALL, currentTimestamp);
        // Send the request and parse the response.
        VersionConfig versionConfig =
                generateAndPostRequest(
                        currentTimestamp, sessionID, currentRequest, timestampOfInstall);
        if (versionConfig == null) {
            Log.w(TAG, "OmahaBase::checkForUpdates(): versionConfig parsed from response is null.");
            return (mRequestErrorCode == RequestFailureException.ERROR_CONNECTIVITY)
                    ? UpdateStatus.OFFLINE
                    : UpdateStatus.FAILED;
        }
        // If the version matches exactly, the Omaha server will return status="noupdate" without
        // providing the latest version number.
        if (versionConfig.updateStatus != null && versionConfig.updateStatus.equals("noupdate")) {
            return UpdateStatus.UPDATED;
        }
        // Compare the current version with the latest received from the server.
        VersionNumber current = VersionNumber.fromString(getInstalledVersion());
        VersionNumber latest = VersionNumber.fromString(versionConfig.latestVersion);
        if (current == null || latest == null) {
            return UpdateStatus.FAILED;
        }
        return current.isSmallerThan(latest) ? UpdateStatus.OUTDATED : UpdateStatus.UPDATED;
    }

    protected void run() {
        if (OmahaBase.isDisabled() || getRequestGenerator() == null) {
            Log.v(TAG, "Disabled.  Ignoring intent.");
            return;
        }

        restoreState();

        long nextTimestamp = Long.MAX_VALUE;
        if (mDelegate.isChromeBeingUsed()) {
            handleRegisterActiveRequest();
            nextTimestamp = Math.min(nextTimestamp, mTimestampForNewRequest);
        }

        if (hasRequest()) {
            @PostResult int result = handlePostRequest();
            if (result == PostResult.FAILED || result == PostResult.SCHEDULED) {
                nextTimestamp = Math.min(nextTimestamp, mTimestampForNextPostAttempt);
            }
        }

        // TODO(dfalcantara): Prevent Omaha code from repeatedly rescheduling itself immediately in
        //                    case a scheduling error occurs.
        if (nextTimestamp != Long.MAX_VALUE && nextTimestamp >= 0) {
            long currentTimestamp = mDelegate.getScheduler().getCurrentTime();
            Log.d(TAG, "Attempting to schedule next job for: " + new Date(nextTimestamp));
            mDelegate.scheduleService(currentTimestamp, nextTimestamp);
        }

        saveState();
    }

    /**
     * Determines if a new request should be generated.  New requests are only generated if enough
     * time has passed between now and the last time a request was generated.
     */
    private void handleRegisterActiveRequest() {
        // If the current request is too old, generate a new one.
        long currentTimestamp = getBackoffScheduler().getCurrentTime();
        boolean isTooOld =
                hasRequest()
                        && mCurrentRequest.getAgeInMilliseconds(currentTimestamp)
                                >= MS_BETWEEN_REQUESTS;
        boolean isOverdue = currentTimestamp >= mTimestampForNewRequest;
        if (isTooOld || isOverdue) {
            registerNewRequest(currentTimestamp);
        }
    }

    /** Sends the request it is holding. */
    private @PostResult int handlePostRequest() {
        if (!hasRequest()) {
            mDelegate.onHandlePostRequestDone(PostResult.NO_REQUEST, false);
            return PostResult.NO_REQUEST;
        }

        // If enough time has passed since the last attempt, try sending a request.
        @PostResult int result;
        long currentTimestamp = getBackoffScheduler().getCurrentTime();
        boolean installEventWasSent = false;
        if (currentTimestamp >= mTimestampForNextPostAttempt) {
            // All requests made during the same session should have the same ID.
            String sessionID = mDelegate.generateUUID();
            boolean sendingInstallRequest = mSendInstallEvent;
            boolean succeeded = generateAndPostRequest(currentTimestamp, sessionID);
            onResponseReceived(succeeded);

            if (succeeded && sendingInstallRequest) {
                // Only the first request ever generated should contain an install event.
                mSendInstallEvent = false;
                installEventWasSent = true;

                // Create and immediately send another request for a ping and update check.
                registerNewRequest(currentTimestamp);
                succeeded &= generateAndPostRequest(currentTimestamp, sessionID);
                // Previous line is executed only when succeeded is true, so the updated value
                // reflects the status of the last call.
                onResponseReceived(succeeded);
            }

            result = succeeded ? PostResult.SENT : PostResult.FAILED;
        } else {
            result = PostResult.SCHEDULED;
        }

        mDelegate.onHandlePostRequestDone(result, installEventWasSent);
        return result;
    }

    /**
     * @return version currently installed on the device.
     */
    protected String getInstalledVersion() {
        return VersionNumberGetter.getInstance().getCurrentlyUsedVersion();
    }

    protected boolean generateAndPostRequest(long currentTimestamp, String sessionID) {
        mVersionConfig =
                generateAndPostRequest(
                        currentTimestamp, sessionID, mCurrentRequest, mTimestampOfInstall);
        return mVersionConfig != null;
    }

    protected VersionConfig generateAndPostRequest(
            long currentTimestamp,
            String sessionID,
            RequestData currentRequest,
            long timestampOfInstall) {
        try {
            // Generate the XML for the current request.
            long installAgeInDays =
                    RequestGenerator.installAge(
                            currentTimestamp,
                            timestampOfInstall,
                            currentRequest.isSendInstallEvent());
            String xml =
                    getRequestGenerator()
                            .generateXML(
                                    sessionID,
                                    getInstalledVersion(),
                                    installAgeInDays,
                                    mVersionConfig == null
                                            ? UNKNOWN_DATE
                                            : mVersionConfig.serverDate,
                                    currentRequest);

            // Send the request to the server & wait for a response.
            String response = postRequest(currentTimestamp, xml);

            // Parse out the response.
            String appId = getRequestGenerator().getAppId();
            ResponseParser parser = new ResponseParser(appId, currentRequest.isSendInstallEvent());
            return parser.parseResponse(response);
        } catch (RequestFailureException e) {
            Log.e(TAG, "Failed to contact server: ", e);
            mRequestErrorCode = e.errorCode;
            return null;
        }
    }

    protected boolean onResponseReceived(boolean succeeded) {
        ExponentialBackoffScheduler scheduler = getBackoffScheduler();
        if (succeeded) {
            // If we've gotten this far, we've successfully sent a request.
            mCurrentRequest = null;

            scheduler.resetFailedAttempts();
            mTimestampForNewRequest = scheduler.getCurrentTime() + MS_BETWEEN_REQUESTS;
            mTimestampForNextPostAttempt = scheduler.calculateNextTimestamp();
            Log.d(
                    TAG,
                    "Request to Server Successful. Timestamp for next request:"
                            + mTimestampForNextPostAttempt);
        } else {
            // Set the alarm to try again later.  Failures are incremented after setting the timer
            // to allow the first failure to incur the minimum base delay between POSTs.
            mTimestampForNextPostAttempt = scheduler.calculateNextTimestamp();
            scheduler.increaseFailedAttempts();
        }

        mDelegate.onGenerateAndPostRequestDone(succeeded);
        return succeeded;
    }

    /**
     * Registers a new request with the current timestamp.  Internal timestamps are reset to start
     * fresh.
     * @param currentTimestamp Current time.
     */
    private void registerNewRequest(long currentTimestamp) {
        mCurrentRequest = createRequestData(currentTimestamp, null);
        getBackoffScheduler().resetFailedAttempts();
        mTimestampForNextPostAttempt = currentTimestamp;

        // Tentatively set the timestamp for a new request.  This will be updated when the server
        // is successfully contacted.
        mTimestampForNewRequest = currentTimestamp + MS_BETWEEN_REQUESTS;

        mDelegate.onRegisterNewRequestDone(mTimestampForNewRequest, mTimestampForNextPostAttempt);
    }

    private RequestData createRequestData(long currentTimestamp, String persistedID) {
        return createRequestData(mSendInstallEvent, currentTimestamp, persistedID, mInstallSource);
    }

    private RequestData createRequestData(
            boolean sendInstallEvent,
            long currentTimestamp,
            String persistedID,
            String installSource) {
        // If we're sending a persisted event, keep trying to send the same request ID.
        String requestID;
        if (persistedID == null || INVALID_REQUEST_ID.equals(persistedID)) {
            requestID = mDelegate.generateUUID();
        } else {
            requestID = persistedID;
        }
        return new RequestData(sendInstallEvent, currentTimestamp, requestID, installSource);
    }

    private boolean hasRequest() {
        return mCurrentRequest != null;
    }

    /**
     * Posts the request to the Omaha server.
     * @return the XML response as a String.
     * @throws RequestFailureException if the request fails.
     */
    private String postRequest(long timestamp, String xml) throws RequestFailureException {
        HttpURLConnection urlConnection = createConnection();
        try {
            // Prepare the HTTP header.
            urlConnection.setDoOutput(true);
            urlConnection.setFixedLengthStreamingMode(
                    ApiCompatibilityUtils.getBytesUtf8(xml).length);
            if (mSendInstallEvent && getBackoffScheduler().getNumFailedAttempts() > 0) {
                String age = Long.toString(mCurrentRequest.getAgeInSeconds(timestamp));
                urlConnection.addRequestProperty("X-RequestAge", age);
            }

            return OmahaBase.sendRequestToServer(urlConnection, xml);
        } finally {
            urlConnection.disconnect();
        }
    }

    /** Returns a HttpURLConnection to the server. */
    @VisibleForTesting
    protected HttpURLConnection createConnection() throws RequestFailureException {
        // TODO(crbug.com/1139505): Remove the note about UID when UID fallback is removed.
        NetworkTrafficAnnotationTag annotation =
                NetworkTrafficAnnotationTag.createComplete(
                        "omaha_client_android_uc",
                        """
                semantics {
                  sender: 'Updates'
                  description:
                    'This traffic checks whether the browser is up-to-date and '
                    'provides basic browser telemetry using the Omaha protocol.'
                  trigger: 'Manual or automatic checks for updates.'
                  data:
                    'Various OS and browser parameters such as version, '
                    'architecture, channel, and the calendar date of the previous '
                    'communication. '
                    'A unique identifier for the device may be transmitted.'
                  destination: GOOGLE_OWNED_SERVICE
                }
                policy {
                  cookies_allowed: NO
                  policy_exception_justification: 'Not implemented.'
                  setting: 'This feature cannot be disabled.'
                }""");
        try {
            URL url = new URL(getRequestGenerator().getServerUrl());
            HttpURLConnection connection =
                    (HttpURLConnection) ChromiumNetworkAdapter.openConnection(url, annotation);
            connection.setConnectTimeout(MS_CONNECTION_TIMEOUT);
            connection.setReadTimeout(MS_CONNECTION_TIMEOUT);
            return connection;
        } catch (IOException e) {
            throw new RequestFailureException(
                    "Failed to open connection to URL",
                    e,
                    RequestFailureException.ERROR_CONNECTIVITY);
        }
    }

    /**
     * Reads the data back from the file it was saved to.  Uses SharedPreferences to handle I/O.
     * Validity checks are performed on the timestamps to guard against clock changing.
     */
    private void restoreState() {
        if (mStateHasBeenRestored) return;

        String installSource =
                mDelegate.isInSystemImage() ? INSTALL_SOURCE_SYSTEM : INSTALL_SOURCE_ORGANIC;
        ExponentialBackoffScheduler scheduler = getBackoffScheduler();
        long currentTime = scheduler.getCurrentTime();

        SharedPreferences preferences = OmahaPrefUtils.getSharedPreferences();
        mTimestampForNewRequest =
                preferences.getLong(OmahaPrefUtils.PREF_TIMESTAMP_FOR_NEW_REQUEST, currentTime);
        mTimestampForNextPostAttempt =
                preferences.getLong(
                        OmahaPrefUtils.PREF_TIMESTAMP_FOR_NEXT_POST_ATTEMPT, currentTime);
        mTimestampOfInstall =
                preferences.getLong(OmahaPrefUtils.PREF_TIMESTAMP_OF_INSTALL, currentTime);
        mSendInstallEvent = preferences.getBoolean(OmahaPrefUtils.PREF_SEND_INSTALL_EVENT, true);
        mInstallSource = preferences.getString(OmahaPrefUtils.PREF_INSTALL_SOURCE, installSource);
        mVersionConfig = getVersionConfig(preferences);

        // If we're not sending an install event, don't bother restoring the request ID:
        // the server does not expect to have persisted request IDs for pings or update checks.
        String persistedRequestId =
                mSendInstallEvent
                        ? preferences.getString(
                                OmahaPrefUtils.PREF_PERSISTED_REQUEST_ID, INVALID_REQUEST_ID)
                        : INVALID_REQUEST_ID;
        long requestTimestamp =
                preferences.getLong(OmahaPrefUtils.PREF_TIMESTAMP_OF_REQUEST, INVALID_TIMESTAMP);
        mCurrentRequest =
                requestTimestamp == INVALID_TIMESTAMP
                        ? null
                        : createRequestData(requestTimestamp, persistedRequestId);

        // Confirm that the timestamp for the next request is less than the base delay.
        long delayToNewRequest = mTimestampForNewRequest - currentTime;
        if (delayToNewRequest > MS_BETWEEN_REQUESTS) {
            Log.w(
                    TAG,
                    "Delay to next request ("
                            + delayToNewRequest
                            + ") is longer than expected.  Resetting to now.");
            mTimestampForNewRequest = currentTime;
        }

        // Confirm that the timestamp for the next POST is less than the current delay.
        long delayToNextPost = mTimestampForNextPostAttempt - currentTime;
        long lastGeneratedDelay = scheduler.getGeneratedDelay();
        if (delayToNextPost > lastGeneratedDelay) {
            Log.w(
                    TAG,
                    "Delay to next post attempt ("
                            + delayToNextPost
                            + ") is greater than expected ("
                            + lastGeneratedDelay
                            + ").  Resetting to now.");
            mTimestampForNextPostAttempt = currentTime;
        }

        mStateHasBeenRestored = true;
    }

    /** Writes out the current state to a file. */
    private void saveState() {
        SharedPreferences prefs = OmahaPrefUtils.getSharedPreferences();
        SharedPreferences.Editor editor = prefs.edit();
        editor.putBoolean(OmahaPrefUtils.PREF_SEND_INSTALL_EVENT, mSendInstallEvent);
        editor.putLong(OmahaPrefUtils.PREF_TIMESTAMP_OF_INSTALL, mTimestampOfInstall);
        editor.putLong(
                OmahaPrefUtils.PREF_TIMESTAMP_FOR_NEXT_POST_ATTEMPT, mTimestampForNextPostAttempt);
        editor.putLong(OmahaPrefUtils.PREF_TIMESTAMP_FOR_NEW_REQUEST, mTimestampForNewRequest);
        editor.putLong(
                OmahaPrefUtils.PREF_TIMESTAMP_OF_REQUEST,
                hasRequest() ? mCurrentRequest.getCreationTimestamp() : INVALID_TIMESTAMP);
        editor.putString(
                OmahaPrefUtils.PREF_PERSISTED_REQUEST_ID,
                hasRequest() ? mCurrentRequest.getRequestID() : INVALID_REQUEST_ID);
        editor.putString(OmahaPrefUtils.PREF_INSTALL_SOURCE, mInstallSource);
        setVersionConfig(editor, mVersionConfig);
        editor.apply();

        mDelegate.onSaveStateDone(mTimestampForNewRequest, mTimestampForNextPostAttempt);
    }

    private RequestGenerator getRequestGenerator() {
        return mDelegate.getRequestGenerator();
    }

    private ExponentialBackoffScheduler getBackoffScheduler() {
        return mDelegate.getScheduler();
    }

    /** Begin communicating with the Omaha Update Server. */
    public static void onForegroundSessionStart() {
        if (!VersionInfo.isOfficialBuild() || isDisabled()) return;
        OmahaService.startServiceImmediately();
    }

    /** Checks whether Chrome has ever tried contacting Omaha before. */
    public static boolean isProbablyFreshInstall() {
        SharedPreferences prefs = OmahaPrefUtils.getSharedPreferences();
        return prefs.getLong(OmahaPrefUtils.PREF_TIMESTAMP_OF_INSTALL, -1) == -1;
    }

    /** Sends the request to the server and returns the response. */
    static String sendRequestToServer(HttpURLConnection urlConnection, String request)
            throws RequestFailureException {
        try {
            OutputStream out = new BufferedOutputStream(urlConnection.getOutputStream());
            OutputStreamWriter writer = new OutputStreamWriter(out);
            writer.write(request, 0, request.length());
            StreamUtil.closeQuietly(writer);
            checkServerResponseCode(urlConnection);
        } catch (IOException
                | SecurityException
                | IndexOutOfBoundsException
                | IllegalArgumentException e) {
            // IndexOutOfBoundsException is thought to be triggered by a bug in okio.
            // TODO(crbug.com/40709132): Record IndexOutOfBoundsException specifically.
            // IllegalArgumentException is triggered by a bug in okio. crbug.com/1149863.
            throw new RequestFailureException(
                    "Failed to write request to server: ",
                    e,
                    RequestFailureException.ERROR_CONNECTIVITY);
        }

        try {
            InputStreamReader reader = new InputStreamReader(urlConnection.getInputStream());
            BufferedReader in = new BufferedReader(reader);
            try {
                StringBuilder response = new StringBuilder();
                for (String line = in.readLine(); line != null; line = in.readLine()) {
                    response.append(line);
                }
                checkServerResponseCode(urlConnection);
                return response.toString();
            } finally {
                StreamUtil.closeQuietly(in);
            }
        } catch (IOException e) {
            throw new RequestFailureException(
                    "Failed when reading response from server: ",
                    e,
                    RequestFailureException.ERROR_CONNECTIVITY);
        }
    }

    /** Confirms that the Omaha server sent back an "OK" code. */
    private static void checkServerResponseCode(HttpURLConnection urlConnection)
            throws RequestFailureException {
        try {
            if (urlConnection.getResponseCode() != HttpURLConnection.HTTP_OK) {
                throw new RequestFailureException(
                        "Received "
                                + urlConnection.getResponseCode()
                                + " code instead of 200 (OK) from the server.  Aborting.");
            }
        } catch (IOException e) {
            throw new RequestFailureException("Failed to read response code from server: ", e);
        }
    }

    static void setVersionConfig(SharedPreferences.Editor editor, VersionConfig versionConfig) {
        editor.putString(
                OmahaPrefUtils.PREF_LATEST_VERSION,
                versionConfig == null ? "" : versionConfig.latestVersion);
        editor.putString(
                OmahaPrefUtils.PREF_MARKET_URL,
                versionConfig == null ? "" : versionConfig.downloadUrl);
        if (versionConfig != null) {
            editor.putInt(OmahaPrefUtils.PREF_SERVER_DATE, versionConfig.serverDate);
        }
    }

    static VersionConfig getVersionConfig(SharedPreferences sharedPref) {
        return new VersionConfig(
                sharedPref.getString(OmahaPrefUtils.PREF_LATEST_VERSION, ""),
                sharedPref.getString(OmahaPrefUtils.PREF_MARKET_URL, ""),
                sharedPref.getInt(OmahaPrefUtils.PREF_SERVER_DATE, -2),
                // updateStatus is only used for the on-demand check.
                null);
    }
}
