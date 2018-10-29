// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertThat;

import android.net.Uri;

import com.google.android.collect.Lists;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.CommandLine;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandler;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Unit tests (run on host) for {@link org.chromium.chrome.browser.media.remote.MediaUrlResolver}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {CustomShadowAsyncTask.class})
public class MediaUrlResolverTest {
    // Constants copied from MediaUrlResolver. Don't use the copies in MediaUrlResolver
    // since we want the tests to detect if these are changed or corrupted.
    private static final String COOKIES_HEADER_NAME = "Cookies";
    private static final String CORS_HEADER_NAME = "Access-Control-Allow-Origin";
    private static final String RANGE_HEADER_NAME = "Range";
    private static final String RANGE_HEADER_VALUE = "bytes=0-65536";

    private static final String USER_AGENT_HEADER_NAME = "User-Agent";
    private static final String CHROMECAST_ORIGIN = "https://www.gstatic.com";

    private class TestDelegate implements MediaUrlResolver.Delegate {
        private final String mCookies;
        private final Uri mInputUri;

        private boolean mReturnedPlayable;
        private Uri mReturnedUri;

        TestDelegate(Uri uri, String cookies) {
            mInputUri = uri;
            mCookies = cookies;
        }

        @Override
        public String getCookies() {
            return mCookies;
        }

        @Override
        public Uri getUri() {
            return mInputUri;
        }

        @Override
        public void deliverResult(Uri uri, boolean playable) {
            mReturnedUri = uri;
            mReturnedPlayable = playable;
        }

        Uri getReturnedUri() {
            return mReturnedUri;
        }

        boolean isPlayable() {
            return mReturnedPlayable;
        }
    }

    /**
     * Dummy HttpURLConnection class that returns canned headers. Also prevents the test from going
     * out to the real network.
     */
    private class DummyUrlConnection extends HttpURLConnection {

        private final URL mUrl;

        protected DummyUrlConnection(URL u) {
            super(u);
            mUrl = u;
        }

        @Override
        public void connect() throws IOException {
            if (mThrowIOException) throw new IOException();
        }

        @Override
        public void disconnect() {
        }

        @Override
        public Map<String, List<String>> getHeaderFields() {
            return mReturnedHeaders;
        }

        @Override
        public URL getURL() {
            return mReturnedUrl == null ? mUrl : mReturnedUrl;
        }

        @Override
        public void setRequestProperty(String key, String value) {
            mRequestProperties.put(key, value);
        }

        @Override
        public boolean usingProxy() {
            return false;
        }

        @Override
        public int getResponseCode() {
            return mReturnedResponseCode;
        }

    }

    /**
     * Class for plugging in DummyUrlConnection
     */
    private class DummyUrlStreamHandler extends URLStreamHandler {

        @Override
        protected URLConnection openConnection(URL u) throws IOException {
            return new DummyUrlConnection(u);
        }

    }

    private Map<String, String> mRequestProperties;

    private Map<String, List<String>> mReturnedHeaders;

    private URL mReturnedUrl;

    private int mReturnedResponseCode;

    private boolean mThrowIOException;

    @Before
    public void setup() {
        // Initialize the command line to avoid a crash when the code checks the logging flag.
        CommandLine.init(new String[0]);
    }

    /**
     * Test method for {@link MediaUrlResolver#MediaUrlResolver} with valid MPEG4 URL
     *
     * @throws MalformedURLException
     */
    @Test
    public void testMediaUrlResolver_validMpeg4() throws MalformedURLException {
        // A valid mpeg4 URI is playable and unchanged.
        Uri uri = Uri.parse("http://example.com/test.mp4");
        TestDelegate delegate =  resolveUri(uri, null, 200, null, null, false);
        assertThat("A valid mp4 uri is unchanged", delegate.getReturnedUri(), equalTo(uri));
        assertThat("A valid mp4 uri is playable", delegate.isPlayable(), equalTo(true));

        // Check that the correct message was sent
        assertThat("The message contained the user agent name",
                mRequestProperties.get(USER_AGENT_HEADER_NAME), equalTo("User agent"));
        assertThat("The message contained the range header",
                mRequestProperties.get(RANGE_HEADER_NAME), equalTo(RANGE_HEADER_VALUE));
        assertThat("The message didn't contain any cookies",
                mRequestProperties.get(COOKIES_HEADER_NAME), nullValue());
    }

