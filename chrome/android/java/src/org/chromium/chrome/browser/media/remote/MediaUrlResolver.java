// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import android.net.Uri;
import android.support.annotation.IntDef;
import android.text.TextUtils;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;

import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.HttpURLConnection;
import java.net.URISyntaxException;
import java.net.URL;
import java.net.URLStreamHandler;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Resolves the final URL if it's a redirect. Works asynchronously, uses HTTP
 * HEAD request to determine if the URL is redirected.
 */
public class MediaUrlResolver extends AsyncTask<MediaUrlResolver.Result> {
    // Cast.Sender.UrlResolveResult UMA histogram values; must match values of
    // RemotePlaybackUrlResolveResult in histograms.xml. Do not change these values, as they are
    // being used in UMA.
    @IntDef({ResolveResult.SUCCESS, ResolveResult.MALFORMED_URL, ResolveResult.NO_CORS,
            ResolveResult.INCOMPATIBLE_CORS, ResolveResult.SERVER_ERROR,
            ResolveResult.NETWORK_ERROR, ResolveResult.UNSUPPORTED_MEDIA,
            ResolveResult.HUC_EXCEPTION})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ResolveResult {
        int SUCCESS = 0;
        int MALFORMED_URL = 1;
        int NO_CORS = 2;
        int INCOMPATIBLE_CORS = 3;
        int SERVER_ERROR = 4;
        int NETWORK_ERROR = 5;
        int UNSUPPORTED_MEDIA = 6;
        int HUC_EXCEPTION = 7;
        int NUM_ENTRIES = 8;
    }

    // Acceptal response codes for URL resolving request.
    private static final Integer[] SUCCESS_RESPONSE_CODES = {
        // Request succeeded.
        HttpURLConnection.HTTP_OK,
        HttpURLConnection.HTTP_PARTIAL,

        // HttpURLConnection only follows up to 5 redirects, this response is unlikely but possible.
        HttpURLConnection.HTTP_MOVED_PERM,
        HttpURLConnection.HTTP_MOVED_TEMP,
    };

    /**
     * The interface to get the initial URI with cookies from and pass the final
     * URI to.
     */
    public interface Delegate {
        /**
         * @return the original URL to resolve.
         */
        Uri getUri();

        /**
         * @return the cookies to fetch the URL with.
         */
        String getCookies();

        /**
         * Passes the resolved URL to the delegate.
         *
         * @param uri the resolved URL.
         */
        void deliverResult(Uri uri, boolean palyable);
    }


    protected static final class Result {
        private final Uri mUri;
        private final boolean mPlayable;

        public Result(Uri uri, boolean playable) {
            mUri = uri;
            mPlayable = playable;
        }

        public Uri getUri() {
            return mUri;
        }

        public boolean isPlayable() {
            return mPlayable;
        }
    }

    private static final String TAG = "MediaFling";

    private static final String COOKIES_HEADER_NAME = "Cookies";
    private static final String USER_AGENT_HEADER_NAME = "User-Agent";
    private static final String ORIGIN_HEADER_NAME = "Origin";
    private static final String RANGE_HEADER_NAME = "Range";
    private static final String CORS_HEADER_NAME = "Access-Control-Allow-Origin";

    private static final String CHROMECAST_ORIGIN = "https://www.gstatic.com";

    // Media types supported for cast, see
    // media/base/container_names.h for the actual enum where these are defined.
    // See https://developers.google.com/cast/docs/media#media-container-formats for the formats
    // supported by Cast devices.
    @IntDef({MediaType.UNKNOWN, MediaType.AAC, MediaType.HLS, MediaType.MP3, MediaType.MPEG4,
            MediaType.OGG, MediaType.WAV, MediaType.WEBM, MediaType.DASH, MediaType.SMOOTHSTREAM})
    @Retention(RetentionPolicy.SOURCE)
    private @interface MediaType {
        int UNKNOWN = 0;
        int AAC = 1;
        int HLS = 22;
        int MP3 = 26;
        int MPEG4 = 29;
        int OGG = 30;
        int WAV = 35;
        int WEBM = 36;
        int DASH = 38;
        int SMOOTHSTREAM = 39;
    }

    // We don't want to necessarily fetch the whole video but we don't want to miss the CORS header.
    // Assume that 64k should be more than enough to keep all the headers.
    private static final String RANGE_HEADER_VALUE = "bytes=0-65536";

    private final Delegate mDelegate;

    private final String mUserAgent;
    private final URLStreamHandler mStreamHandler;

    /**
     * The constructor
     * @param delegate The customer for this URL resolver.
     * @param userAgent The browser user agent
     */
    public MediaUrlResolver(Delegate delegate, String userAgent) {
        this(delegate, userAgent, null);
    }

    @VisibleForTesting
    MediaUrlResolver(Delegate delegate, String userAgent, URLStreamHandler streamHandler) {
        mDelegate = delegate;
        mUserAgent = userAgent;
        mStreamHandler = streamHandler;
    }

