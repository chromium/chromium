// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.net.connectivitydetector;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Handler;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.content.ContentUtils;
import org.chromium.net.ChromiumNetworkAdapter;
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.NetworkTrafficAnnotationTag;

import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * Class that detects the network connectivity. We will get the connectivity info from Android
 * connection manager if available, as in Marshmallow and above. Otherwise, we will do http probes
 * to verify that a well-known URL returns an expected result. If the result can't be validated,
 * we will retry with exponential backoff.
 */
public class ConnectivityDetector implements NetworkChangeNotifier.ConnectionTypeObserver {
    // ProbeUrlType defined in tools/metrics/histograms/enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    public static final int PROBE_WITH_DEFAULT_URL = 0;
    public static final int PROBE_WITH_FALLBACK_URL = 1;
    public static final int PROBE_WITH_URL_COUNT = 2;

    // Denotes the connection state.
    @IntDef({
        ConnectionState.NONE,
        ConnectionState.DISCONNECTED,
        ConnectionState.NO_INTERNET,
        ConnectionState.CAPTIVE_PORTAL,
        ConnectionState.VALIDATED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ConnectionState {
        // Initial state or connection state can't be evaluated.
        int NONE = 0;
        // The network is disconnected.
        int DISCONNECTED = 1;
        // The network is connected, but it can't reach the Internet, i.e. connecting to a hotspot
        // that is not conencted to Internet.
        int NO_INTERNET = 2;
        // The network is connected, but capitive portal is detected and the user has not signed
        // into it.
        int CAPTIVE_PORTAL = 3;
        // The network is connected to Internet and validated. If the connected network imposes
        // capitive portal, the user has already signed into it.
        int VALIDATED = 4;
        // Count.
        int RESULT_COUNT = 5;
    }

    // Denotes how the connectivity check is done.
    @IntDef({
        ConnectivityCheckingStage.NOT_STARTED,
        ConnectivityCheckingStage.FROM_SYSTEM,
        ConnectivityCheckingStage.PROBE_DEFAULT_URL,
        ConnectivityCheckingStage.PROBE_FALLBACK_URL
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface ConnectivityCheckingStage {
        // Not started.
        int NOT_STARTED = 0;
        // Retrieved from the Android system.
        int FROM_SYSTEM = 1;
        // By probing the default URL.
        int PROBE_DEFAULT_URL = 2;
        // By probing the fallback URL.
        int PROBE_FALLBACK_URL = 3;
        // Count.
        int RESULT_COUNT = 4;
    }

    // The result of the HTTP probing. Defined in tools/metrics/histograms/enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        ProbeResult.NO_INTERNET,
        ProbeResult.SERVER_ERROR,
        ProbeResult.NOT_VALIDATED,
        ProbeResult.VALIDATED_WITH_NO_CONTENT,
        ProbeResult.VALIDATED_WITH_OK_BUT_ZERO_CONTENT_LENGTH,
        ProbeResult.VALIDATED_WITH_OK_BUT_NO_CONTENT_LENGTH
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface ProbeResult {
        // The network is connected, but it can't reach the Internet, i.e. connecting to a hotspot
        // that is not conencted to Internet.
        int NO_INTERNET = 0;
        // Server returns response code >= 400.
        int SERVER_ERROR = 1;
        // Cannot be validated due to not receiving expected result from server. This is most likely
        // caused by captive portal.
        int NOT_VALIDATED = 2;
        // Validated when the expected result is received from server.
        int VALIDATED_WITH_NO_CONTENT = 3;
        int VALIDATED_WITH_OK_BUT_ZERO_CONTENT_LENGTH = 4;
        int VALIDATED_WITH_OK_BUT_NO_CONTENT_LENGTH = 5;
        // Count.
        int RESULT_COUNT = 6;
    }

    /** Interface for observing network connectivity changes. */
    public interface Observer {
        /**
         * Called when the network connection state changes.
         * @param connectionState Current connection state.
         */
        void onConnectionStateChanged(@ConnectionState int connectionState);
    }

    /** Interface that allows the testing code to override certain behaviors. */
    public interface Delegate {
        // Infers the connection state based on the connectivity info returned from the Android
        // connectivity manager. Retrurns ConnectionState.NONE if we don't want to do this.
        @ConnectionState
        int inferConnectionStateFromSystem();

        // Returns true if we want to skip http probes.
        boolean shouldSkipHttpProbes();
    }

    /** Implementation that talks with the Android connectivity manager service. */
    public static class DelegateImpl implements Delegate {
        @Override
        public @ConnectionState int inferConnectionStateFromSystem() {
            // NET_CAPABILITY_VALIDATED and NET_CAPABILITY_CAPTIVE_PORTAL are only available on
            // Marshmallow and later versions.
            ConnectivityManager connectivityManager =
                    (ConnectivityManager)
                            ContextUtils.getApplicationContext()
                                    .getSystemService(Context.CONNECTIVITY_SERVICE);

            if (connectivityManager == null) return ConnectionState.NONE;

            boolean isCapitivePortal = false;
            Network[] networks = connectivityManager.getAllNetworks();
            if (networks.length == 0) return ConnectionState.DISCONNECTED;

            for (Network network : networks) {
                NetworkCapabilities capabilities =
                        connectivityManager.getNetworkCapabilities(network);
                if (capabilities == null) continue;
                Log.i(TAG, "Reported by system: " + capabilities.toString());
                if (capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED)
                        && capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                        && capabilities.hasCapability(
                                NetworkCapabilities.NET_CAPABILITY_NOT_RESTRICTED)) {
                    return ConnectionState.VALIDATED;
                }
                if (capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_CAPTIVE_PORTAL)) {
                    isCapitivePortal = true;
                }
            }

            return isCapitivePortal ? ConnectionState.CAPTIVE_PORTAL : ConnectionState.NO_INTERNET;
        }

        @Override
        public boolean shouldSkipHttpProbes() {
            return false;
        }
    }

    private static final String TAG = "OfflineIndicator";

    private static final String USER_AGENT_HEADER_NAME = "User-Agent";
    // Send the HTTPS probe first since the captive portals cannot see the encrypted URL path.
    private static final String DEFAULT_PROBE_URL = "https://www.google.com/generate_204";
    private static final String FALLBACK_PROBE_URL =
            "http://connectivitycheck.gstatic.com/generate_204";
    private static final String PROBE_METHOD = "GET";
    private static final int SOCKET_TIMEOUT_MS = 5000;
    private static final int CONNECTIVITY_CHECK_INITIAL_DELAY_MS = 5000;
    private static final int CONNECTIVITY_CHECK_MAX_DELAY_MS = 2 * 60 * 1000;

    private static Delegate sDelegateForTesting;
    private static String sDefaultProbeUrl = DEFAULT_PROBE_URL;
    private static String sFallbackProbeUrl = FALLBACK_PROBE_URL;
    private static String sProbeMethod = PROBE_METHOD;
    private static int sConnectivityCheckInitialDelayMs = CONNECTIVITY_CHECK_INITIAL_DELAY_MS;

    /** |mObserver| will be null after destruction. */
    @Nullable private Observer mObserver;

    private Delegate mDelegate;

    // Name of the client used for recording histograms.
    private final String mClientName;

    private @ConnectionType int mConnectionType = ConnectionType.CONNECTION_UNKNOWN;
    private @ConnectionState int mConnectionState = ConnectionState.NONE;

    private String mUserAgentString;
    private boolean mIsCheckingConnectivity;
    private @ConnectivityCheckingStage int mConnectivityCheckingStage =
            ConnectivityCheckingStage.NOT_STARTED;
    // The delay time, in milliseconds, before we can send next http probe request.
    private int mConnectivityCheckDelayMs;
    // The starting time, in milliseconds since boot, when we start to do http probes to validate
    // the connectivity. This is used in UMA reporting.
    private long mConnectivityCheckStartTimeMs;
    private Handler mHandler;
    private Runnable mRunnable;

    public ConnectivityDetector(Observer observer, String clientName) {
        mObserver = observer;
        mClientName = clientName;
        mDelegate = sDelegateForTesting != null ? sDelegateForTesting : new DelegateImpl();
        mHandler = new Handler();
        NetworkChangeNotifier.addConnectionTypeObserver(this);
        detect();
    }

    public void destroy() {
        NetworkChangeNotifier.removeConnectionTypeObserver(this);
        stopConnectivityCheck();
        mObserver = null;
    }

    public void detect() {
        onConnectionTypeChanged(NetworkChangeNotifier.getInstance().getCurrentConnectionType());
    }

    public @ConnectionState int getConnectionState() {
        return mConnectionState;
    }

    @Override
    public void onConnectionTypeChanged(@ConnectionType int connectionType) {
        // Even when there is no connection type change, we may still want to get the network
        // connectivity info from the system since the connectivity info can get updated by the
        // underlying network monitor, i.e., changing from not validated to validated.
        boolean hasConnectionTypeChange = mConnectionType != connectionType;
        mConnectionType = connectionType;
        Log.i(TAG, "onConnectionTypeChanged " + mConnectionType);

        // If not connected at all, no further check is needed.
        if (mConnectionType == ConnectionType.CONNECTION_NONE) {
            setConnectionState(ConnectionState.DISCONNECTED);

            // If the network is disconnected, no need to check for connectivity.
            stopConnectivityCheck();

            return;
        }

        // If there is no change to connection and connectivity checking is still in progress,
        // don't restart it.
        if (!hasConnectionTypeChange
                && mConnectivityCheckingStage != ConnectivityCheckingStage.NOT_STARTED) {
            return;
        }

        stopConnectivityCheck();
        performConnectivityCheck();
    }

    private void performConnectivityCheck() {
        mConnectivityCheckingStage = ConnectivityCheckingStage.FROM_SYSTEM;
        mConnectivityCheckDelayMs = 0;
        mConnectivityCheckStartTimeMs = SystemClock.elapsedRealtime();

        // Check the Android system to determine the network connectivity. If unavailable, as in
        // Android version below Marshmallow, we will kick off our own probes.
        @ConnectionState int newConnectionState = mDelegate.inferConnectionStateFromSystem();
        if (newConnectionState != ConnectionState.NONE) {
            setConnectionState(newConnectionState);
            processConnectivityCheckResult();
            return;
        }

        // Do manual check via sending HTTP probes to server.
        if (mUserAgentString == null) {
            mUserAgentString = ContentUtils.getBrowserUserAgent();
        }
        mConnectivityCheckingStage = ConnectivityCheckingStage.PROBE_DEFAULT_URL;
        checkConnectivityViaHttpProbe();
    }

    private void stopConnectivityCheck() {
        if (mConnectivityCheckingStage == ConnectivityCheckingStage.NOT_STARTED) return;

        // Cancel any backoff scheduling. The currently running probe should hit an exception and
        // bail out.
        if (mRunnable != null) {
            mHandler.removeCallbacks(mRunnable);
            mRunnable = null;
        }

        mConnectivityCheckingStage = ConnectivityCheckingStage.NOT_STARTED;
    }

    @VisibleForTesting
    void checkConnectivityViaHttpProbe() {
        assert mConnectivityCheckingStage == ConnectivityCheckingStage.PROBE_DEFAULT_URL
                || mConnectivityCheckingStage == ConnectivityCheckingStage.PROBE_FALLBACK_URL;

        if (mDelegate.shouldSkipHttpProbes()) {
            // Just assume to be validated which is what the testing code wants.
            setConnectionState(ConnectionState.VALIDATED);
            processConnectivityCheckResult();
        } else {
            sendHttpProbe(
                    mConnectivityCheckingStage == ConnectivityCheckingStage.PROBE_DEFAULT_URL,
                    SOCKET_TIMEOUT_MS,
                    (result) -> {
                        Log.i(
                                TAG,
                                "sendHttpProbe returned with result="
                                        + result
                                        + " and mConnectivityCheckingStage="
                                        + mConnectivityCheckingStage);
                        if (mConnectivityCheckingStage
                                == ConnectivityCheckingStage.PROBE_DEFAULT_URL) {
                            RecordHistogram.recordEnumeratedHistogram(
                                    "ConnectivityDetector.DefaultHttpProbeResult." + mClientName,
                                    result,
                                    ProbeResult.RESULT_COUNT);
                        } else {
                            RecordHistogram.recordEnumeratedHistogram(
                                    "ConnectivityDetector.FallbackHttpProbeResult." + mClientName,
                                    result,
                                    ProbeResult.RESULT_COUNT);
                        }

                        // If we just lose the connection, bail out.
                        if (mConnectionType == ConnectionType.CONNECTION_NONE) return;

                        updateConnectionStatePerProbeResult(result);
                        RecordHistogram.recordEnumeratedHistogram(
                                "ConnectivityDetector.ConnectionState." + mClientName,
                                mConnectionState,
                                ConnectionState.RESULT_COUNT);
                        processConnectivityCheckResult();
                    });
        }
    }

    private void processConnectivityCheckResult() {
        // If the connection is validated, we're done.
        Log.i(
                TAG,
                "processConnectivityCheckResult mConnectionState="
                        + mConnectionState
                        + " mConnectivityCheckingStage="
                        + mConnectivityCheckingStage);
        if (mConnectionState == ConnectionState.VALIDATED) {
            stopConnectivityCheck();
            return;
        }

        switch (mConnectivityCheckingStage) {
            case ConnectivityCheckingStage.FROM_SYSTEM:
                // Wait some time and ask the system again.
                scheduleNextConnectivityCheck();
                break;
            case ConnectivityCheckingStage.PROBE_DEFAULT_URL:
                // Probe again with the fallback URL. The captive portal may react differently
                // for a different url.
                mConnectivityCheckingStage = ConnectivityCheckingStage.PROBE_FALLBACK_URL;
                checkConnectivityViaHttpProbe();
                break;
            case ConnectivityCheckingStage.PROBE_FALLBACK_URL:
                // Wait some time and retry probing again.
                mConnectivityCheckingStage = ConnectivityCheckingStage.PROBE_DEFAULT_URL;
                scheduleNextConnectivityCheck();
                break;
        }
    }

    /**
     * Fetch a URL from a well-known server using Android system network stack.
     * Check asynchronously whether the device is currently connected to the Internet using the
     * Android system network stack. |callback| will be invoked with the boolean result to
     * denote if the connectivity is validated.
     */
    private void sendHttpProbe(
            final boolean useDefaultUrl, final int timeoutMs, final Callback<Integer> callback) {
        final String urlString = useDefaultUrl ? sDefaultProbeUrl : sFallbackProbeUrl;
        new AsyncTask<Integer>() {
            @Override
            protected Integer doInBackground() {
                HttpURLConnection urlConnection = null;
                try {
                    RecordHistogram.recordCount1MHistogram(
                            "ConnectivityDetector.SentHttpProbe." + mClientName, 1);
                    Log.i(TAG, "Sending HTTP Probe now to url:" + urlString);

                    URL url = new URL(urlString);
                    urlConnection =
                            (HttpURLConnection)
                                    ChromiumNetworkAdapter.openConnection(
                                            url,
                                            NetworkTrafficAnnotationTag.MISSING_TRAFFIC_ANNOTATION);
                    urlConnection.setInstanceFollowRedirects(false);
                    urlConnection.setRequestMethod(sProbeMethod);
                    urlConnection.setConnectTimeout(timeoutMs);
                    urlConnection.setReadTimeout(timeoutMs);
                    urlConnection.setUseCaches(false);
                    urlConnection.setRequestProperty(USER_AGENT_HEADER_NAME, mUserAgentString);

                    long requestTimestamp = SystemClock.elapsedRealtime();
                    urlConnection.connect();
                    long responseTimestamp = SystemClock.elapsedRealtime();
                    int responseCode = urlConnection.getResponseCode();

                    Log.i(
                            TAG,
                            "Probe "
                                    + urlString
                                    + " time="
                                    + (responseTimestamp - requestTimestamp)
                                    + "ms ret="
                                    + responseCode
                                    + " headers="
                                    + urlConnection.getHeaderFields());

                    if (responseCode == HttpURLConnection.HTTP_NO_CONTENT) {
                        return ProbeResult.VALIDATED_WITH_NO_CONTENT;
                    } else if (responseCode >= 400) {
                        return ProbeResult.SERVER_ERROR;
                    } else if (responseCode == HttpURLConnection.HTTP_OK) {
                        // Treat 200 response with zero content length to not be a captive portal
                        // because the user cannot sign in to an empty page. Probably this is due to
                        // a broken transparent proxy.
                        if (urlConnection.getContentLength() == 0) {
                            return ProbeResult.VALIDATED_WITH_OK_BUT_ZERO_CONTENT_LENGTH;
                        } else if (urlConnection.getContentLength() == -1) {
                            // When no Content-length (default value == -1), attempt to read a byte
                            // from the response.
                            if (urlConnection.getInputStream().read() == -1) {
                                return ProbeResult.VALIDATED_WITH_OK_BUT_NO_CONTENT_LENGTH;
                            }
                        }
                    }
                } catch (IOException e) {
                    Log.i(TAG, "Probe " + urlString + " failed w/ exception " + e);
                    // Most likely the exception is thrown due to host name not resolved or socket
                    // timeout.
                    return ProbeResult.NO_INTERNET;
                } finally {
                    if (urlConnection != null) {
                        urlConnection.disconnect();
                    }
                }
                // The result returned from a well-known URL doesn't match the expected result,
                // probably due to that the traffic is intercepted by the captive portal.
                return ProbeResult.NOT_VALIDATED;
            }

            @Override
            protected void onPostExecute(Integer result) {
                callback.onResult(result);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private void scheduleNextConnectivityCheck() {
        Log.i(TAG, "scheduleNextConnectivityCheck");
        assert mConnectivityCheckDelayMs >= 0;
        if (mConnectivityCheckDelayMs == 0) {
            mConnectivityCheckDelayMs = sConnectivityCheckInitialDelayMs;
        } else {
            mConnectivityCheckDelayMs *= 2;
        }

        // Give up after exceeding the maximum delay.
        if (mConnectivityCheckDelayMs >= CONNECTIVITY_CHECK_MAX_DELAY_MS) {
            // Prevent overflow of the delay value.
            mConnectivityCheckDelayMs = CONNECTIVITY_CHECK_MAX_DELAY_MS;
            Log.i(TAG, "No more retry after exceeding " + CONNECTIVITY_CHECK_MAX_DELAY_MS + "ms");
            if (mConnectionState == ConnectionState.NONE) {
                setConnectionState(ConnectionState.NO_INTERNET);
            }
            mIsCheckingConnectivity = false;
            return;
        }
        Log.i(TAG, "Retry after " + mConnectivityCheckDelayMs + "ms");

        mRunnable =
                new Runnable() {
                    @Override
                    public void run() {
                        performConnectivityCheck();
                    }
                };
        mHandler.postDelayed(mRunnable, mConnectivityCheckDelayMs);
    }

    private void updateConnectionStatePerProbeResult(@ProbeResult int result) {
        @ConnectionState int newConnectionState = mConnectionState;
        Log.i(TAG, "updateConnectionStatePerProbeResult result=" + result);
        switch (result) {
            case ProbeResult.VALIDATED_WITH_NO_CONTENT:
            case ProbeResult.VALIDATED_WITH_OK_BUT_ZERO_CONTENT_LENGTH:
            case ProbeResult.VALIDATED_WITH_OK_BUT_NO_CONTENT_LENGTH:
                newConnectionState = ConnectionState.VALIDATED;
                break;
            case ProbeResult.NOT_VALIDATED:
                newConnectionState = ConnectionState.CAPTIVE_PORTAL;
                break;
            case ProbeResult.NO_INTERNET:
                newConnectionState = ConnectionState.NO_INTERNET;
                break;
            case ProbeResult.SERVER_ERROR:
                // Don't update the connection state if there is a server error which should
                // be rare.
                break;
        }
        setConnectionState(newConnectionState);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void setConnectionState(@ConnectionState int connectionState) {
        Log.i(TAG, "setConnectionState connectionState=" + connectionState);
        if (mConnectionState == connectionState) return;
        mConnectionState = connectionState;
        if (mObserver != null) mObserver.onConnectionStateChanged(mConnectionState);
    }

    public static void setDelegateForTesting(Delegate delegate) {
        sDelegateForTesting = delegate;
        ResettersForTesting.register(() -> sDelegateForTesting = null);
    }

    static void overrideDefaultProbeUrlForTesting(String url) {
        sDefaultProbeUrl = url;
    }

    static void resetDefaultProbeUrlForTesting() {
        sDefaultProbeUrl = DEFAULT_PROBE_URL;
    }

    static void overrideFallbackProbeUrlForTesting(String url) {
        sFallbackProbeUrl = url;
    }

    static void resetFallbackProbeUrlForTesting() {
        sFallbackProbeUrl = FALLBACK_PROBE_URL;
    }

    static void overrideProbeMethodForTesting(String method) {
        sProbeMethod = method;
    }

    static void resetProbeMethodForTesting() {
        sProbeMethod = PROBE_METHOD;
    }

    @VisibleForTesting
    static void overrideConnectivityCheckInitialDelayMs(int delayMs) {
        sConnectivityCheckInitialDelayMs = delayMs;
    }

    void forceConnectionStateForTesting(@ConnectionState int connectionState) {
        mConnectionState = connectionState;
    }

    Handler getHandlerForTesting() {
        return mHandler;
    }

    void setUseDefaultUrlForTesting(boolean useDefaultUrl) {
        mConnectivityCheckingStage =
                useDefaultUrl
                        ? ConnectivityCheckingStage.PROBE_DEFAULT_URL
                        : ConnectivityCheckingStage.PROBE_FALLBACK_URL;
    }
}