    /**
     * Test method for {@link MediaUrlResolver#MediaUrlResolver} testing a null URL.
     */
    @Test
    public void testMediaUrlResolver_nullUri() {
        // An null URL isn't playable
        TestDelegate delegate =  resolveUri(null, null, 200, null, null, false);

        assertThat("An empty uri remains empty", delegate.getReturnedUri(), equalTo(Uri.EMPTY));
        assertThat("An empty uri isn't playable", delegate.isPlayable(), equalTo(false));
    }

    /**
     * Test method for {@link MediaUrlResolver#MediaUrlResolver} testing empty URL.
     */
    @Test
    public void testMediaUrlResolver_emptyUri() {
        // An empty URL isn't playable
        TestDelegate delegate =  resolveUri(Uri.EMPTY, null, 200, null, null, false);

        assertThat("An empty uri remains empty", delegate.getReturnedUri(), equalTo(Uri.EMPTY));
        assertThat("An empty uri isn't playable", delegate.isPlayable(), equalTo(false));
    }

    /**
     * Test method for {@link MediaUrlResolver#MediaUrlResolver} setting the cookies
     *
     * @throws MalformedURLException
     */
    @Test
    public void testMediaUrlResolver_cookies() throws MalformedURLException {
        // Check that cookies are sent correctly.
        Uri uri = Uri.parse("http://example.com/test.mp4");
        TestDelegate delegate =  resolveUri(uri, "Cookies!", 200, null, null, false);

        assertThat("The message contained the cookies",
                mRequestProperties.get(COOKIES_HEADER_NAME), equalTo("Cookies!"));
    }

    /**
     * Test method for {@link MediaUrlResolver#MediaUrlResolver} with a valid HLS URL and
     * the Range-Request header.
     *
     * @throws MalformedURLException
     */
    @Test
    public void testMediaUrlResolver_validHLSNoRange() throws MalformedURLException {
        // Don't set range request header for manifest URLs like HLS.
        Uri uri = Uri.parse("http://example.com/test.m3u8");
        TestDelegate delegate =  resolveUri(uri, null, 200, null, null, false);
        assertThat("The message didn't have the range header",
                mRequestProperties.get(RANGE_HEADER_NAME), nullValue());
    }

    /**
     * Test method for {@link MediaUrlResolver#MediaUrlResolver} updating the URL in case of
     * redirects.
     *
     * @throws MalformedURLException
     */
    @Test
    public void testMediaUrlResolver_redirect() throws MalformedURLException {
        // A redirected URI is retuend and is playable.
        Uri uri = Uri.parse("http://example.com/test.mp4");
        URL returnedUri = new URL("http://cdn.example.com/foo/test.mp4");
        TestDelegate delegate =  resolveUri(uri, null, 200, returnedUri, null, false);

        assertThat("A redirected uri is returned",
                delegate.getReturnedUri(), equalTo(Uri.parse(returnedUri.toString())));
        assertThat("A redirected uri is playable", delegate.isPlayable(), equalTo(true));
    }

    /**
     * Test method for {@link MediaUrlResolver#MediaUrlResolver} testing bad response code.
     *
     * @throws MalformedURLException
     */
    @Test
    public void testMediaUrlResolver_serverError() {
        // A valid URL is unplayable and an empty URL is returned if the server request fails.
        Uri uri = Uri.parse("http://example.com/test.mp4");
        TestDelegate delegate =  resolveUri(uri, null, 404, null, null, false);

        assertThat("An empty uri is returned on server error",
                delegate.getReturnedUri(), equalTo(Uri.EMPTY));
        assertThat("Server error means unplayable uri", delegate.isPlayable(), equalTo(false));
    }

    /**
     * Test method for {@link MediaUrlResolver#MediaUrlResolver} when a network error happens.
     *
     * @throws MalformedURLException
     */
    @Test
    public void testMediaUrlResolver_networkError() throws MalformedURLException {
        // A random, non parsable, URI of unknown type is treated as not playable.
        Uri uri = Uri.parse("http://example.com/test.mp4");
        TestDelegate delegate =  resolveUri(uri, null, 404, null, null, true);

        assertThat("An empty uri is returned on network error",
                delegate.getReturnedUri(), equalTo(Uri.EMPTY));
        assertThat("Network error means unplayable uri", delegate.isPlayable(), equalTo(false));
    }