    @Override
    protected MediaUrlResolver.Result doInBackground() {
        Uri uri = mDelegate.getUri();
        if (uri == null || uri.equals(Uri.EMPTY)) {
            return new MediaUrlResolver.Result(Uri.EMPTY, false);
        }
        String cookies = mDelegate.getCookies();

        Map<String, List<String>> headers = null;
        HttpURLConnection urlConnection = null;
        try {
            URL requestUrl = new URL(null, uri.toString(), mStreamHandler);
            urlConnection = (HttpURLConnection) requestUrl.openConnection();
            if (!TextUtils.isEmpty(cookies)) {
                urlConnection.setRequestProperty(COOKIES_HEADER_NAME, cookies);
            }

            // Pretend that this is coming from the Chromecast.
            urlConnection.setRequestProperty(ORIGIN_HEADER_NAME, CHROMECAST_ORIGIN);
            urlConnection.setRequestProperty(USER_AGENT_HEADER_NAME, mUserAgent);
            if (!isEnhancedMedia(uri)) {
                // Manifest files are typically smaller than 64K so range request can fail.
                urlConnection.setRequestProperty(RANGE_HEADER_NAME, RANGE_HEADER_VALUE);
            }

            // This triggers resolving the URL and receiving the headers.
            headers = urlConnection.getHeaderFields();

            uri = Uri.parse(urlConnection.getURL().toString());

            // If server's response is not valid, don't try to fling the video.
            int responseCode = urlConnection.getResponseCode();
            if (!Arrays.asList(SUCCESS_RESPONSE_CODES).contains(responseCode)) {
                recordResultHistogram(ResolveResult.SERVER_ERROR);
                Log.e(TAG, "Server response is not valid: %d", responseCode);
                uri = Uri.EMPTY;
            }
        } catch (IOException | IllegalArgumentException e) {
            // IllegalArgumentException for SSL issue (https://b/78588631).
            recordResultHistogram(ResolveResult.NETWORK_ERROR);
            Log.e(TAG, "Failed to fetch the final url", e);
            uri = Uri.EMPTY;
        } catch (ArrayIndexOutOfBoundsException | NullPointerException e) {
            recordResultHistogram(ResolveResult.HUC_EXCEPTION);
            Log.e(TAG, "Threading issue with HUC, see https://crbug.com/754480", e);
            uri = Uri.EMPTY;
        } catch (RuntimeException e) {
            if (e.getCause() instanceof URISyntaxException) {
                Log.e(TAG, "Invalid URL format", e);
                uri = Uri.EMPTY;
            } else {
                throw e;
            }
        } finally {
            if (urlConnection != null) urlConnection.disconnect();
        }
        return new MediaUrlResolver.Result(uri, canPlayMedia(uri, headers));
    }

    @Override
    protected void onPostExecute(MediaUrlResolver.Result result) {
        mDelegate.deliverResult(result.getUri(), result.isPlayable());
    }

    private boolean canPlayMedia(Uri uri, Map<String, List<String>> headers) {
        if (uri == null || uri.equals(Uri.EMPTY)) {
            recordResultHistogram(ResolveResult.MALFORMED_URL);
            return false;
        }

        if (headers != null && headers.containsKey(CORS_HEADER_NAME)) {
            // Check that the CORS data is valid for Chromecast
            List<String> corsData = headers.get(CORS_HEADER_NAME);
            if (corsData.isEmpty() || (!corsData.get(0).equals("*")
                    && !corsData.get(0).equals(CHROMECAST_ORIGIN))) {
                recordResultHistogram(ResolveResult.INCOMPATIBLE_CORS);
                return false;
            }
        } else if (isEnhancedMedia(uri)) {
            // HLS media requires CORS headers.
            // TODO(avayvod): it actually requires CORS on the final video URLs vs the manifest.
            // Clank assumes that if CORS is set for the manifest it's set for everything but
            // it not necessary always true. See b/19138712
            Log.d(TAG, "HLS stream without CORS header: %s", uri);
            recordResultHistogram(ResolveResult.NO_CORS);
            return false;
        }

        if (getMediaType(uri) == MediaType.UNKNOWN) {
            Log.d(TAG, "Unsupported media container format: %s", uri);
            recordResultHistogram(ResolveResult.UNSUPPORTED_MEDIA);
            return false;
        }

        recordResultHistogram(ResolveResult.SUCCESS);
        return true;
    }

    private boolean isEnhancedMedia(Uri uri) {
        int mediaType = getMediaType(uri);
        return mediaType == MediaType.HLS || mediaType == MediaType.DASH
                || mediaType == MediaType.SMOOTHSTREAM;
    }

    @VisibleForTesting
    void recordResultHistogram(@ResolveResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "Cast.Sender.UrlResolveResult", result, ResolveResult.NUM_ENTRIES);
    }

    static @MediaType int getMediaType(Uri uri) {
        String path = uri.getPath();

        if (path == null) return MediaType.UNKNOWN;

        path = path.toLowerCase(Locale.US);
        if (path.endsWith(".m3u8")) return MediaType.HLS;
        if (path.endsWith(".mp4")) return MediaType.MPEG4;
        if (path.endsWith(".mpd")) return MediaType.DASH;
        if (path.endsWith(".ism")) return MediaType.SMOOTHSTREAM;
        if (path.endsWith(".m4a") || path.endsWith(".aac")) return MediaType.AAC;
        if (path.endsWith(".mp3")) return MediaType.MP3;
        if (path.endsWith(".wav")) return MediaType.WAV;
        if (path.endsWith(".webm")) return MediaType.WEBM;
        if (path.endsWith(".ogg")) return MediaType.OGG;
        return MediaType.UNKNOWN;
    }
}