    /**
     * Test method for {@link MediaUrlResolver#MediaUrlResolver} with valid MPEG4 URL and compatible
     * CORS header in the response.
     *
     * @throws MalformedURLException
     */
    @Test
    public void testMediaUrlResolver_validMpeg4CompatibleCORS() throws MalformedURLException {
        // If a compatible CORS header returned, a valid mpeg4 URI is playable and unchanged.
        Uri uri = Uri.parse("http://example.com/test.mp4");
        HashMap<String, List<String>> corsHeaders = new HashMap<String, List<String>>();
        corsHeaders.put(CORS_HEADER_NAME, Lists.newArrayList(CHROMECAST_ORIGIN));
        TestDelegate delegate =  resolveUri(uri, null, 200, null, corsHeaders, false);
        assertThat("A valid mp4 uri with CORS is unchanged",
                delegate.getReturnedUri(), equalTo(uri));
        assertThat("A valid mp4 uri with CORS is playable", delegate.isPlayable(), equalTo(true));
    }

    /**
     * Test method for {@link MediaUrlResolver#MediaUrlResolver} with valid MPEG4 URL and
     * incompatible CORS header in the response.
     *
     * @throws MalformedURLException
     */
    @Test
    public void testMediaUrlResolver_validMpeg4InompatilbeCORS() throws MalformedURLException {
        // If an incompatible CORS header returned, a valid mpeg4 URI is not playable but unchanged.
        Uri uri = Uri.parse("http://example.com/test.mp4");
        HashMap<String, List<String>> corsHeaders = new HashMap<String, List<String>>();
        corsHeaders.put(CORS_HEADER_NAME, Lists.newArrayList("http://google.com"));

        TestDelegate delegate =  resolveUri(uri, null, 200, null, corsHeaders, false);
        assertThat("A valid mp4 uri with incompatible CORS is unchanged",
                delegate.getReturnedUri(), equalTo(uri));
        assertThat("A valid mp4 uri with incompatible CORS is not playable",
                delegate.isPlayable(), equalTo(false));
    }

    /**
     * Test method for {@link MediaUrlResolver#MediaUrlResolver} with a valid HLS URL and no CORS.
     *
     * @throws MalformedURLException
     */
    @Test
    public void testMediaUrlResolver_validHLSNoCORS() throws MalformedURLException {
        // A valid mpeg4 URI is playable and unchanged.
        Uri uri = Uri.parse("http://example.com/test.m3u8");
        TestDelegate delegate =  resolveUri(uri, null, 200, null, null, false);

        assertThat("A valid HLS uri with no CORS is unchanged",
                delegate.getReturnedUri(), equalTo(uri));
        assertThat("A valid HLS uri with no CORS is not playable",
                delegate.isPlayable(), equalTo(false));
    }

    /**
     * Test method for {@link MediaUrlResolver#MediaUrlResolver} with an unknown media type.
     *
     * @throws MalformedURLException
     */
    @Test
    public void testMediaUrlResolver_unknownMediaType() throws MalformedURLException {
        // A URI with an unknown media type is unchanged but not playable.
        Uri uri = Uri.parse("http://example.com/test.foo");
        TestDelegate delegate =  resolveUri(uri, null, 200, null, null, false);

        assertThat("A valid uri with unknown media type is unchanged",
                delegate.getReturnedUri(), equalTo(uri));
        assertThat("A valid uri with unknown media type is not playable",
                delegate.isPlayable(), equalTo(false));
    }

    private TestDelegate resolveUri(
            final Uri uri,
            final String cookies,
            int responseCode,
            URL returnedUrl,
            Map<String, List<String>> returnedHeaders,
            boolean throwIOException) {
        mReturnedResponseCode = responseCode;
        mReturnedUrl = returnedUrl;
        mReturnedHeaders = returnedHeaders == null ? new HashMap<String, List<String>>()
                : returnedHeaders;
        mRequestProperties = new HashMap<String, String>();
        mThrowIOException = throwIOException;

        TestDelegate delegate = new TestDelegate(uri, cookies);
        MediaUrlResolver resolver = new MediaUrlResolver(delegate, "User agent",
                new DummyUrlStreamHandler()) {
            // The RecordHistogram members are static, and call native, so we must prevent any calls
            // to them by overriding the wrapper.
            @Override
            void recordResultHistogram(int result) {
            }
        };
        resolver.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
        ShadowApplication.runBackgroundTasks();

        return delegate;
    }

}
