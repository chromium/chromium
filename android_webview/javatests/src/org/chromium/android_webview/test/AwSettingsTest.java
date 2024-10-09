// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.content.Context;
import android.graphics.Point;
import android.net.http.SslError;
import android.os.Build;
import android.os.SystemClock;
import android.view.WindowManager;
import android.webkit.JavascriptInterface;
import android.webkit.WebSettings;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwFeatureMap;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.AwSettings.LayoutAlgorithm;
import org.chromium.android_webview.ManifestMetadataUtil;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.android_webview.test.TestAwContentsClient.DoUpdateVisitedHistoryHelper;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.ImagePageGenerator;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.android_webview.test.util.VideoTestUtil;
import org.chromium.android_webview.test.util.VideoTestWebServer;
import org.chromium.base.Callback;
import org.chromium.base.FileUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.TestFileUtil;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.HistoryUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

import java.io.File;
import java.io.FileInputStream;
import java.net.URLEncoder;
import java.util.Collections;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * A test suite for AwSettings class. The key objective is to verify that each
 * settings applies either to each individual view or to all views of the
 * application
 */
@RunWith(AwJUnit4ClassRunner.class)
@CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
@Batch(Batch.PER_CLASS)
public class AwSettingsTest {
    @Rule
    public AwActivityTestRule mActivityTestRule =
            new AwActivityTestRule() {
                @Override
                public TestDependencyFactory createTestDependencyFactory() {
                    if (mOverriddenFactory == null) {
                        return new TestDependencyFactory();
                    } else {
                        return mOverriddenFactory;
                    }
                }
            };

    private static final boolean ENABLED = true;
    private static final boolean DISABLED = false;

    private int mTitleIdx;

    /**
     * A helper class for testing a particular preference from AwSettings.
     * The generic type T is the type of the setting. Usually, to test an
     * effect of the preference, JS code is executed that sets document's title.
     * In this case, requiresJsEnabled constructor argument must be set to true.
     */
    abstract class AwSettingsTestHelper<T> {
        protected final AwContents mAwContents;
        protected final Context mContext;
        protected final TestAwContentsClient mContentViewClient;
        protected final AwSettings mAwSettings;

        AwSettingsTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient,
                boolean requiresJsEnabled)
                throws Throwable {
            mAwContents = containerView.getAwContents();
            mContext = containerView.getContext();
            mContentViewClient = contentViewClient;
            mAwSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
            mAwSettings.setAllowFileAccess(true);
            if (requiresJsEnabled) {
                mAwSettings.setJavaScriptEnabled(true);
            }
        }

        void ensureSettingHasAlteredValue() throws Throwable {
            ensureSettingHasValue(getAlteredValue());
        }

        void ensureSettingHasInitialValue() throws Throwable {
            ensureSettingHasValue(getInitialValue());
        }

        void setAlteredSettingValue() throws Throwable {
            setCurrentValue(getAlteredValue());
        }

        void setInitialSettingValue() throws Throwable {
            setCurrentValue(getInitialValue());
        }

        protected abstract T getAlteredValue();

        protected abstract T getInitialValue();

        protected abstract T getCurrentValue();

        protected abstract void setCurrentValue(T value) throws Throwable;

        protected abstract void doEnsureSettingHasValue(T value) throws Throwable;

        protected String getTitleOnUiThread() throws Exception {
            return mActivityTestRule.getTitleOnUiThread(mAwContents);
        }

        protected void loadDataSync(String data) throws Throwable {
            mActivityTestRule.loadDataSync(
                    mAwContents,
                    mContentViewClient.getOnPageFinishedHelper(),
                    data,
                    "text/html",
                    false);
        }

        protected void loadUrlSync(String url) throws Throwable {
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentViewClient.getOnPageFinishedHelper(), url);
        }

        protected void loadUrlSyncAndExpectError(String url) throws Throwable {
            mActivityTestRule.loadUrlSyncAndExpectError(
                    mAwContents,
                    mContentViewClient.getOnPageFinishedHelper(),
                    mContentViewClient.getOnReceivedErrorHelper(),
                    url);
        }

        protected String executeJavaScriptAndWaitForResult(String script) throws Exception {
            return executeJavaScriptAndWaitForResult(script, /* shouldCheckSettings= */ true);
        }

        protected String executeJavaScriptAndWaitForResult(
                String script, boolean shouldCheckSettings) throws Exception {
            return mActivityTestRule.executeJavaScriptAndWaitForResult(
                    mAwContents, mContentViewClient, script, shouldCheckSettings);
        }

        private void ensureSettingHasValue(T value) throws Throwable {
            Assert.assertEquals(value, getCurrentValue());
            doEnsureSettingHasValue(value);
        }
    }

    class AwSettingsJavaScriptTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String JS_ENABLED_STRING = "JS Enabled";
        private static final String JS_DISABLED_STRING = "JS Disabled";

        AwSettingsJavaScriptTestHelper(
                AwTestContainerView containerView, TestAwContentsClient contentViewClient)
                throws Throwable {
            super(containerView, contentViewClient, false);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getJavaScriptEnabled();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setJavaScriptEnabled(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadDataSync(getData());
            Assert.assertEquals(
                    value == ENABLED ? JS_ENABLED_STRING : JS_DISABLED_STRING,
                    getTitleOnUiThread());
        }

        private String getData() {
            return "<html><head><title>"
                    + JS_DISABLED_STRING
                    + "</title>"
                    + "</head><body onload=\"document.title='"
                    + JS_ENABLED_STRING
                    + "';\"></body></html>";
        }
    }

    // In contrast to AwSettingsJavaScriptTestHelper, doesn't reload the page when testing
    // JavaScript state.
    class AwSettingsJavaScriptDynamicTestHelper extends AwSettingsJavaScriptTestHelper {
        AwSettingsJavaScriptDynamicTestHelper(
                AwTestContainerView containerView, TestAwContentsClient contentViewClient)
                throws Throwable {
            super(containerView, contentViewClient);
            // Load the page.
            super.doEnsureSettingHasValue(getInitialValue());
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            String oldTitle = getTitleOnUiThread();
            String newTitle = oldTitle + "_modified";
            // Do not check if JavaScript is enabled, since the point of this test is to verify that
            // when JavaScript is disabled the script does not execute and cannot change the title.
            executeJavaScriptAndWaitForResult(
                    getScript(newTitle), /* shouldCheckSettings= */ false);
            Assert.assertEquals(value == ENABLED ? newTitle : oldTitle, getTitleOnUiThread());
        }

        private String getScript(String title) {
            return "document.title='" + title + "';";
        }
    }

    class AwSettingsStandardFontFamilyTestHelper extends AwSettingsTestHelper<String> {
        AwSettingsStandardFontFamilyTestHelper(
                AwTestContainerView containerView, TestAwContentsClient contentViewClient)
                throws Throwable {
            super(containerView, contentViewClient, true);
        }

        @Override
        protected String getAlteredValue() {
            return "cursive";
        }

        @Override
        protected String getInitialValue() {
            return "sans-serif";
        }

        @Override
        protected String getCurrentValue() {
            return mAwSettings.getStandardFontFamily();
        }

        @Override
        protected void setCurrentValue(String value) {
            mAwSettings.setStandardFontFamily(value);
        }

        @Override
        protected void doEnsureSettingHasValue(String value) throws Throwable {
            loadDataSync(getData());
            Assert.assertEquals(value, getTitleOnUiThread());
        }

        private String getData() {
            return "<html><body onload=\"document.title = "
                    + "getComputedStyle(document.body).getPropertyValue('font-family');\">"
                    + "</body></html>";
        }
    }

    class AwSettingsDefaultFontSizeTestHelper extends AwSettingsTestHelper<Integer> {
        AwSettingsDefaultFontSizeTestHelper(
                AwTestContainerView containerView, TestAwContentsClient contentViewClient)
                throws Throwable {
            super(containerView, contentViewClient, true);
        }

        @Override
        protected Integer getAlteredValue() {
            return 42;
        }

        @Override
        protected Integer getInitialValue() {
            return 16;
        }

        @Override
        protected Integer getCurrentValue() {
            return mAwSettings.getDefaultFontSize();
        }

        @Override
        protected void setCurrentValue(Integer value) {
            mAwSettings.setDefaultFontSize(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Integer value) throws Throwable {
            loadDataSync(getData());
            Assert.assertEquals(value.toString() + "px", getTitleOnUiThread());
        }

        private String getData() {
            return "<html><body onload=\"document.title = "
                    + "getComputedStyle(document.body).getPropertyValue('font-size');\">"
                    + "</body></html>";
        }
    }

    class AwSettingsLoadImagesAutomaticallyTestHelper extends AwSettingsTestHelper<Boolean> {
        private ImagePageGenerator mGenerator;

        AwSettingsLoadImagesAutomaticallyTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient,
                ImagePageGenerator generator)
                throws Throwable {
            super(containerView, contentViewClient, true);
            mGenerator = generator;
        }

        @Override
        protected Boolean getAlteredValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getLoadsImagesAutomatically();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setLoadsImagesAutomatically(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadDataSync(mGenerator.getPageSource());
            Assert.assertEquals(
                    value == ENABLED
                            ? ImagePageGenerator.IMAGE_LOADED_STRING
                            : ImagePageGenerator.IMAGE_NOT_LOADED_STRING,
                    getTitleOnUiThread());
        }
    }

    class AwSettingsImagesEnabledHelper extends AwSettingsTestHelper<Boolean> {

        AwSettingsImagesEnabledHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient,
                TestWebServer webServer,
                ImagePageGenerator generator)
                throws Throwable {
            super(containerView, contentViewClient, true);
            mWebServer = webServer;
            mGenerator = generator;
        }

        @Override
        protected Boolean getAlteredValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getImagesEnabled();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setImagesEnabled(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            final String httpImageUrl = mGenerator.getPageUrl(mWebServer);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentViewClient.getOnPageFinishedHelper(), httpImageUrl);
            Assert.assertEquals(
                    value == ENABLED
                            ? ImagePageGenerator.IMAGE_LOADED_STRING
                            : ImagePageGenerator.IMAGE_NOT_LOADED_STRING,
                    getTitleOnUiThread());
        }

        private TestWebServer mWebServer;
        private ImagePageGenerator mGenerator;
    }

    class AwSettingsDefaultTextEncodingTestHelper extends AwSettingsTestHelper<String> {
        AwSettingsDefaultTextEncodingTestHelper(
                AwTestContainerView containerView, TestAwContentsClient contentViewClient)
                throws Throwable {
            super(containerView, contentViewClient, true);
        }

        // A string which can be encoded by UTF-8 charset but not by Latin-1 charset. Translates to
        // "Hello world."
        private static final String NON_LATIN_TEXT = "你好世界";

        @Override
        protected String getAlteredValue() {
            return "Latin-1";
        }

        @Override
        protected String getInitialValue() {
            return "UTF-8";
        }

        @Override
        protected String getCurrentValue() {
            return mAwSettings.getDefaultTextEncodingName();
        }

        @Override
        protected void setCurrentValue(String value) {
            mAwSettings.setDefaultTextEncodingName(value);
        }

        @Override
        protected void doEnsureSettingHasValue(String value) throws Throwable {
            loadDataSync(getData());

            if ("UTF-8".equals(value)) {
                Assert.assertEquals(
                        "Title should be decoded correctly when charset is UTF-8",
                        NON_LATIN_TEXT,
                        getTitleOnUiThread());
            } else {
                // The content seems to decode as "ä½ å¥½ä¸–ç•Œ", but it's sufficient to just
                // enforce the text decodes incorrectly.
                Assert.assertNotEquals(
                        "Title should be garbled (decoded incorrectly) when charset is Latin-1",
                        NON_LATIN_TEXT,
                        getTitleOnUiThread());
            }
        }

        private String getData() {
            return "<html><body onload='document.title=\"" + NON_LATIN_TEXT + "\"'></body></html>";
        }
    }

    class AwSettingsUserAgentStringTestHelper extends AwSettingsTestHelper<String> {
        private final String mDefaultUa;
        private static final String DEFAULT_UA = "";
        private static final String CUSTOM_UA = "ChromeViewTest";

        AwSettingsUserAgentStringTestHelper(
                AwTestContainerView containerView, TestAwContentsClient contentViewClient)
                throws Throwable {
            super(containerView, contentViewClient, true);
            mDefaultUa = mAwSettings.getUserAgentString();
        }

        @Override
        protected String getAlteredValue() {
            return CUSTOM_UA;
        }

        @Override
        protected String getInitialValue() {
            return DEFAULT_UA;
        }

        @Override
        protected String getCurrentValue() {
            // The test framework expects that getXXX() == Z after setXXX(Z).
            // But setUserAgentString("" / null) resets the UA string to default,
            // and getUserAgentString returns the default UA string afterwards.
            // To align with the framework, we return an empty string instead of
            // the default UA.
            String currentUa = mAwSettings.getUserAgentString();
            return mDefaultUa.equals(currentUa) ? DEFAULT_UA : currentUa;
        }

        @Override
        protected void setCurrentValue(String value) {
            mAwSettings.setUserAgentString(value);
        }

        @Override
        protected void doEnsureSettingHasValue(String value) throws Throwable {
            loadDataSync(getData());
            Assert.assertEquals(
                    DEFAULT_UA.equals(value) ? mDefaultUa : value, getTitleOnUiThread());
        }

        private String getData() {
            return "<html><body onload='document.title=navigator.userAgent'></body></html>";
        }
    }

    class AwSettingsDomStorageEnabledTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String TEST_FILE = "android_webview/test/data/localStorage.html";
        private static final String NO_LOCAL_STORAGE = "No localStorage";
        private static final String HAS_LOCAL_STORAGE = "Has localStorage";

        AwSettingsDomStorageEnabledTestHelper(
                AwTestContainerView containerView, TestAwContentsClient contentViewClient)
                throws Throwable {
            super(containerView, contentViewClient, true);
            AwSettingsTest.assertFileIsReadable(UrlUtils.getIsolatedTestFilePath(TEST_FILE));
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getDomStorageEnabled();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setDomStorageEnabled(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            // It is not permitted to access localStorage from data URLs in WebKit,
            // that is why a standalone page must be used.
            loadUrlSync(UrlUtils.getIsolatedTestFileUrl(TEST_FILE));
            Assert.assertEquals(
                    value == ENABLED ? HAS_LOCAL_STORAGE : NO_LOCAL_STORAGE, getTitleOnUiThread());
        }
    }

    class AwSettingsDatabaseTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String TEST_FILE = "android_webview/test/data/database_access.html";
        private static final String NO_DATABASE = "No database";
        private static final String HAS_DATABASE = "Has database";

        AwSettingsDatabaseTestHelper(
                AwTestContainerView containerView, TestAwContentsClient contentViewClient)
                throws Throwable {
            super(containerView, contentViewClient, true);
            AwSettingsTest.assertFileIsReadable(UrlUtils.getIsolatedTestFilePath(TEST_FILE));
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getDatabaseEnabled();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setDatabaseEnabled(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            // It seems accessing the database through a data scheme is not
            // supported, and fails with a DOM exception (likely a cross-domain
            // violation).
            loadUrlSync(UrlUtils.getIsolatedTestFileUrl(TEST_FILE));
            Assert.assertEquals(
                    value == ENABLED ? HAS_DATABASE : NO_DATABASE, getTitleOnUiThread());
        }
    }

    class AwSettingsUniversalAccessFromFilesTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String TEST_CONTAINER_FILE =
                "android_webview/test/data/iframe_access.html";
        private static final String TEST_FILE = "android_webview/test/data/hello_world.html";
        private static final String ACCESS_DENIED_TITLE = "Exception";

        AwSettingsUniversalAccessFromFilesTestHelper(
                AwTestContainerView containerView, TestAwContentsClient contentViewClient)
                throws Throwable {
            super(containerView, contentViewClient, true);
            AwSettingsTest.assertFileIsReadable(
                    UrlUtils.getIsolatedTestFilePath(TEST_CONTAINER_FILE));
            AwSettingsTest.assertFileIsReadable(UrlUtils.getIsolatedTestFilePath(TEST_FILE));
            mIframeContainerUrl = UrlUtils.getIsolatedTestFileUrl(TEST_CONTAINER_FILE);
            mIframeUrl = UrlUtils.getIsolatedTestFileUrl(TEST_FILE);
            // The value of the setting depends on the SDK version.
            mAwSettings.setAllowUniversalAccessFromFileURLs(false);
            // If universal access is true, the value of file access doesn't
            // matter. While if universal access is false, having file access
            // enabled will allow file loading.
            mAwSettings.setAllowFileAccessFromFileURLs(false);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getAllowUniversalAccessFromFileURLs();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setAllowUniversalAccessFromFileURLs(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadUrlSync(mIframeContainerUrl);
            Assert.assertEquals(
                    value == ENABLED ? mIframeUrl : ACCESS_DENIED_TITLE, getTitleOnUiThread());
        }

        private final String mIframeContainerUrl;
        private final String mIframeUrl;
    }

    class AwSettingsFileAccessFromFilesIframeTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String TEST_CONTAINER_FILE =
                "android_webview/test/data/iframe_access.html";
        private static final String TEST_FILE = "android_webview/test/data/hello_world.html";
        private static final String ACCESS_DENIED_TITLE = "Exception";

        AwSettingsFileAccessFromFilesIframeTestHelper(
                AwTestContainerView containerView, TestAwContentsClient contentViewClient)
                throws Throwable {
            super(containerView, contentViewClient, true);
            AwSettingsTest.assertFileIsReadable(
                    UrlUtils.getIsolatedTestFilePath(TEST_CONTAINER_FILE));
            AwSettingsTest.assertFileIsReadable(UrlUtils.getIsolatedTestFilePath(TEST_FILE));
            mIframeContainerUrl = UrlUtils.getIsolatedTestFileUrl(TEST_CONTAINER_FILE);
            mIframeUrl = UrlUtils.getIsolatedTestFileUrl(TEST_FILE);
            mAwSettings.setAllowUniversalAccessFromFileURLs(false);
            // The value of the setting depends on the SDK version.
            mAwSettings.setAllowFileAccessFromFileURLs(false);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getAllowFileAccessFromFileURLs();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setAllowFileAccessFromFileURLs(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadUrlSync(mIframeContainerUrl);
            Assert.assertEquals(
                    value == ENABLED ? mIframeUrl : ACCESS_DENIED_TITLE, getTitleOnUiThread());
        }

        private final String mIframeContainerUrl;
        private final String mIframeUrl;
    }

    class AwSettingsFileAccessFromFilesXhrTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String TEST_FILE = "android_webview/test/data/xhr_access.html";
        private static final String ACCESS_GRANTED_TITLE = "Hello, World!";
        private static final String ACCESS_DENIED_TITLE = "Exception";

        AwSettingsFileAccessFromFilesXhrTestHelper(
                AwTestContainerView containerView, TestAwContentsClient contentViewClient)
                throws Throwable {
            super(containerView, contentViewClient, true);
            assertFileIsReadable(UrlUtils.getIsolatedTestFilePath(TEST_FILE));
            mXhrContainerUrl = UrlUtils.getIsolatedTestFileUrl(TEST_FILE);
            mAwSettings.setAllowUniversalAccessFromFileURLs(false);
            // The value of the setting depends on the SDK version.
            mAwSettings.setAllowFileAccessFromFileURLs(false);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getAllowFileAccessFromFileURLs();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setAllowFileAccessFromFileURLs(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadUrlSync(mXhrContainerUrl);
            Assert.assertEquals(
                    value == ENABLED ? ACCESS_GRANTED_TITLE : ACCESS_DENIED_TITLE,
                    getTitleOnUiThread());
        }

        private final String mXhrContainerUrl;
    }

    class AwSettingsFileUrlAccessTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String TEST_FILE = "android_webview/test/data/hello_world.html";
        private static final String ACCESS_GRANTED_TITLE = "Hello, World!";

        AwSettingsFileUrlAccessTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient,
                int startIndex)
                throws Throwable {
            super(containerView, contentViewClient, true);
            mIndex = startIndex;
            AwSettingsTest.assertFileIsReadable(UrlUtils.getIsolatedTestFilePath(TEST_FILE));
        }

        @Override
        protected Boolean getAlteredValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getAllowFileAccess();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setAllowFileAccess(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            // Use query parameters to avoid hitting a cached page.
            String fileUrl = UrlUtils.getIsolatedTestFileUrl(TEST_FILE + "?id=" + mIndex);
            mIndex += 2;
            if (value == ENABLED) {
                loadUrlSync(fileUrl);
                Assert.assertEquals(ACCESS_GRANTED_TITLE, getTitleOnUiThread());
            } else {
                loadUrlSyncAndExpectError(fileUrl);
            }
        }

        private int mIndex;
    }

    class AwSettingsContentUrlAccessTestHelper extends AwSettingsTestHelper<Boolean> {

        AwSettingsContentUrlAccessTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient,
                int index)
                throws Throwable {
            super(containerView, contentViewClient, true);
            mTarget = "content_access_" + index;
        }

        @Override
        protected Boolean getAlteredValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getAllowContentAccess();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setAllowContentAccess(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            resetResourceRequestCountInContentProvider(mTarget);
            if (value == ENABLED) {
                loadUrlSync(createContentUrl(mTarget));
                String title = getTitleOnUiThread();
                Assert.assertNotNull(title);
                Assert.assertTrue(
                        "[" + mTarget + "] Actual title: \"" + title + "\"",
                        title.contains(mTarget));
                ensureResourceRequestCountInContentProvider(mTarget, 1);
            } else {
                loadUrlSyncAndExpectError(createContentUrl(mTarget));
                ensureResourceRequestCountInContentProvider(mTarget, 0);
            }
        }

        private final String mTarget;
    }

    class AwSettingsContentUrlAccessFromFileTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String TARGET = "content_from_file";

        AwSettingsContentUrlAccessFromFileTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient,
                int index)
                throws Throwable {
            super(containerView, contentViewClient, true);
            mIndex = index;
            mTempDir =
                    InstrumentationRegistry.getInstrumentation()
                            .getTargetContext()
                            .getCacheDir()
                            .getPath();
        }

        @Override
        protected Boolean getAlteredValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getAllowContentAccess();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setAllowContentAccess(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            resetResourceRequestCountInContentProvider(TARGET);
            final String fileName = mTempDir + "/" + TARGET + ".html";
            try {
                TestFileUtil.createNewHtmlFile(
                        fileName,
                        TARGET,
                        "<img src=\""
                                // Adding a query avoids hitting a cached image, and also verifies
                                // that content URL query parameters are ignored when accessing
                                // a content provider.
                                + createContentUrl(TARGET + "?id=" + mIndex)
                                + "\">");
                mIndex += 2;
                loadUrlSync("file://" + fileName);
                if (value == ENABLED) {
                    ensureResourceRequestCountInContentProvider(TARGET, 1);
                } else {
                    ensureResourceRequestCountInContentProvider(TARGET, 0);
                }
            } finally {
                TestFileUtil.deleteFile(fileName);
            }
        }

        private int mIndex;
        private String mTempDir;
    }

    // This class provides helper methods for testing of settings related to
    // the text autosizing feature.
    abstract class AwSettingsTextAutosizingTestHelper<T> extends AwSettingsTestHelper<T> {
        protected static final float PARAGRAPH_FONT_SIZE = 14.0f;

        AwSettingsTextAutosizingTestHelper(
                AwTestContainerView containerView, TestAwContentsClient contentViewClient)
                throws Throwable {
            super(containerView, contentViewClient, true);
            mNeedToWaitForFontSizeChange = false;
            loadDataSync(getData());
        }

        @Override
        protected void setCurrentValue(T value) throws Throwable {
            mNeedToWaitForFontSizeChange = false;
            if (value != getCurrentValue()) {
                mOldFontSize = getActualFontSize();
                mNeedToWaitForFontSizeChange = true;
            }
        }

        protected float getActualFontSize() throws Throwable {
            if (!mNeedToWaitForFontSizeChange) {
                executeJavaScriptAndWaitForResult("setTitleToActualFontSize()");
            } else {
                final float oldFontSize = mOldFontSize;
                AwActivityTestRule.pollInstrumentationThread(
                        () -> {
                            executeJavaScriptAndWaitForResult("setTitleToActualFontSize()");
                            float newFontSize = Float.parseFloat(getTitleOnUiThread());
                            return newFontSize != oldFontSize;
                        });
                mNeedToWaitForFontSizeChange = false;
            }
            return Float.parseFloat(getTitleOnUiThread());
        }

        protected String getData() {
            int displayWidth = calcDisplayWidthDp(mContext);
            int layoutWidth = (int) (displayWidth * 2.5f); // Use 2.5 as autosizing layout tests do.
            StringBuilder sb = new StringBuilder();
            sb.append(
                    "<html>"
                            + "<head>"
                            + "<meta name=\"viewport\" content=\"width="
                            + layoutWidth
                            + "\">"
                            + "<style>"
                            + "body { width: "
                            + layoutWidth
                            + "px; margin: 0; overflow-y: hidden; }"
                            + "</style>"
                            + "<script>"
                            + "function setTitleToActualFontSize() {"
                            // parseFloat is used to trim out the "px" suffix.
                            + "  document.title = parseFloat(getComputedStyle("
                            + "    document.getElementById('par')).getPropertyValue('font-size'));"
                            + "}</script></head>"
                            + "<body>"
                            + "<p id=\"par\" style=\"font-size:");
            sb.append(PARAGRAPH_FONT_SIZE);
            sb.append("px;\">");
            // Make the paragraph wide enough for being processed by the font autosizer.
            for (int i = 0; i < 500; i++) {
                sb.append("Hello, World! ");
            }
            sb.append("</p></body></html>");
            return sb.toString();
        }

        private boolean mNeedToWaitForFontSizeChange;
        private float mOldFontSize;
    }

    class AwSettingsLayoutAlgorithmTestHelper extends AwSettingsTextAutosizingTestHelper<Integer> {
        AwSettingsLayoutAlgorithmTestHelper(
                AwTestContainerView containerView, TestAwContentsClient contentViewClient)
                throws Throwable {
            super(containerView, contentViewClient);
            // Font autosizing doesn't step in for narrow layout widths.
            mAwSettings.setUseWideViewPort(true);
        }

        @LayoutAlgorithm
        @Override
        protected Integer getAlteredValue() {
            return AwSettings.LAYOUT_ALGORITHM_TEXT_AUTOSIZING;
        }

        @LayoutAlgorithm
        @Override
        protected Integer getInitialValue() {
            return AwSettings.LAYOUT_ALGORITHM_NARROW_COLUMNS;
        }

        @LayoutAlgorithm
        @Override
        protected Integer getCurrentValue() {
            return mAwSettings.getLayoutAlgorithm();
        }

        @Override
        protected void setCurrentValue(@LayoutAlgorithm Integer value) throws Throwable {
            super.setCurrentValue(value);
            mAwSettings.setLayoutAlgorithm(value);
        }

        @Override
        protected void doEnsureSettingHasValue(@LayoutAlgorithm Integer value) throws Throwable {
            final float actualFontSize = getActualFontSize();
            if (value == AwSettings.LAYOUT_ALGORITHM_TEXT_AUTOSIZING) {
                Assert.assertFalse(
                        "Actual font size: " + actualFontSize,
                        actualFontSize == PARAGRAPH_FONT_SIZE);
            } else {
                Assert.assertTrue(
                        "Actual font size: " + actualFontSize,
                        actualFontSize == PARAGRAPH_FONT_SIZE);
            }
        }
    }

    class AwSettingsTextZoomTestHelper extends AwSettingsTextAutosizingTestHelper<Integer> {
        private static final int INITIAL_TEXT_ZOOM = 100;
        private final float mInitialActualFontSize;

        AwSettingsTextZoomTestHelper(
                AwTestContainerView containerView, TestAwContentsClient contentViewClient)
                throws Throwable {
            super(containerView, contentViewClient);
            mInitialActualFontSize = getActualFontSize();
        }

        @Override
        protected Integer getAlteredValue() {
            return INITIAL_TEXT_ZOOM * 2;
        }

        @Override
        protected Integer getInitialValue() {
            return INITIAL_TEXT_ZOOM;
        }

        @Override
        protected Integer getCurrentValue() {
            return mAwSettings.getTextZoom();
        }

        @Override
        protected void setCurrentValue(Integer value) throws Throwable {
            super.setCurrentValue(value);
            mAwSettings.setTextZoom(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Integer value) throws Throwable {
            final float actualFontSize = getActualFontSize();
            // Ensure that actual vs. initial font size ratio is similar to actual vs. initial
            // text zoom values ratio.
            final float ratiosDelta =
                    Math.abs(
                            (actualFontSize / mInitialActualFontSize)
                                    - (value / (float) INITIAL_TEXT_ZOOM));
            Assert.assertTrue(
                    "|("
                            + actualFontSize
                            + " / "
                            + mInitialActualFontSize
                            + ") - ("
                            + value
                            + " / "
                            + INITIAL_TEXT_ZOOM
                            + ")| = "
                            + ratiosDelta,
                    ratiosDelta <= 0.2f);
        }
    }

    class AwSettingsTextZoomAutosizingTestHelper
            extends AwSettingsTextAutosizingTestHelper<Integer> {
        private static final int INITIAL_TEXT_ZOOM = 100;
        private final float mInitialActualFontSize;

        AwSettingsTextZoomAutosizingTestHelper(
                AwTestContainerView containerView, TestAwContentsClient contentViewClient)
                throws Throwable {
            super(containerView, contentViewClient);
            mAwSettings.setLayoutAlgorithm(AwSettings.LAYOUT_ALGORITHM_TEXT_AUTOSIZING);
            // The initial font size can be adjusted by font autosizer depending on the page's
            // viewport width.
            mInitialActualFontSize = getActualFontSize();
        }

        @Override
        protected Integer getAlteredValue() {
            return INITIAL_TEXT_ZOOM * 2;
        }

        @Override
        protected Integer getInitialValue() {
            return INITIAL_TEXT_ZOOM;
        }

        @Override
        protected Integer getCurrentValue() {
            return mAwSettings.getTextZoom();
        }

        @Override
        protected void setCurrentValue(Integer value) throws Throwable {
            super.setCurrentValue(value);
            mAwSettings.setTextZoom(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Integer value) throws Throwable {
            final float actualFontSize = getActualFontSize();
            // Ensure that actual vs. initial font size ratio is similar to actual vs. initial
            // text zoom values ratio.
            final float ratiosDelta =
                    Math.abs(
                            (actualFontSize / mInitialActualFontSize)
                                    - (value / (float) INITIAL_TEXT_ZOOM));
            Assert.assertTrue(
                    "|("
                            + actualFontSize
                            + " / "
                            + mInitialActualFontSize
                            + ") - ("
                            + value
                            + " / "
                            + INITIAL_TEXT_ZOOM
                            + ")| = "
                            + ratiosDelta,
                    ratiosDelta <= 0.2f);
        }
    }

    class AwSettingsJavaScriptPopupsTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String POPUP_ENABLED = "Popup enabled";
        private static final String POPUP_BLOCKED = "Popup blocked";

        AwSettingsJavaScriptPopupsTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient,
                boolean openTwice)
                throws Throwable {
            super(containerView, contentViewClient, true);
            mOpenTwice = openTwice;
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getJavaScriptCanOpenWindowsAutomatically();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setJavaScriptCanOpenWindowsAutomatically(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadDataSync(getData());
            final boolean expectPopupEnabled = value;
            AwActivityTestRule.pollInstrumentationThread(
                    () -> {
                        String title = getTitleOnUiThread();
                        // When popup is enabled, expect the title to be either POPUP_ENABLED or
                        // "about:blank". The latter is possible if the document.write() that sets
                        // the title finishes before the "about:blank" navigation commits. After
                        // that navigation commits, the title will be set to "about:blank".
                        return expectPopupEnabled
                                ? (POPUP_ENABLED.equals(title) || "about:blank".equals(title))
                                : POPUP_BLOCKED.equals(title);
                    });
            String title = getTitleOnUiThread();
            Assert.assertTrue(
                    value
                            ? (POPUP_ENABLED.equals(title) || "about:blank".equals(title))
                            : POPUP_BLOCKED.equals(title));
        }

        private String getData() {
            return "<html><head>"
                    + "<script>"
                    + "    function tryOpenWindow() {"
                    + "        var newWindow = window.open('about:blank');"
                    + (mOpenTwice ? "newWindow = window.open('about:blank');" : "")
                    + "        if (newWindow) {"
                    + "            if (newWindow === window) {"
                    + "                if (newWindow.opener != null) {"
                    + "                    newWindow.document.write("
                    + "                        '<html><head><title>"
                    + POPUP_ENABLED
                    + "</title></head></html>');"
                    + "                } else {"
                    + "                    newWindow.document.write('failed to set opener');"
                    + "                }"
                    + "            } else {"
                    + "                document.title = '"
                    + POPUP_ENABLED
                    + "';"
                    + "            }"
                    + "        } else {"
                    + "          document.title = '"
                    + POPUP_BLOCKED
                    + "';"
                    + "        }"
                    + "    }"
                    + "</script></head>"
                    + "<body onload='tryOpenWindow()'></body></html>";
        }

        private boolean mOpenTwice;
    }

    class AwSettingsCacheModeTestHelper extends AwSettingsTestHelper<Integer> {

        AwSettingsCacheModeTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient,
                int index,
                TestWebServer webServer)
                throws Throwable {
            super(containerView, contentViewClient, true);
            mIndex = index;
            mWebServer = webServer;
        }

        @Override
        protected Integer getAlteredValue() {
            // We use the value that results in a behaviour completely opposite to default.
            return WebSettings.LOAD_CACHE_ONLY;
        }

        @Override
        protected Integer getInitialValue() {
            return WebSettings.LOAD_DEFAULT;
        }

        @Override
        protected Integer getCurrentValue() {
            return mAwSettings.getCacheMode();
        }

        @Override
        protected void setCurrentValue(Integer value) {
            mAwSettings.setCacheMode(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Integer value) throws Throwable {
            final String htmlPath = "/cache_mode_" + mIndex + ".html";
            mIndex += 2;
            final String url = mWebServer.setResponse(htmlPath, "response", null);
            Assert.assertEquals(0, mWebServer.getRequestCount(htmlPath));
            if (value == WebSettings.LOAD_DEFAULT) {
                loadUrlSync(url);
                Assert.assertEquals(1, mWebServer.getRequestCount(htmlPath));
            } else {
                loadUrlSyncAndExpectError(url);
                Assert.assertEquals(0, mWebServer.getRequestCount(htmlPath));
            }
        }

        private int mIndex;
        private TestWebServer mWebServer;
    }

    // To verify whether UseWideViewport works, we check, if the page width specified
    // in the "meta viewport" tag is applied. When UseWideViewport is turned off, the
    // "viewport" tag is ignored, and the layout width is set to device width in DIP pixels.
    // We specify a very high width value to make sure that it doesn't intersect with
    // device screen widths (in DIP pixels).
    class AwSettingsUseWideViewportTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String VIEWPORT_TAG_LAYOUT_WIDTH = "3000";

        AwSettingsUseWideViewportTestHelper(
                AwTestContainerView containerView, TestAwContentsClient contentViewClient)
                throws Throwable {
            super(containerView, contentViewClient, true);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getUseWideViewPort();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setUseWideViewPort(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadDataSync(getData());
            final String bodyWidth = getTitleOnUiThread();
            if (value) {
                Assert.assertTrue(bodyWidth, VIEWPORT_TAG_LAYOUT_WIDTH.equals(bodyWidth));
            } else {
                Assert.assertFalse(bodyWidth, VIEWPORT_TAG_LAYOUT_WIDTH.equals(bodyWidth));
            }
        }

        private String getData() {
            return "<html><head>"
                    + "<meta name='viewport' content='width="
                    + VIEWPORT_TAG_LAYOUT_WIDTH
                    + "' />"
                    + "</head>"
                    + "<body onload='document.title=document.body.clientWidth'></body></html>";
        }
    }

    class AwSettingsLoadWithOverviewModeTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final float DEFAULT_PAGE_SCALE = 1.0f;

        AwSettingsLoadWithOverviewModeTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient,
                boolean withViewPortTag)
                throws Throwable {
            super(containerView, contentViewClient, true);
            mWithViewPortTag = withViewPortTag;
            mAwSettings.setUseWideViewPort(true);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getLoadWithOverviewMode();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            // On tablets, viewport width will default to device width without viewport tag; so the
            // page will not have any overflowing content to zoom out.
            mExpectScaleChange =
                    mAwSettings.getLoadWithOverviewMode() != value
                            && (!isTablet() || mWithViewPortTag);
            if (mExpectScaleChange) {
                mOnScaleChangedCallCount =
                        mContentViewClient.getOnScaleChangedHelper().getCallCount();
            }
            mAwSettings.setLoadWithOverviewMode(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadDataSync(getData());
            if (mExpectScaleChange) {
                mContentViewClient
                        .getOnScaleChangedHelper()
                        .waitForCallback(mOnScaleChangedCallCount);
                mExpectScaleChange = false;
            }
            float currentScale = mActivityTestRule.getScaleOnUiThread(mAwContents);
            // On tablets, viewport width will default to device width without viewport tag; so the
            // page will not have any overflowing content to zoom out.
            if (value && (!isTablet() || mWithViewPortTag)) {
                Assert.assertTrue(
                        "Expected: " + currentScale + " < " + DEFAULT_PAGE_SCALE,
                        currentScale < DEFAULT_PAGE_SCALE);
            } else {
                Assert.assertEquals(DEFAULT_PAGE_SCALE, currentScale, 0);
            }
        }

        private String getData() {
            // Add a sequence number as a comment to ensure WebView does not do
            // something special for the same page load, for instance, restoring
            // user state like a scroll position.
            return "<html><head>"
                    + (mWithViewPortTag ? "<meta name='viewport' content='width=3000' />" : "")
                    + "</head><body><!-- "
                    + mDataSequence++
                    + " --></body></html>";
        }

        private final boolean mWithViewPortTag;
        private boolean mExpectScaleChange;
        private int mOnScaleChangedCallCount;
        private int mDataSequence;
    }

    class AwSettingsForceZeroLayoutHeightTestHelper extends AwSettingsTestHelper<Boolean> {

        AwSettingsForceZeroLayoutHeightTestHelper(
                AwTestContainerView containerView,
                TestAwContentsClient contentViewClient,
                boolean withViewPortTag)
                throws Throwable {
            super(containerView, contentViewClient, true);
            mWithViewPortTag = withViewPortTag;
            mAwSettings.setUseWideViewPort(true);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getForceZeroLayoutHeight();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setForceZeroLayoutHeight(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadDataSync(getData());
            int height = Integer.parseInt(getTitleOnUiThread());
            if (value) {
                Assert.assertEquals(0, height);
            } else {
                Assert.assertTrue("Div should be at least 50px high, was: " + height, height >= 50);
            }
        }

        private String getData() {
            return "<html><head>"
                    + (mWithViewPortTag ? "<meta name='viewport' content='height=3000' />" : "")
                    + "  <script type='text/javascript'> "
                    + "    window.addEventListener('load', function(event) { "
                    + "       document.title = document.getElementById('testDiv').clientHeight; "
                    + "    }); "
                    + "  </script> "
                    + "</head>"
                    + "<body> "
                    + "  <div style='height:50px;'>test</div> "
                    + "  <div id='testDiv' style='height:100%;'></div> "
                    + "</body></html>";
        }

        private final boolean mWithViewPortTag;
    }

    class AwSettingsZeroLayoutHeightDisablesViewportQuirkTestHelper
            extends AwSettingsTestHelper<Boolean> {

        AwSettingsZeroLayoutHeightDisablesViewportQuirkTestHelper(
                AwTestContainerView containerView, TestAwContentsClient contentViewClient)
                throws Throwable {
            super(containerView, contentViewClient, true);
            mAwSettings.setUseWideViewPort(true);
            mAwSettings.setForceZeroLayoutHeight(true);
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getZeroLayoutHeightDisablesViewportQuirk();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setZeroLayoutHeightDisablesViewportQuirk(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            loadDataSync(getData());
            final int reportedClientWidth = Integer.parseInt(getTitleOnUiThread());
            if (value) {
                int displayWidth = calcDisplayWidthDp(mContext);
                Assert.assertEquals(displayWidth, reportedClientWidth);
            } else {
                Assert.assertEquals(3000, reportedClientWidth);
            }
        }

        private String getData() {
            return "<html><head>"
                    + "<meta name='viewport' content='width=3000, minimum-scale=1' />"
                    + "  <script type='text/javascript'> "
                    + "    window.addEventListener('load', function(event) { "
                    + "       document.title = document.documentElement.clientWidth; "
                    + "    }); "
                    + "  </script> "
                    + "</head>"
                    + "<body> "
                    + "  <div style='height:50px;'>test</div> "
                    + "  <div id='testDiv' style='height:100%;'></div> "
                    + "</body></html>";
        }
    }

    class AwSettingsWillSuppressErrorPageTestHelper extends AwSettingsTestHelper<Boolean> {
        private static final String BAD_SCHEME_URL = "htt://nonsense";
        private static final String PREV_TITLE = "cuencpobgjhfdmdovhmfdkjf";
        private static final int MAX_TIME_LOADING_ERROR_PAGE = 1000;
        private final AwContents mAwContents;

        AwSettingsWillSuppressErrorPageTestHelper(
                AwTestContainerView containerView, TestAwContentsClient contentViewClient)
                throws Throwable {
            super(containerView, contentViewClient, true);
            mAwContents = containerView.getAwContents();
        }

        @Override
        protected Boolean getAlteredValue() {
            return ENABLED;
        }

        @Override
        protected Boolean getInitialValue() {
            return DISABLED;
        }

        @Override
        protected Boolean getCurrentValue() {
            return mAwSettings.getWillSuppressErrorPage();
        }

        @Override
        protected void setCurrentValue(Boolean value) {
            mAwSettings.setWillSuppressErrorPage(value);
        }

        @Override
        protected void doEnsureSettingHasValue(Boolean value) throws Throwable {
            // Load a known state
            loadDataSync(getData());

            final WebContents webContents = mAwContents.getWebContents();
            final CallbackHelper onTitleUpdatedHelper = new CallbackHelper();
            final WebContentsObserver observer =
                    ThreadUtils.runOnUiThreadBlocking(
                            () ->
                                    new WebContentsObserver(webContents) {
                                        @Override
                                        public void titleWasSet(String title) {
                                            onTitleUpdatedHelper.notifyCalled();
                                        }
                                    });
            int callCount = onTitleUpdatedHelper.getCallCount();

            loadUrlSync(BAD_SCHEME_URL);

            // Verify the state in settings reflect what we expect
            AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
            Assert.assertEquals(value, settings.getWillSuppressErrorPage());

            // Verify the error page is shown / suppressed
            if (value == DISABLED) {
                // Showing an error page should change the page title.
                onTitleUpdatedHelper.waitForCallback(
                        "Showing an error page should change the page title, "
                                + "but no change happened",
                        callCount);
                Assert.assertNotEquals(
                        "Showing an error page should change the page title, "
                                + "but no change happened",
                        PREV_TITLE,
                        getTitleOnUiThread());
            } else {
                // Suppressing the error page should mean nothing changes (no callbacks). However,
                // verifying that the error page actually never loads isn't straight-forward,
                // as it happens asynchronously.
                // In fact, there doesn't seem to be any direct, non-flaky way of detecting this.
                Thread.sleep(MAX_TIME_LOADING_ERROR_PAGE);
                Assert.assertEquals(
                        "Suppressing an error page should leave the page title unchanged, "
                                + "but a change still happened",
                        PREV_TITLE,
                        getTitleOnUiThread());
            }

            ThreadUtils.runOnUiThreadBlocking(() -> webContents.removeObserver(observer));
        }

        private String getData() {
            return "<html><head><title>"
                    + PREV_TITLE
                    + "</title></head><body>Page Text</body></html>";
        }
    }

    public static int calcDisplayWidthDp(Context context) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DisplayAndroid displayAndroid = DisplayAndroid.getNonMultiDisplay(context);
                    return DisplayUtil.pxToDp(displayAndroid, displayAndroid.getDisplayWidth());
                });
    }

    class AwSettingsCorsTestHelper {
        public static final String ASSET_MAIN_URL = "file:///android_asset/cors.html";
        public static final String RESOURCE_IMAGE_URL = "file:///android_res/raw/resource_icon.png";

        private static final String TEST_HTML_FILE_PATH = "android_webview/test/data/cors.html";
        private static final String TEST_IMAGE_FILE_PATH = "android_webview/test/data/chrome.png";
        private static final String TEST_HTML_CONTENT_PATH = "cors.html";

        private final TestAwContentsClient mContentClient;
        private final AwTestContainerView mTestContainerView;
        private final AwContents mAwContents;
        private final AwSettings mAwSettings;

        public final String mContentMainUrl;
        public final String mFileMainUrl;
        public final String mContentImageUrl;
        public final String mFileImageUrl;

        AwSettingsCorsTestHelper() throws Throwable {
            mContentClient = new TestAwContentsClient();
            mTestContainerView =
                    mActivityTestRule.createAwTestContainerViewOnMainSync(mContentClient);
            mAwContents = mTestContainerView.getAwContents();
            mAwSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);

            TestContentProvider.register(
                    TEST_HTML_CONTENT_PATH,
                    "text/html",
                    FileUtils.readStream(
                            new FileInputStream(
                                    UrlUtils.getIsolatedTestFilePath(TEST_HTML_FILE_PATH))));
            AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
            mAwSettings.setAllowFileAccess(true);
            mAwSettings.setAllowContentAccess(true);
            mAwSettings.setAllowFileAccessFromFileURLs(false);
            mAwSettings.setAllowUniversalAccessFromFileURLs(false);

            mContentMainUrl = TestContentProvider.createContentUrl(TEST_HTML_CONTENT_PATH);
            mFileMainUrl = UrlUtils.getIsolatedTestFileUrl(TEST_HTML_FILE_PATH);
            mContentImageUrl = TestContentProvider.createContentUrl("image");
            mFileImageUrl = UrlUtils.getIsolatedTestFileUrl(TEST_IMAGE_FILE_PATH);
        }

        public String getTestResult(String api, String mainUrl, String resourceUrl)
                throws Throwable {
            final String targetUrl =
                    mainUrl + "?api=" + api + "&url=" + URLEncoder.encode(resourceUrl);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentClient.getOnPageFinishedHelper(), targetUrl);
            AwActivityTestRule.pollInstrumentationThread(
                    () -> !"running".equals(mActivityTestRule.getTitleOnUiThread(mAwContents)));
            return mActivityTestRule.getTitleOnUiThread(mAwContents);
        }

        public void allowFileAccessFromFileURLs() {
            mAwSettings.setAllowFileAccessFromFileURLs(true);
        }

        public void allowUniversalAccessFromFileURLs() {
            mAwSettings.setAllowUniversalAccessFromFileURLs(true);
        }

        public void disallowFileAccess() {
            mAwSettings.setAllowFileAccess(false);
        }

        public void disallowContentAccess() {
            mAwSettings.setAllowContentAccess(false);
        }
    }

    // The test verifies that JavaScript is disabled upon WebView
    // creation without accessing AwSettings. If the test passes,
    // it means that WebView-specific web preferences configuration
    // is applied on WebView creation. JS state is used, because it is
    // enabled by default in Chrome, but must be disabled by default
    // in WebView.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testJavaScriptDisabledByDefault() throws Throwable {
        final String jsEnabledString = "JS has run";
        final String jsDisabledString = "JS has not run";
        final String testPageHtml =
                "<html><head><title>"
                        + jsDisabledString
                        + "</title>"
                        + "</head><body onload=\"document.title='"
                        + jsEnabledString
                        + "';\"></body></html>";
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        mActivityTestRule.loadDataSync(
                awContents,
                contentClient.getOnPageFinishedHelper(),
                testPageHtml,
                "text/html",
                false);
        Assert.assertEquals(jsDisabledString, mActivityTestRule.getTitleOnUiThread(awContents));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testJavaScriptEnabledWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsJavaScriptTestHelper(views.getContainer0(), views.getClient0()),
                new AwSettingsJavaScriptTestHelper(views.getContainer1(), views.getClient1()));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testJavaScriptEnabledDynamicWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsJavaScriptDynamicTestHelper(
                        views.getContainer0(), views.getClient0()),
                new AwSettingsJavaScriptDynamicTestHelper(
                        views.getContainer1(), views.getClient1()));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testStandardFontFamilyWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsStandardFontFamilyTestHelper(
                        views.getContainer0(), views.getClient0()),
                new AwSettingsStandardFontFamilyTestHelper(
                        views.getContainer1(), views.getClient1()));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testDefaultFontSizeWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsDefaultFontSizeTestHelper(views.getContainer0(), views.getClient0()),
                new AwSettingsDefaultFontSizeTestHelper(views.getContainer1(), views.getClient1()));
    }

    // The test verifies that after changing the LoadsImagesAutomatically
    // setting value from false to true previously skipped images are
    // automatically loaded.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testLoadsImagesAutomaticallyNoPageReload() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        settings.setJavaScriptEnabled(true);
        ImagePageGenerator generator = new ImagePageGenerator(0, false);
        settings.setLoadsImagesAutomatically(false);
        mActivityTestRule.loadDataSync(
                awContents,
                contentClient.getOnPageFinishedHelper(),
                generator.getPageSource(),
                "text/html",
                false);
        Assert.assertEquals(
                ImagePageGenerator.IMAGE_NOT_LOADED_STRING,
                mActivityTestRule.getTitleOnUiThread(awContents));
        settings.setLoadsImagesAutomatically(true);
        AwActivityTestRule.pollInstrumentationThread(
                () ->
                        !ImagePageGenerator.IMAGE_NOT_LOADED_STRING.equals(
                                mActivityTestRule.getTitleOnUiThread(awContents)));
        Assert.assertEquals(
                ImagePageGenerator.IMAGE_LOADED_STRING,
                mActivityTestRule.getTitleOnUiThread(awContents));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testLoadsImagesAutomaticallyWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsLoadImagesAutomaticallyTestHelper(
                        views.getContainer0(), views.getClient0(), new ImagePageGenerator(0, true)),
                new AwSettingsLoadImagesAutomaticallyTestHelper(
                        views.getContainer1(),
                        views.getClient1(),
                        new ImagePageGenerator(1, true)));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testDefaultTextEncodingWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsDefaultTextEncodingTestHelper(
                        views.getContainer0(), views.getClient0()),
                new AwSettingsDefaultTextEncodingTestHelper(
                        views.getContainer1(), views.getClient1()));
    }

    // The test verifies that the default user agent string follows the format
    // defined in Android CTS tests:
    //
    // Mozilla/5.0 (Linux;[ U;] Android <version>;[ <language>-<country>;]
    // [<devicemodel>;] Build/<buildID>; wv) AppleWebKit/<major>.<minor> (KHTML, like Gecko)
    // Version/<major>.<minor>[ Mobile] Safari/<major>.<minor>
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUserAgentStringDefault() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        final String actualUserAgentString = settings.getUserAgentString();
        Assert.assertEquals(actualUserAgentString, AwSettings.getDefaultUserAgent());
        final String patternString =
                "Mozilla/5\\.0 \\(Linux;( U;)? Android ([^;]+);( (\\w+)-(\\w+);)?"
                        + "\\s?(.*)\\sBuild/(.+); wv\\) "
                        + "AppleWebKit/(\\d+)\\.(\\d+) \\(KHTML, like Gecko\\) "
                        + "Version/\\d+\\.\\d Chrome/\\d+\\.\\d+\\.\\d+\\.\\d+"
                        + "( Mobile)? Safari/(\\d+)\\.(\\d+)";
        final Pattern userAgentExpr = Pattern.compile(patternString);
        Matcher patternMatcher = userAgentExpr.matcher(actualUserAgentString);
        Assert.assertTrue(
                String.format(
                        "User agent string did not match expected pattern. %nExpected "
                                + "pattern:%n%s%nActual:%n%s",
                        patternString, actualUserAgentString),
                patternMatcher.find());
        // No country-language code token.
        Assert.assertEquals(null, patternMatcher.group(3));
        if ("REL".equals(Build.VERSION.CODENAME)) {
            // Model is only added in release builds
            Assert.assertEquals(Build.MODEL, patternMatcher.group(6));
            // Release version is valid only in release builds
            Assert.assertEquals(Build.VERSION.RELEASE, patternMatcher.group(2));
        }
        Assert.assertEquals(Build.ID, patternMatcher.group(7));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testWillSuppressErrorPage() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsWillSuppressErrorPageTestHelper(
                        views.getContainer0(), views.getClient0()),
                new AwSettingsWillSuppressErrorPageTestHelper(
                        views.getContainer1(), views.getClient1()));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUserAgentStringOverride() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        final String defaultUserAgentString = settings.getUserAgentString();

        // Check that an attempt to reset the default UA string has no effect.
        settings.setUserAgentString(null);
        Assert.assertEquals(defaultUserAgentString, settings.getUserAgentString());
        settings.setUserAgentString("");
        Assert.assertEquals(defaultUserAgentString, settings.getUserAgentString());

        // Check that we can also set the default value.
        settings.setUserAgentString(defaultUserAgentString);
        Assert.assertEquals(defaultUserAgentString, settings.getUserAgentString());

        // Set a custom UA string, verify that it can be reset back to default.
        final String customUserAgentString = "AwSettingsTest";
        settings.setUserAgentString(customUserAgentString);
        Assert.assertEquals(customUserAgentString, settings.getUserAgentString());
        settings.setUserAgentString(null);
        Assert.assertEquals(defaultUserAgentString, settings.getUserAgentString());

        // Try to set invalid UAs and ensure they throw an exception.
        final String[] invalids = {"null\u0000", "cr\r", "nl\n"};
        for (String ua : invalids) {
            try {
                settings.setUserAgentString(ua);
                Assert.fail("Invalid UA accepted: " + ua);
            } catch (IllegalArgumentException e) {
                // success
            }
        }
    }

    // Verify that the current UA override setting has a priority over UA
    // overrides in navigation history entries.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUserAgentStringOverrideForHistory() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        final WebContents webContents = awContents.getWebContents();
        CallbackHelper onPageFinishedHelper = contentClient.getOnPageFinishedHelper();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        settings.setJavaScriptEnabled(true);
        final String defaultUserAgentString = settings.getUserAgentString();
        final String customUserAgentString = "AwSettingsTest";
        // We are using different page titles to make sure that we are really
        // going back and forward between them.
        final String pageTemplate =
                "<html><head><title>%s</title></head>"
                        + "<body onload='document.title+=navigator.userAgent'></body>"
                        + "</html>";
        final String page1Title = "Page1";
        final String page2Title = "Page2";
        final String page1 = String.format(pageTemplate, page1Title);
        final String page2 = String.format(pageTemplate, page2Title);
        settings.setUserAgentString(customUserAgentString);
        mActivityTestRule.loadDataSync(awContents, onPageFinishedHelper, page1, "text/html", false);
        Assert.assertEquals(
                page1Title + customUserAgentString,
                mActivityTestRule.getTitleOnUiThread(awContents));
        mActivityTestRule.loadDataSync(awContents, onPageFinishedHelper, page2, "text/html", false);
        Assert.assertEquals(
                page2Title + customUserAgentString,
                mActivityTestRule.getTitleOnUiThread(awContents));
        settings.setUserAgentString(null);
        // Must not cause any changes until the next page loading.
        Assert.assertEquals(
                page2Title + customUserAgentString,
                mActivityTestRule.getTitleOnUiThread(awContents));
        HistoryUtils.goBackSync(
                InstrumentationRegistry.getInstrumentation(), webContents, onPageFinishedHelper);
        Assert.assertEquals(
                page1Title + defaultUserAgentString,
                mActivityTestRule.getTitleOnUiThread(awContents));
        HistoryUtils.goForwardSync(
                InstrumentationRegistry.getInstrumentation(), webContents, onPageFinishedHelper);
        Assert.assertEquals(
                page2Title + defaultUserAgentString,
                mActivityTestRule.getTitleOnUiThread(awContents));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUserAgentStringWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsUserAgentStringTestHelper(views.getContainer0(), views.getClient0()),
                new AwSettingsUserAgentStringTestHelper(views.getContainer1(), views.getClient1()));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUserAgentWithTestServer() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final String customUserAgentString = "testUserAgentWithTestServerUserAgent";
        AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        // Create url with echoheader echoing the User-Agent header in the the html body.
        String url = testServer.getURL("/echoheader?User-Agent");
        settings.setUserAgentString(customUserAgentString);
        mActivityTestRule.loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), url);
        String userAgent =
                mActivityTestRule.getJavaScriptResultBodyTextContent(awContents, contentClient);
        Assert.assertEquals(customUserAgentString, userAgent);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({"enable-features=UserAgentClientHint,UACHOverrideBlank"})
    public void testUserAgentOverrideClientHints() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final String customUserAgentString = "testUserAgentOverrideClientHints";
        AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        settings.setUserAgentString(customUserAgentString);

        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_OK);

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        String targetUrl =
                testServer.getURL("/android_webview/test/data/fetch-echo.html")
                        + "?url="
                        + URLEncoder.encode("/echoheader?Sec-CH-UA&Sec-CH-UA-Platform&User-Agent");
        mActivityTestRule.loadUrlSync(
                awContents, contentClient.getOnPageFinishedHelper(), targetUrl);
        AwActivityTestRule.pollInstrumentationThread(
                () -> !"running".equals(mActivityTestRule.getTitleOnUiThread(awContents)));

        String actualTitleContent = mActivityTestRule.getTitleOnUiThread(awContents);
        // Here we can't directly validate the exact value for the client hints: Sec-CH-UA and
        // Sec-CH-UA-Platform since they change over release version. Try best to validate the value
        // ends with platform string and user-agent string.
        Assert.assertTrue(
                actualTitleContent.endsWith(
                        /* sec-ch-ua-platform= */ "\"Android\" "
                                +
                                /* user-agent= */ customUserAgentString));
        // Sec-ch-ua value has brand AndroidWebview.
        Assert.assertTrue(actualTitleContent.indexOf("\"Android WebView\";v=\"") != -1);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({"enable-features=UserAgentClientHint"})
    public void testUserAgentOverrideWithDefaultUserAgentClientHints() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        final String customUserAgentString =
                settings.getUserAgentString() + "UserAgentOverrideSuffix";
        settings.setUserAgentString(customUserAgentString);

        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_OK);

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        String targetUrl =
                testServer.getURL("/android_webview/test/data/fetch-echo.html")
                        + "?url="
                        + URLEncoder.encode(
                                "/echoheader?Sec-CH-UA-Mobile&Sec-CH-UA-Platform&User-Agent");
        mActivityTestRule.loadUrlSync(
                awContents, contentClient.getOnPageFinishedHelper(), targetUrl);
        AwActivityTestRule.pollInstrumentationThread(
                () -> !"running".equals(mActivityTestRule.getTitleOnUiThread(awContents)));
        // Make sure the Sec-CH-UA-Mobile, Sec-CH-UA-Platform client hint returns the correct
        // value. If use the mobile user agent, Sec-CH-UA-Mobile should return true, otherwise
        // false.
        if (customUserAgentString.indexOf(" Mobile") != -1) {
            Assert.assertEquals(
                    "?1 \"Android\" " + customUserAgentString,
                    mActivityTestRule.getTitleOnUiThread(awContents));
        } else {
            Assert.assertEquals(
                    "?0 \"Android\" " + customUserAgentString,
                    mActivityTestRule.getTitleOnUiThread(awContents));
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testDomStorageEnabledWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsDomStorageEnabledTestHelper(
                        views.getContainer0(), views.getClient0()),
                new AwSettingsDomStorageEnabledTestHelper(
                        views.getContainer1(), views.getClient1()));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @RequiresRestart("setDatabaseEnabled is ignored after the first use of WebView in the process")
    @CommandLineFlags.Add({"enable-features=kWebSQLAccess"})
    public void testDatabaseInitialValue() throws Throwable {
        TestAwContentsClient client = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(client);
        AwSettingsDatabaseTestHelper helper =
                new AwSettingsDatabaseTestHelper(testContainerView, client);
        helper.ensureSettingHasInitialValue();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @RequiresRestart("setDatabaseEnabled is ignored after the first use of WebView in the process")
    @CommandLineFlags.Add({"enable-features=kWebSQLAccess"})
    public void testDatabaseEnabled() throws Throwable {
        TestAwContentsClient client = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(client);
        AwSettingsDatabaseTestHelper helper =
                new AwSettingsDatabaseTestHelper(testContainerView, client);
        helper.setAlteredSettingValue();
        helper.ensureSettingHasAlteredValue();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @RequiresRestart("setDatabaseEnabled is ignored after the first use of WebView in the process")
    @CommandLineFlags.Add({"enable-features=kWebSQLAccess"})
    public void testDatabaseDisabled() throws Throwable {
        TestAwContentsClient client = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(client);
        AwSettingsDatabaseTestHelper helper =
                new AwSettingsDatabaseTestHelper(testContainerView, client);
        helper.setInitialSettingValue();
        helper.ensureSettingHasInitialValue();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUniversalAccessFromFilesWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsUniversalAccessFromFilesTestHelper(
                        views.getContainer0(), views.getClient0()),
                new AwSettingsUniversalAccessFromFilesTestHelper(
                        views.getContainer1(), views.getClient1()));
    }

    // This test verifies that local image resources can be loaded from file:
    // URLs regardless of file access state.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testFileAccessFromFilesImage() throws Throwable {
        final String testFile = "android_webview/test/data/image_access.html";
        assertFileIsReadable(UrlUtils.getIsolatedTestFilePath(testFile));
        final String imageContainerUrl = UrlUtils.getIsolatedTestFileUrl(testFile);
        final String imageHeight = "16";
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        settings.setJavaScriptEnabled(true);
        settings.setAllowFileAccess(true);
        settings.setAllowUniversalAccessFromFileURLs(false);
        settings.setAllowFileAccessFromFileURLs(false);
        mActivityTestRule.loadUrlSync(
                awContents, contentClient.getOnPageFinishedHelper(), imageContainerUrl);
        Assert.assertEquals(imageHeight, mActivityTestRule.getTitleOnUiThread(awContents));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testFileAccessFromFilesIframeWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsFileAccessFromFilesIframeTestHelper(
                        views.getContainer0(), views.getClient0()),
                new AwSettingsFileAccessFromFilesIframeTestHelper(
                        views.getContainer1(), views.getClient1()));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testFileAccessFromFilesXhrWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsFileAccessFromFilesXhrTestHelper(
                        views.getContainer0(), views.getClient0()),
                new AwSettingsFileAccessFromFilesXhrTestHelper(
                        views.getContainer1(), views.getClient1()));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testFileUrlAccessWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsFileUrlAccessTestHelper(views.getContainer0(), views.getClient0(), 0),
                new AwSettingsFileUrlAccessTestHelper(
                        views.getContainer1(), views.getClient1(), 1));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testContentUrlAccessWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsContentUrlAccessTestHelper(
                        views.getContainer0(), views.getClient0(), 0),
                new AwSettingsContentUrlAccessTestHelper(
                        views.getContainer1(), views.getClient1(), 1));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "Navigation"})
    public void testBlockingContentUrlsFromDataUrls() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        final String target = "content_from_data";
        final String page =
                "<html><body>"
                        + "<img src=\""
                        + createContentUrl(target)
                        + "\">"
                        + "</body></html>";
        resetResourceRequestCountInContentProvider(target);
        mActivityTestRule.loadDataSync(
                awContents, contentClient.getOnPageFinishedHelper(), page, "text/html", false);
        ensureResourceRequestCountInContentProvider(target, 0);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "Navigation"})
    public void testContentUrlFromFileWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsContentUrlAccessFromFileTestHelper(
                        views.getContainer0(), views.getClient0(), 0),
                new AwSettingsContentUrlAccessFromFileTestHelper(
                        views.getContainer1(), views.getClient1(), 1));
    }

    // Resources under content:// and file:// should not be accessed via XHR
    // from content:// and file://.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "CORS"})
    public void testContentUrlMakesXhrRequests() throws Throwable {
        final AwSettingsCorsTestHelper corsTestHelper = new AwSettingsCorsTestHelper();

        // Case a) content:// to content:// should fail.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr", corsTestHelper.mContentMainUrl, corsTestHelper.mContentImageUrl));

        // Case b) content:// to file:// should fail.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr", corsTestHelper.mContentMainUrl, corsTestHelper.mFileImageUrl));

        // Case b') content:// to file:///android_res/ should also fail.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr",
                        corsTestHelper.mContentMainUrl,
                        AwSettingsCorsTestHelper.RESOURCE_IMAGE_URL));

        // Case c) file:// to content:// should fail.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr", corsTestHelper.mFileMainUrl, corsTestHelper.mContentImageUrl));

        // Case c') file:///android_asset/ to content:// should also fail.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        corsTestHelper.mContentImageUrl));

        // Case d) file:// to file:// should fail.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr", corsTestHelper.mFileMainUrl, corsTestHelper.mFileImageUrl));

        // Case d') file:///android_asset/ to file:// should also fail.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        corsTestHelper.mFileImageUrl));

        // Case d'') file:///android_asset/ to file:///android_res/ should also fail.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        AwSettingsCorsTestHelper.RESOURCE_IMAGE_URL));
    }

    // Check if setAllowFileAccessFromFileURLs(true) allows same-scheme CORS accesses.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "CORS"})
    public void testContentUrlMakesXhrRequestsWithAllowFileAccess() throws Throwable {
        final AwSettingsCorsTestHelper corsTestHelper = new AwSettingsCorsTestHelper();
        corsTestHelper.allowFileAccessFromFileURLs();

        // Case a) content:// to content:// should pass.
        Assert.assertEquals(
                "load",
                corsTestHelper.getTestResult(
                        "xhr", corsTestHelper.mContentMainUrl, corsTestHelper.mContentImageUrl));

        // Case b) content:// to file:// should fail.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr", corsTestHelper.mContentMainUrl, corsTestHelper.mFileImageUrl));

        // Case b') content:// to file:///android_res/ should also fail.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr",
                        corsTestHelper.mContentMainUrl,
                        AwSettingsCorsTestHelper.RESOURCE_IMAGE_URL));

        // Case c) file:// to content:// should fail.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr", corsTestHelper.mFileMainUrl, corsTestHelper.mContentImageUrl));

        // Case c') file:///android_asset/ to content:// should also fail.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        corsTestHelper.mContentImageUrl));

        // Case d) file:// to file:// should pass.
        Assert.assertEquals(
                "load",
                corsTestHelper.getTestResult(
                        "xhr", corsTestHelper.mFileMainUrl, corsTestHelper.mFileImageUrl));

        // Case d') file:///android_asset/ to file:// should also pass.
        Assert.assertEquals(
                "load",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        corsTestHelper.mFileImageUrl));

        // Case d'') file:///android_asset/ to file:///android_res/ should also pass.
        Assert.assertEquals(
                "load",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        AwSettingsCorsTestHelper.RESOURCE_IMAGE_URL));
    }

    // Check if setAllowUniversalAccessFromFileURLs(true) allows any CORS access from file://.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "CORS"})
    public void testContentUrlMakesXhrRequestsWithAllowUniversalAccess() throws Throwable {
        final AwSettingsCorsTestHelper corsTestHelper = new AwSettingsCorsTestHelper();
        corsTestHelper.allowUniversalAccessFromFileURLs();

        // Case a) content:// to content:// should pass.
        Assert.assertEquals(
                "load",
                corsTestHelper.getTestResult(
                        "xhr", corsTestHelper.mContentMainUrl, corsTestHelper.mContentImageUrl));

        // Case b) content:// to file:// should fail.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr", corsTestHelper.mContentMainUrl, corsTestHelper.mFileImageUrl));

        // Case b') content:// to file:///android_res/ should also fail.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr",
                        corsTestHelper.mContentMainUrl,
                        AwSettingsCorsTestHelper.RESOURCE_IMAGE_URL));

        // Case c) file:// to content:// should pass.
        Assert.assertEquals(
                "load",
                corsTestHelper.getTestResult(
                        "xhr", corsTestHelper.mFileMainUrl, corsTestHelper.mContentImageUrl));

        // Case c') file:///android_asset/ to content:// should also pass.
        Assert.assertEquals(
                "load",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        corsTestHelper.mContentImageUrl));

        // Case d) file:// to file:// should pass.
        Assert.assertEquals(
                "load",
                corsTestHelper.getTestResult(
                        "xhr", corsTestHelper.mFileMainUrl, corsTestHelper.mFileImageUrl));

        // Case d') file:///android_asset/ to file:// should also pass.
        Assert.assertEquals(
                "load",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        corsTestHelper.mFileImageUrl));

        // Case d'') file:///android_asset/ to file:///android_res/ should also pass.
        Assert.assertEquals(
                "load",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        AwSettingsCorsTestHelper.RESOURCE_IMAGE_URL));
    }

    // Check if the Fetch API always fails on file:// and content://.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "CORS"})
    public void testContentUrlMakesFetchRequestsWithoutFileAccess() throws Throwable {
        final AwSettingsCorsTestHelper corsTestHelper = new AwSettingsCorsTestHelper();
        // Run tests with the most relaxed settings.
        corsTestHelper.allowFileAccessFromFileURLs();
        corsTestHelper.allowUniversalAccessFromFileURLs();

        // Case a) content:// to content:// should fail.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "fetch", corsTestHelper.mContentMainUrl, corsTestHelper.mContentImageUrl));

        // Case b) content:// to file:// should fail.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "fetch", corsTestHelper.mContentMainUrl, corsTestHelper.mFileImageUrl));

        // Case c) file:// to content:// should fail.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "fetch", corsTestHelper.mFileMainUrl, corsTestHelper.mContentImageUrl));

        // Case d) file:// to file:// should fail.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "fetch", corsTestHelper.mFileMainUrl, corsTestHelper.mFileImageUrl));
    }

    // Check if file:// and content:// can be accessible from
    // file:///android_asset/ when the file and content access is disallowed.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "CORS"})
    public void testAndroidUrlMakesXhrRequestsWithoutFileAndContentAccesses() throws Throwable {
        final AwSettingsCorsTestHelper corsTestHelper = new AwSettingsCorsTestHelper();
        // file:///android_asset/ and file:///android_res can be accessible even
        // if AllowFileAccess and AllowContentAccess are set to false.
        corsTestHelper.disallowFileAccess();
        corsTestHelper.disallowContentAccess();
        corsTestHelper.allowFileAccessFromFileURLs();

        // Case c') file:///android_asset/ to content:// should fail as
        // content:// is still disallowed by AllowContentAccess.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        corsTestHelper.mContentImageUrl));

        // Case d') file:///android_asset/ to file:// should fail as file://
        // is still disallowed by AllowFileAccess.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        corsTestHelper.mFileImageUrl));

        // Case d'') file:///android_asset/ to file:///android_res/ should pass.
        Assert.assertEquals(
                "load",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        AwSettingsCorsTestHelper.RESOURCE_IMAGE_URL));

        // AllowUniversalAccessFromFileURLs should not help.
        corsTestHelper.allowUniversalAccessFromFileURLs();

        // Case c') file:///android_asset/ to content:// should fail as
        // content:// is still disallowed by AllowContentAccess.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        corsTestHelper.mContentImageUrl));

        // Case d') file:///android_asset/ to file:// should fail as file://
        // is still disallowed by AllowFileAccess.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        corsTestHelper.mFileImageUrl));
    }

    // Check if file:// and content:// can be accessible from
    // file:///android_asset/ when the file access is disallowed.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences", "CORS"})
    public void testAndroidUrlMakesXhrRequests() throws Throwable {
        final AwSettingsCorsTestHelper corsTestHelper = new AwSettingsCorsTestHelper();
        // file:///android_asset/ and file:///android_res can be accessible even
        // if AllowFileAccess is set to false.
        corsTestHelper.disallowFileAccess();
        corsTestHelper.allowFileAccessFromFileURLs();

        // Case c') file:///android_asset/ to content:// should fail as
        // content:// is still accessible but CORS is not permitted.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        corsTestHelper.mContentImageUrl));

        // Case d') file:///android_asset/ to file:// should fail as file://
        // is still disallowed by AllowFileAccess.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        corsTestHelper.mFileImageUrl));

        // Case d'') file:///android_asset/ to file:///android_res/ should pass.
        Assert.assertEquals(
                "load",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        AwSettingsCorsTestHelper.RESOURCE_IMAGE_URL));

        // AllowUniversalAccessFromFileURLs should not help.
        corsTestHelper.allowUniversalAccessFromFileURLs();

        // Case c') file:///android_asset/ to content:// pass as CORS accesses
        // are permitted now.
        Assert.assertEquals(
                "load",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        corsTestHelper.mContentImageUrl));

        // Case d') file:///android_asset/ to file:// should fail as file://
        // is still disallowed by AllowFileAccess.
        Assert.assertEquals(
                "error",
                corsTestHelper.getTestResult(
                        "xhr",
                        AwSettingsCorsTestHelper.ASSET_MAIN_URL,
                        corsTestHelper.mFileImageUrl));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testBlockNetworkImagesDoesNotBlockDataUrlImage() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        final AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        ImagePageGenerator generator = new ImagePageGenerator(0, false);

        settings.setJavaScriptEnabled(true);
        settings.setImagesEnabled(false);
        mActivityTestRule.loadDataSync(
                awContents,
                contentClient.getOnPageFinishedHelper(),
                generator.getPageSource(),
                "text/html",
                false);
        Assert.assertEquals(
                ImagePageGenerator.IMAGE_LOADED_STRING,
                mActivityTestRule.getTitleOnUiThread(awContents));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testBlockNetworkImagesBlocksNetworkImageAndReloadInPlace() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        final AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        settings.setJavaScriptEnabled(true);
        ImagePageGenerator generator = new ImagePageGenerator(0, false);

        TestWebServer webServer = TestWebServer.start();
        try {
            final String httpImageUrl = generator.getPageUrl(webServer);

            settings.setImagesEnabled(false);
            mActivityTestRule.loadUrlSync(
                    awContents, contentClient.getOnPageFinishedHelper(), httpImageUrl);
            Assert.assertEquals(
                    ImagePageGenerator.IMAGE_NOT_LOADED_STRING,
                    mActivityTestRule.getTitleOnUiThread(awContents));

            settings.setImagesEnabled(true);
            AwActivityTestRule.pollInstrumentationThread(
                    () ->
                            ImagePageGenerator.IMAGE_LOADED_STRING.equals(
                                    mActivityTestRule.getTitleOnUiThread(awContents)));
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testBlockNetworkImagesWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        TestWebServer webServer = TestWebServer.start();
        try {
            runPerViewSettingsTest(
                    new AwSettingsImagesEnabledHelper(
                            views.getContainer0(),
                            views.getClient0(),
                            webServer,
                            new ImagePageGenerator(0, true)),
                    new AwSettingsImagesEnabledHelper(
                            views.getContainer1(),
                            views.getClient1(),
                            webServer,
                            new ImagePageGenerator(1, true)));
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testBlockNetworkLoadsWithHttpResources() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainer =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainer.getAwContents();
        final AwSettings awSettings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        awSettings.setJavaScriptEnabled(true);
        awSettings.setAllowFileAccess(true);
        ImagePageGenerator generator = new ImagePageGenerator(0, false);

        String fileName = null;
        TestWebServer webServer = TestWebServer.start();
        try {
            // Set up http image.
            final String httpPath = "/image.png";
            final String imageUrl =
                    webServer.setResponseBase64(
                            httpPath,
                            generator.getImageSourceNoAdvance(),
                            CommonResources.getImagePngHeaders(true));

            // Set up file html that loads http iframe.
            String pageHtml =
                    "<img src='"
                            + imageUrl
                            + "' "
                            + "onload=\"document.title='img_onload_fired';\" "
                            + "onerror=\"document.title='img_onerror_fired';\" />";
            Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
            fileName = context.getCacheDir() + "/block_network_loads_test.html";
            TestFileUtil.deleteFile(fileName); // Remove leftover file if any.
            TestFileUtil.createNewHtmlFile(fileName, "unset", pageHtml);

            // Actual test. Blocking should trigger onerror handler.
            awSettings.setBlockNetworkLoads(true);
            mActivityTestRule.loadUrlSync(
                    awContents, contentClient.getOnPageFinishedHelper(), "file:///" + fileName);
            Assert.assertEquals(0, webServer.getRequestCount(httpPath));
            Assert.assertEquals(
                    "img_onerror_fired", mActivityTestRule.getTitleOnUiThread(awContents));

            // Unblock should load normally.
            awSettings.setBlockNetworkLoads(false);
            mActivityTestRule.loadUrlSync(
                    awContents, contentClient.getOnPageFinishedHelper(), "file:///" + fileName);
            Assert.assertEquals(1, webServer.getRequestCount(httpPath));
            Assert.assertEquals(
                    "img_onload_fired", mActivityTestRule.getTitleOnUiThread(awContents));
        } finally {
            webServer.shutdown();
            if (fileName != null) TestFileUtil.deleteFile(fileName);
        }
    }

    private static class AudioEvent {
        private CallbackHelper mCallback;

        public AudioEvent(CallbackHelper callback) {
            mCallback = callback;
        }

        @JavascriptInterface
        public void onCanPlay() {
            mCallback.notifyCalled();
        }

        @JavascriptInterface
        public void onError() {
            mCallback.notifyCalled();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testBlockNetworkLoadsWithAudio() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainer =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainer.getAwContents();
        final AwSettings awSettings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        final CallbackHelper callback = new CallbackHelper();
        awSettings.setJavaScriptEnabled(true);

        TestWebServer webServer = TestWebServer.start();
        try {
            final String httpPath = "/audio.mp3";
            // Don't care about the response is correct or not, just want
            // to know whether Url is accessed.
            final String audioUrl = webServer.setResponse(httpPath, "1", null);

            String pageHtml =
                    "<html><body><audio id=\"audio\" controls src='"
                            + audioUrl
                            + "' "
                            + "oncanplay=\"AudioEvent.onCanPlay();\" "
                            + "onerror=\"AudioEvent.onError();\"></audio>"
                            + "<button id=\"play\""
                            + "onclick=\"document.getElementById('audio').play();\"></button>"
                            + "</body></html>";
            // Actual test. Blocking should trigger onerror handler.
            awSettings.setBlockNetworkLoads(true);
            AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                    awContents, new AudioEvent(callback), "AudioEvent");
            int count = callback.getCallCount();
            mActivityTestRule.loadDataSync(
                    awContents,
                    contentClient.getOnPageFinishedHelper(),
                    pageHtml,
                    "text/html",
                    false);
            JSUtils.clickNodeWithUserGesture(testContainer.getWebContents(), "play");
            callback.waitForCallback(count, 1);
            Assert.assertEquals(0, webServer.getRequestCount(httpPath));

            // The below test failed in Nexus Galaxy.
            // See https://code.google.com/p/chromium/issues/detail?id=313463
            // Unblock should load normally.
            /*
            awSettings.setBlockNetworkLoads(false);
            count = callback.getCallCount();
            loadDataSync(awContents, contentClient.getOnPageFinishedHelper(), pageHtml,
                    "text/html", false);
            callback.waitForCallback(count, 1);
            assertTrue(0 != webServer.getRequestCount(httpPath));
            */
        } finally {
            webServer.shutdown();
        }
    }

    // Test an Android asset URL (file:///android_asset/)
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testAssetUrl() throws Throwable {
        // Note: this text needs to be kept in sync with the contents of the html file referenced
        // below.
        final String expectedTitle = "Asset File";
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        mActivityTestRule.loadUrlSync(
                awContents,
                contentClient.getOnPageFinishedHelper(),
                "file:///android_asset/asset_file.html");
        Assert.assertEquals(expectedTitle, mActivityTestRule.getTitleOnUiThread(awContents));
    }

    // Test an Android resource URL (file:///android_res/).
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testResourceUrl() throws Throwable {
        // Note: this text needs to be kept in sync with the contents of the html file referenced
        // below.
        final String expectedTitle = "Resource File";
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        mActivityTestRule.loadUrlSync(
                awContents,
                contentClient.getOnPageFinishedHelper(),
                "file:///android_res/raw/resource_file.html");
        Assert.assertEquals(expectedTitle, mActivityTestRule.getTitleOnUiThread(awContents));
    }

    // Test that the file URL access toggle does not affect asset URLs.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testFileUrlAccessToggleDoesNotBlockAssetUrls() throws Throwable {
        // Note: this text needs to be kept in sync with the contents of the html file referenced
        // below.
        final String expectedTitle = "Asset File";
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        final AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        settings.setAllowFileAccess(false);
        mActivityTestRule.loadUrlSync(
                awContents,
                contentClient.getOnPageFinishedHelper(),
                "file:///android_asset/asset_file.html");
        Assert.assertEquals(expectedTitle, mActivityTestRule.getTitleOnUiThread(awContents));
    }

    // Test that the file URL access toggle does not affect resource URLs.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testFileUrlAccessToggleDoesNotBlockResourceUrls() throws Throwable {
        // Note: this text needs to be kept in sync with the contents of the html file referenced
        // below.
        final String expectedTitle = "Resource File";
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        final AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        settings.setAllowFileAccess(false);
        mActivityTestRule.loadUrlSync(
                awContents,
                contentClient.getOnPageFinishedHelper(),
                "file:///android_res/raw/resource_file.html");
        Assert.assertEquals(expectedTitle, mActivityTestRule.getTitleOnUiThread(awContents));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testLayoutAlgorithmWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsLayoutAlgorithmTestHelper(views.getContainer0(), views.getClient0()),
                new AwSettingsLayoutAlgorithmTestHelper(views.getContainer1(), views.getClient1()));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testTextZoomWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsTextZoomTestHelper(views.getContainer0(), views.getClient0()),
                new AwSettingsTextZoomTestHelper(views.getContainer1(), views.getClient1()));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testTextZoomAutosizingWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsTextZoomAutosizingTestHelper(
                        views.getContainer0(), views.getClient0()),
                new AwSettingsTextZoomAutosizingTestHelper(
                        views.getContainer1(), views.getClient1()));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testJavaScriptPopupsWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsJavaScriptPopupsTestHelper(
                        views.getContainer0(), views.getClient0(), false),
                new AwSettingsJavaScriptPopupsTestHelper(
                        views.getContainer1(), views.getClient1(), false));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testJavaScriptPopupsAndMultiWindowsWithTwoViews() throws Throwable {
        final ViewPair views = createViews();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            AwSettings awSettings = views.getContents0().getSettings();
                            awSettings.setSupportMultipleWindows(true);
                            awSettings = views.getContents1().getSettings();
                            awSettings.setSupportMultipleWindows(true);
                        });
        views.getClient0().getOnCreateWindowHelper().setReturnValue(true);
        views.getClient1().getOnCreateWindowHelper().setReturnValue(true);
        runPerViewSettingsTest(
                new AwSettingsJavaScriptPopupsTestHelper(
                        views.getContainer0(), views.getClient0(), false),
                new AwSettingsJavaScriptPopupsTestHelper(
                        views.getContainer1(), views.getClient1(), false));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testJavaScriptPopupsOpenTwice() throws Throwable {
        final ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsJavaScriptPopupsTestHelper(
                        views.getContainer0(), views.getClient0(), true),
                new AwSettingsJavaScriptPopupsTestHelper(
                        views.getContainer1(), views.getClient1(), true));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testCacheMode() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainer =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainer.getAwContents();
        final AwSettings awSettings =
                mActivityTestRule.getAwSettingsOnUiThread(testContainer.getAwContents());
        mActivityTestRule.clearCacheOnUiThread(awContents, true);

        Assert.assertEquals(WebSettings.LOAD_DEFAULT, awSettings.getCacheMode());
        TestWebServer webServer = TestWebServer.start();
        try {
            final String htmlPath = "/testCacheMode.html";
            final String url = webServer.setResponse(htmlPath, "response", null);
            awSettings.setCacheMode(WebSettings.LOAD_CACHE_ELSE_NETWORK);
            mActivityTestRule.loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), url);
            Assert.assertEquals(1, webServer.getRequestCount(htmlPath));
            mActivityTestRule.loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), url);
            Assert.assertEquals(1, webServer.getRequestCount(htmlPath));

            awSettings.setCacheMode(WebSettings.LOAD_NO_CACHE);
            mActivityTestRule.loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), url);
            Assert.assertEquals(2, webServer.getRequestCount(htmlPath));
            mActivityTestRule.loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), url);
            Assert.assertEquals(3, webServer.getRequestCount(htmlPath));

            awSettings.setCacheMode(WebSettings.LOAD_CACHE_ONLY);
            mActivityTestRule.loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), url);
            Assert.assertEquals(3, webServer.getRequestCount(htmlPath));
            mActivityTestRule.loadUrlSync(awContents, contentClient.getOnPageFinishedHelper(), url);
            Assert.assertEquals(3, webServer.getRequestCount(htmlPath));

            final String htmlNotInCachePath = "/testCacheMode-not-in-cache.html";
            final String urlNotInCache = webServer.setResponse(htmlNotInCachePath, "", null);
            mActivityTestRule.loadUrlSyncAndExpectError(
                    awContents,
                    contentClient.getOnPageFinishedHelper(),
                    contentClient.getOnReceivedErrorHelper(),
                    urlNotInCache);
            Assert.assertEquals(0, webServer.getRequestCount(htmlNotInCachePath));
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    // As our implementation of network loads blocking uses the same net::URLRequest settings, make
    // sure that setting cache mode doesn't accidentally enable network loads.  The reference
    // behaviour is that when network loads are blocked, setting cache mode has no effect.
    public void testCacheModeWithBlockedNetworkLoads() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainer =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainer.getAwContents();
        final AwSettings awSettings =
                mActivityTestRule.getAwSettingsOnUiThread(testContainer.getAwContents());
        mActivityTestRule.clearCacheOnUiThread(awContents, true);

        Assert.assertEquals(WebSettings.LOAD_DEFAULT, awSettings.getCacheMode());
        awSettings.setBlockNetworkLoads(true);
        TestWebServer webServer = TestWebServer.start();
        try {
            final String htmlPath = "/testCacheModeWithBlockedNetworkLoads.html";
            final String url = webServer.setResponse(htmlPath, "response", null);
            mActivityTestRule.loadUrlSyncAndExpectError(
                    awContents,
                    contentClient.getOnPageFinishedHelper(),
                    contentClient.getOnReceivedErrorHelper(),
                    url);
            Assert.assertEquals(0, webServer.getRequestCount(htmlPath));

            awSettings.setCacheMode(WebSettings.LOAD_CACHE_ELSE_NETWORK);
            mActivityTestRule.loadUrlSyncAndExpectError(
                    awContents,
                    contentClient.getOnPageFinishedHelper(),
                    contentClient.getOnReceivedErrorHelper(),
                    url);
            Assert.assertEquals(0, webServer.getRequestCount(htmlPath));

            awSettings.setCacheMode(WebSettings.LOAD_NO_CACHE);
            mActivityTestRule.loadUrlSyncAndExpectError(
                    awContents,
                    contentClient.getOnPageFinishedHelper(),
                    contentClient.getOnReceivedErrorHelper(),
                    url);
            Assert.assertEquals(0, webServer.getRequestCount(htmlPath));

            awSettings.setCacheMode(WebSettings.LOAD_CACHE_ONLY);
            mActivityTestRule.loadUrlSyncAndExpectError(
                    awContents,
                    contentClient.getOnPageFinishedHelper(),
                    contentClient.getOnReceivedErrorHelper(),
                    url);
            Assert.assertEquals(0, webServer.getRequestCount(htmlPath));
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testCacheModeWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        TestWebServer webServer = TestWebServer.start();
        try {
            runPerViewSettingsTest(
                    new AwSettingsCacheModeTestHelper(
                            views.getContainer0(), views.getClient0(), 0, webServer),
                    new AwSettingsCacheModeTestHelper(
                            views.getContainer1(), views.getClient1(), 1, webServer));
        } finally {
            webServer.shutdown();
        }
    }

    static class ManifestTestHelper {
        private final TestWebServer mWebServer;
        private final String mHtmlPath;
        private final String mHtmlUrl;
        private final String mManifestPath;

        ManifestTestHelper(TestWebServer webServer, String htmlPageName, String manifestName) {
            mWebServer = webServer;
            mHtmlPath = "/" + htmlPageName;
            mHtmlUrl =
                    webServer.setResponse(
                            mHtmlPath, "<html manifest=\"" + manifestName + "\"></html>", null);
            mManifestPath = "/" + manifestName;
            webServer.setResponse(
                    mManifestPath,
                    "CACHE MANIFEST",
                    CommonResources.getContentTypeAndCacheHeaders("text/cache-manifest", false));
        }

        String getHtmlPath() {
            return mHtmlPath;
        }

        String getHtmlUrl() {
            return mHtmlUrl;
        }

        String getManifestPath() {
            return mManifestPath;
        }

        int waitUntilHtmlIsRequested(final int initialRequestCount) {
            return waitUntilResourceIsRequested(mHtmlPath, initialRequestCount);
        }

        int waitUntilManifestIsRequested(final int initialRequestCount) {
            return waitUntilResourceIsRequested(mManifestPath, initialRequestCount);
        }

        private int waitUntilResourceIsRequested(final String path, final int initialRequestCount) {
            AwActivityTestRule.pollInstrumentationThread(
                    () -> mWebServer.getRequestCount(path) > initialRequestCount);
            return mWebServer.getRequestCount(path);
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUseWideViewportWithTwoViews() throws Throwable {
        ViewPair views = createViews(true);
        runPerViewSettingsTest(
                new AwSettingsUseWideViewportTestHelper(views.getContainer0(), views.getClient0()),
                new AwSettingsUseWideViewportTestHelper(views.getContainer1(), views.getClient1()));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUseWideViewportWithTwoViewsNoQuirks() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsUseWideViewportTestHelper(views.getContainer0(), views.getClient0()),
                new AwSettingsUseWideViewportTestHelper(views.getContainer1(), views.getClient1()));
    }

    private void useWideViewportLayoutWidthTest(
            AwTestContainerView testContainer, CallbackHelper onPageFinishedHelper)
            throws Throwable {
        final AwContents awContents = testContainer.getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);

        final String pageTemplate =
                "<html><head>%s</head>"
                        + "<body onload='document.title=document.body.clientWidth'></body></html>";
        final String pageNoViewport = String.format(pageTemplate, "");
        final String pageViewportDeviceWidth =
                String.format(
                        pageTemplate, "<meta name='viewport' content='width=device-width' />");
        final String viewportTagSpecifiedWidth = "3000";
        final String pageViewportSpecifiedWidth =
                String.format(
                        pageTemplate,
                        "<meta name='viewport' content='width="
                                + viewportTagSpecifiedWidth
                                + "' />");

        int displayWidth = calcDisplayWidthDp(testContainer.getContext());

        settings.setJavaScriptEnabled(true);
        Assert.assertFalse(settings.getUseWideViewPort());
        // When UseWideViewPort is off, "width" setting of "meta viewport"
        // tags is ignored, and the layout width is set to device width in CSS pixels.
        // Thus, all 3 pages will have the same body width.
        mActivityTestRule.loadDataSync(
                awContents, onPageFinishedHelper, pageNoViewport, "text/html", false);
        int actualWidth = Integer.parseInt(mActivityTestRule.getTitleOnUiThread(awContents));
        // Avoid rounding errors.
        Assert.assertTrue(
                "Expected: " + displayWidth + ", Actual: " + actualWidth,
                Math.abs(displayWidth - actualWidth) <= 1);
        mActivityTestRule.loadDataSync(
                awContents, onPageFinishedHelper, pageViewportDeviceWidth, "text/html", false);
        actualWidth = Integer.parseInt(mActivityTestRule.getTitleOnUiThread(awContents));
        Assert.assertTrue(
                "Expected: " + displayWidth + ", Actual: " + actualWidth,
                Math.abs(displayWidth - actualWidth) <= 1);
        mActivityTestRule.loadDataSync(
                awContents, onPageFinishedHelper, pageViewportSpecifiedWidth, "text/html", false);
        actualWidth = Integer.parseInt(mActivityTestRule.getTitleOnUiThread(awContents));
        Assert.assertTrue(
                "Expected: " + displayWidth + ", Actual: " + actualWidth,
                Math.abs(displayWidth - actualWidth) <= 1);

        settings.setUseWideViewPort(true);
        // When UseWideViewPort is on, "meta viewport" tag is used.
        // If there is no viewport tag, or width isn't specified,
        // then layout width is set to max(980, <device-width-in-DIP-pixels>)
        mActivityTestRule.loadDataSync(
                awContents, onPageFinishedHelper, pageNoViewport, "text/html", false);
        actualWidth = Integer.parseInt(mActivityTestRule.getTitleOnUiThread(awContents));
        if (isTablet()) {
            // On tablets, viewport width will default to device width without viewport tag.
            Assert.assertTrue(
                    "Expected: " + displayWidth + ", Actual: " + actualWidth,
                    Math.abs(displayWidth - actualWidth) <= 1);
        } else {
            Assert.assertTrue("Expected: >= 980 , Actual: " + actualWidth, actualWidth >= 980);
        }
        mActivityTestRule.loadDataSync(
                awContents, onPageFinishedHelper, pageViewportDeviceWidth, "text/html", false);
        actualWidth = Integer.parseInt(mActivityTestRule.getTitleOnUiThread(awContents));
        Assert.assertTrue(
                "Expected: " + displayWidth + ", Actual: " + actualWidth,
                Math.abs(displayWidth - actualWidth) <= 1);
        mActivityTestRule.loadDataSync(
                awContents, onPageFinishedHelper, pageViewportSpecifiedWidth, "text/html", false);
        Assert.assertEquals(
                viewportTagSpecifiedWidth, mActivityTestRule.getTitleOnUiThread(awContents));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUseWideViewportLayoutWidth() throws Throwable {
        TestAwContentsClient contentClient = new TestAwContentsClient();
        AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient, true);
        useWideViewportLayoutWidthTest(testContainerView, contentClient.getOnPageFinishedHelper());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUseWideViewportLayoutWidthNoQuirks() throws Throwable {
        TestAwContentsClient contentClient = new TestAwContentsClient();
        AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        useWideViewportLayoutWidthTest(testContainerView, contentClient.getOnPageFinishedHelper());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUseWideViewportControlsDoubleTabToZoom() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        CallbackHelper onPageFinishedHelper = contentClient.getOnPageFinishedHelper();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        settings.setBuiltInZoomControls(true);

        int displayWidth = calcDisplayWidthDp(testContainerView.getContext());
        int layoutWidth = displayWidth * 2;
        final String page =
                "<html>"
                        + "<head><meta name='viewport' content='width="
                        + layoutWidth
                        + "'>"
                        + "<style> body { width: "
                        + layoutWidth
                        + "px; }</style></head>"
                        + "<body>Page Text</body></html>";

        Assert.assertFalse(settings.getUseWideViewPort());
        // Without wide viewport the <meta viewport> tag will be ignored by WebView,
        // but it doesn't really matter as we don't expect double tap to change the scale.
        mActivityTestRule.loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        final float initialScale = mActivityTestRule.getScaleOnUiThread(awContents);
        simulateDoubleTapCenterOfWebViewOnUiThread(testContainerView);
        Thread.sleep(1000);
        Assert.assertEquals(initialScale, mActivityTestRule.getScaleOnUiThread(awContents), 0);

        settings.setUseWideViewPort(true);
        mActivityTestRule.loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        int onScaleChangedCallCount = contentClient.getOnScaleChangedHelper().getCallCount();
        simulateDoubleTapCenterOfWebViewOnUiThread(testContainerView);
        contentClient.getOnScaleChangedHelper().waitForCallback(onScaleChangedCallCount);
        final float zoomedOutScale = mActivityTestRule.getScaleOnUiThread(awContents);
        Assert.assertTrue(
                "zoomedOut: " + zoomedOutScale + ", initial: " + initialScale,
                zoomedOutScale < initialScale);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testForceZeroLayoutHeightWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsForceZeroLayoutHeightTestHelper(
                        views.getContainer0(), views.getClient0(), false),
                new AwSettingsForceZeroLayoutHeightTestHelper(
                        views.getContainer1(), views.getClient1(), false));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testForceZeroLayoutHeightViewportTagWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsForceZeroLayoutHeightTestHelper(
                        views.getContainer0(), views.getClient0(), true),
                new AwSettingsForceZeroLayoutHeightTestHelper(
                        views.getContainer1(), views.getClient1(), true));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @DisabledTest(message = "crbug.com/746264")
    public void testZeroLayoutHeightDisablesViewportQuirkWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsZeroLayoutHeightDisablesViewportQuirkTestHelper(
                        views.getContainer0(), views.getClient0()),
                new AwSettingsZeroLayoutHeightDisablesViewportQuirkTestHelper(
                        views.getContainer1(), views.getClient1()));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testLoadWithOverviewModeWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsLoadWithOverviewModeTestHelper(
                        views.getContainer0(), views.getClient0(), false),
                new AwSettingsLoadWithOverviewModeTestHelper(
                        views.getContainer1(), views.getClient1(), false));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testLoadWithOverviewModeViewportTagWithTwoViews() throws Throwable {
        ViewPair views = createViews();
        runPerViewSettingsTest(
                new AwSettingsLoadWithOverviewModeTestHelper(
                        views.getContainer0(), views.getClient0(), true),
                new AwSettingsLoadWithOverviewModeTestHelper(
                        views.getContainer1(), views.getClient1(), true));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testSetInitialScale() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        final AwSettings awSettings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        CallbackHelper onPageFinishedHelper = contentClient.getOnPageFinishedHelper();

        WindowManager wm =
                (WindowManager)
                        InstrumentationRegistry.getInstrumentation()
                                .getTargetContext()
                                .getSystemService(Context.WINDOW_SERVICE);
        Point screenSize = new Point();
        wm.getDefaultDisplay().getSize(screenSize);
        // Make sure after 50% scale, page width still larger than screen.
        int height = screenSize.y * 2 + 1;
        int width = screenSize.x * 2 + 1;
        final String page =
                "<html><body>"
                        + "<p style='height:"
                        + height
                        + "px;width:"
                        + width
                        + "px'>"
                        + "testSetInitialScale</p></body></html>";
        final float defaultScale =
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getResources()
                        .getDisplayMetrics()
                        .density;

        Assert.assertEquals(
                defaultScale, mActivityTestRule.getPixelScaleOnUiThread(awContents), .01f);
        mActivityTestRule.loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        Assert.assertEquals(
                defaultScale, mActivityTestRule.getPixelScaleOnUiThread(awContents), .01f);

        int onScaleChangedCallCount = contentClient.getOnScaleChangedHelper().getCallCount();
        awSettings.setInitialPageScale(50);
        mActivityTestRule.loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        contentClient.getOnScaleChangedHelper().waitForCallback(onScaleChangedCallCount);
        Assert.assertEquals(0.5f, mActivityTestRule.getPixelScaleOnUiThread(awContents), .01f);

        onScaleChangedCallCount = contentClient.getOnScaleChangedHelper().getCallCount();
        awSettings.setInitialPageScale(500);
        mActivityTestRule.loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        contentClient.getOnScaleChangedHelper().waitForCallback(onScaleChangedCallCount);
        Assert.assertEquals(5.0f, mActivityTestRule.getPixelScaleOnUiThread(awContents), .01f);

        onScaleChangedCallCount = contentClient.getOnScaleChangedHelper().getCallCount();
        awSettings.setInitialPageScale(0);
        mActivityTestRule.loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        contentClient.getOnScaleChangedHelper().waitForCallback(onScaleChangedCallCount);
        Assert.assertEquals(
                defaultScale, mActivityTestRule.getPixelScaleOnUiThread(awContents), .01f);
    }

    @Test
    @DisableHardwareAcceleration
    @LargeTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testMediaPlaybackWithoutUserGesture() throws Throwable {
        Assert.assertTrue(
                VideoTestUtil.runVideoTest(
                        InstrumentationRegistry.getInstrumentation(),
                        mActivityTestRule,
                        false,
                        WAIT_TIMEOUT_MS));
    }

    @Test
    @DisableHardwareAcceleration
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testMediaPlaybackWithUserGesture() throws Throwable {
        // Wait for 5 second to see if video played.
        Assert.assertFalse(
                VideoTestUtil.runVideoTest(
                        InstrumentationRegistry.getInstrumentation(),
                        mActivityTestRule,
                        true,
                        5000L));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testDefaultVideoPosterURL() throws Throwable {
        final CallbackHelper videoPosterAccessedCallbackHelper = new CallbackHelper();
        final String defaultVideoPosterUrl = "http://default_video_poster/";
        TestAwContentsClient client =
                new TestAwContentsClient() {
                    @Override
                    public WebResourceResponseInfo shouldInterceptRequest(
                            AwWebResourceRequest request) {
                        if (request.url.equals(defaultVideoPosterUrl)) {
                            videoPosterAccessedCallbackHelper.notifyCalled();
                        }
                        return null;
                    }
                };
        final AwContents awContents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(client).getAwContents();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            AwSettings awSettings = awContents.getSettings();
                            awSettings.setDefaultVideoPosterURL(defaultVideoPosterUrl);
                        });
        VideoTestWebServer webServer = new VideoTestWebServer();
        try {
            String data =
                    "<html><head><body>"
                            + "<video id='video' control src='"
                            + webServer.getOnePixelOneFrameWebmURL()
                            + "' /> </body></html>";
            mActivityTestRule.loadDataAsync(awContents, data, "text/html", false);
            videoPosterAccessedCallbackHelper.waitForCallback(0, 1, 20, TimeUnit.SECONDS);
        } finally {
            if (webServer.getTestWebServer() != null) {
                webServer.getTestWebServer().shutdown();
            }
        }
    }

    private void testScrollTopLeftInteropState(boolean state) throws Throwable {
        final TestAwContentsClient client = new TestAwContentsClient();
        final AwTestContainerView view =
                mActivityTestRule.createAwTestContainerViewOnMainSync(client);
        final AwContents awContents = view.getAwContents();
        CallbackHelper onPageFinishedHelper = client.getOnPageFinishedHelper();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> awContents.getSettings().setScrollTopLeftInteropEnabled(state));
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        final String page =
                "<!doctype html><script>window.onload = function() {  document.title ="
                        + " document.scrollingElement === document.documentElement;};</script>";
        mActivityTestRule.loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        String actualTitle = mActivityTestRule.getTitleOnUiThread(awContents);
        Assert.assertEquals(state ? "true" : "false", actualTitle);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testScrollTopLeftInteropEnabled() throws Throwable {
        testScrollTopLeftInteropState(true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testScrollTopLeftInteropDisabled() throws Throwable {
        testScrollTopLeftInteropState(false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testAllowMixedMode() throws Throwable {
        final TestAwContentsClient contentClient =
                new TestAwContentsClient() {
                    @Override
                    public void onReceivedSslError(Callback<Boolean> callback, SslError error) {
                        callback.onResult(true);
                    }
                };
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        final AwSettings awSettings = mActivityTestRule.getAwSettingsOnUiThread(awContents);

        awSettings.setJavaScriptEnabled(true);

        TestWebServer httpsServer = null;
        TestWebServer httpServer = null;
        try {
            httpsServer = TestWebServer.startSsl();
            httpServer = TestWebServer.start();
            httpServer.setServerHost("example.com");
            httpsServer.setServerHost("secure.com");

            final String jsUrl = "/insecure.js";
            final String imageUrl = "/insecure.png";
            final String secureUrl = "/secure.html";
            httpServer.setResponse(jsUrl, "window.loaded_js = 42;", null);
            httpServer.setResponseBase64(imageUrl, CommonResources.FAVICON_DATA_BASE64, null);

            final String jsHtml =
                    "<script src=\"" + httpServer.getResponseUrl(jsUrl) + "\"></script>";
            final String imageHtml = "<img src=\"" + httpServer.getResponseUrl(imageUrl) + "\" />";
            final String secureHtml = "<body>" + imageHtml + " " + jsHtml + "</body>";

            String fullSecureUrl = httpsServer.setResponse(secureUrl, secureHtml, null);

            awSettings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
            mActivityTestRule.loadUrlSync(
                    awContents, contentClient.getOnPageFinishedHelper(), fullSecureUrl);
            Assert.assertEquals(1, httpsServer.getRequestCount(secureUrl));
            Assert.assertEquals(0, httpServer.getRequestCount(jsUrl));
            Assert.assertEquals(0, httpServer.getRequestCount(imageUrl));

            awSettings.setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);
            mActivityTestRule.loadUrlSync(
                    awContents, contentClient.getOnPageFinishedHelper(), fullSecureUrl);
            Assert.assertEquals(2, httpsServer.getRequestCount(secureUrl));
            Assert.assertEquals(1, httpServer.getRequestCount(jsUrl));
            Assert.assertEquals(1, httpServer.getRequestCount(imageUrl));

            awSettings.setMixedContentMode(WebSettings.MIXED_CONTENT_COMPATIBILITY_MODE);
            if (AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_MIXED_CONTENT_AUTOUPGRADES)) {
                // COMPATIBILITY_MODE enables autoupgrades for passive mixed content (including
                // images), so we set the image url to the HTTP version of the HTTPS server, and
                // check it was autoupgraded by expecting the HTTPS server to be hit.
                String httpImageUrl =
                        httpsServer.setResponseBase64(
                                imageUrl, CommonResources.FAVICON_DATA_BASE64, null);
                httpImageUrl = httpImageUrl.replaceFirst("https", "http");
                final String autoupgradedImageHtml = "<img src=\"" + httpImageUrl + "\" />";
                final String htmlForAutoupgrade =
                        "<body>" + autoupgradedImageHtml + " " + jsHtml + "</body>";
                fullSecureUrl = httpsServer.setResponse(secureUrl, htmlForAutoupgrade, null);
                mActivityTestRule.loadUrlSync(
                        awContents, contentClient.getOnPageFinishedHelper(), fullSecureUrl);
                Assert.assertEquals(1, httpsServer.getRequestCount(secureUrl));
                Assert.assertEquals(1, httpsServer.getRequestCount(imageUrl));
                Assert.assertEquals(1, httpServer.getRequestCount(jsUrl));
            } else {
                mActivityTestRule.loadUrlSync(
                        awContents, contentClient.getOnPageFinishedHelper(), fullSecureUrl);
                Assert.assertEquals(3, httpsServer.getRequestCount(secureUrl));
                Assert.assertEquals(1, httpServer.getRequestCount(jsUrl));
                Assert.assertEquals(2, httpServer.getRequestCount(imageUrl));
            }
        } finally {
            if (httpServer != null) {
                httpServer.shutdown();
            }
            if (httpsServer != null) {
                httpsServer.shutdown();
            }
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testUpdatingUserAgentWhileLoadingCausesReload() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        final AwSettings awSettings = mActivityTestRule.getAwSettingsOnUiThread(awContents);

        TestWebServer httpServer = null;
        try {
            httpServer = TestWebServer.start();

            String url =
                    httpServer.setResponseWithRunnableAction(
                            "/about.html",
                            "Hello, World!",
                            null,
                            () -> {
                                // This will update the UA string on the UI thread synchronously.
                                awSettings.setUserAgentString("UA Override");
                            });

            DoUpdateVisitedHistoryHelper doUpdateVisitedHistoryHelper =
                    contentClient.getDoUpdateVisitedHistoryHelper();
            int callCount = doUpdateVisitedHistoryHelper.getCallCount();
            mActivityTestRule.loadUrlAsync(awContents, url);
            doUpdateVisitedHistoryHelper.waitForCallback(callCount);
            Assert.assertEquals(url, doUpdateVisitedHistoryHelper.getUrl());
            Assert.assertEquals(true, doUpdateVisitedHistoryHelper.getIsReload());
        } finally {
            if (httpServer != null) {
                httpServer.shutdown();
            }
        }
    }

    private TestDependencyFactory mOverriddenFactory;

    @After
    public void tearDown() {
        mOverriddenFactory = null;
    }

    private static class EmptyDocumentPersistenceTestDependencyFactory
            extends TestDependencyFactory {
        private boolean mAllow;

        public EmptyDocumentPersistenceTestDependencyFactory(boolean allow) {
            mAllow = allow;
        }

        @Override
        public AwSettings createAwSettings(Context context, boolean supportsLegacyQuirks) {
            return new AwSettings(
                    context,
                    /* isAccessFromFileURLsGrantedByDefault= */ false,
                    supportsLegacyQuirks,
                    mAllow,
                    /* allowGeolocationOnInsecureOrigins= */ true,
                    /* doNotUpdateSelectionOnMutatingSelectionRange= */ false);
        }
    }

    private void doAllowEmptyDocumentPersistenceTest(boolean allow) throws Throwable {
        mOverriddenFactory = new EmptyDocumentPersistenceTestDependencyFactory(allow);

        final TestAwContentsClient client = new TestAwContentsClient();
        final AwTestContainerView mContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(client);
        final AwContents awContents = mContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        JSUtils.executeJavaScriptAndWaitForResult(
                InstrumentationRegistry.getInstrumentation(),
                awContents,
                client.getOnEvaluateJavaScriptResultHelper(),
                "window.emptyDocumentPersistenceTest = true;");
        mActivityTestRule.loadUrlSync(
                awContents,
                client.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        String result =
                JSUtils.executeJavaScriptAndWaitForResult(
                        InstrumentationRegistry.getInstrumentation(),
                        awContents,
                        client.getOnEvaluateJavaScriptResultHelper(),
                        "window.emptyDocumentPersistenceTest ? 'set' : 'not set';");
        Assert.assertEquals(allow ? "\"set\"" : "\"not set\"", result);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testAllowEmptyDocumentPersistence() throws Throwable {
        doAllowEmptyDocumentPersistenceTest(true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testDisallowEmptyDocumentPersistence() throws Throwable {
        doAllowEmptyDocumentPersistenceTest(false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testCSSHexAlphaColorEnabled() throws Throwable {
        final TestAwContentsClient client = new TestAwContentsClient();
        final AwTestContainerView view =
                mActivityTestRule.createAwTestContainerViewOnMainSync(client);
        final AwContents awContents = view.getAwContents();
        CallbackHelper onPageFinishedHelper = client.getOnPageFinishedHelper();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        final String expectedTitle = "false"; // https://crbug.com/618472
        final String page =
                "<!doctype html>"
                        + "<script>"
                        + "window.onload = function() {"
                        + "  document.title = CSS.supports('color', '#AABBCCDD');"
                        + "};"
                        + "</script>";
        // Loading the html via a data URI requires us to encode '#' symbols as '%23'.
        mActivityTestRule.loadDataSync(
                awContents, onPageFinishedHelper, page.replace("#", "%23"), "text/html", false);
        String actualTitle = mActivityTestRule.getTitleOnUiThread(awContents);
        Assert.assertEquals(expectedTitle, actualTitle);
    }

    private static class SelectionRangeTestDependencyFactory extends TestDependencyFactory {
        private boolean mDoNotUpdate;

        public SelectionRangeTestDependencyFactory(boolean doNotUpdate) {
            mDoNotUpdate = doNotUpdate;
        }

        @Override
        public AwSettings createAwSettings(Context context, boolean supportsLegacyQuirks) {
            return new AwSettings(
                    context,
                    /* isAccessFromFileURLsGrantedByDefault= */ false,
                    supportsLegacyQuirks,
                    /* allowEmptyDocumentPersistence= */ false,
                    /* allowGeolocationOnInsecureOrigins= */ true,
                    /* doNotUpdateSelectionOnMutatingSelectionRange= */ mDoNotUpdate);
        }
    }

    private void selectionUpdateOnMutatingSelectionRangeTest(boolean doNotUpdate) throws Throwable {
        mOverriddenFactory = new SelectionRangeTestDependencyFactory(doNotUpdate);

        final TestAwContentsClient client = new TestAwContentsClient();
        final AwTestContainerView mContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(client);
        final AwContents awContents = mContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        final String testPageHtml =
                "<html><head></head><body><div id='a' contenteditable></div><script>"
                        + "var cnt = 0;"
                        + "var a = document.getElementById('a');"
                        + "document.addEventListener('selectionchange', onSelectionChange, false);"
                        + "function onSelectionChange(event) {"
                        + "  cnt++;"
                        + "}"
                        + "</script></body></html>";
        mActivityTestRule.loadDataSync(
                awContents, client.getOnPageFinishedHelper(), testPageHtml, "text/html", false);

        // Focus on an empty DIV.
        JSUtils.executeJavaScriptAndWaitForResult(
                InstrumentationRegistry.getInstrumentation(),
                awContents,
                client.getOnEvaluateJavaScriptResultHelper(),
                "window.a.focus();");
        Assert.assertEquals(1, getSelectionChangeCountForSelectionUpdateTest(awContents, client));

        // Create and delete a zero-width space. See crbug.com/698752 for details.
        JSUtils.executeJavaScriptAndWaitForResult(
                InstrumentationRegistry.getInstrumentation(),
                awContents,
                client.getOnEvaluateJavaScriptResultHelper(),
                "(function() {"
                        + "var sel = window.getSelection();"
                        + "var range = sel.getRangeAt(0);"
                        + "var span = document.createElement('span');"
                        + "var textNodeForZWSP = document.createTextNode('\u200B');"
                        + "span.appendChild(textNodeForZWSP);"
                        + "range.insertNode(span);"
                        + "range.selectNode(span);"
                        + "range.deleteContents();"
                        + "}) ();");
        int expectedResult = doNotUpdate ? 0 : 1;
        Assert.assertEquals(
                expectedResult, getSelectionChangeCountForSelectionUpdateTest(awContents, client));
    }

    private void pollTitleAs(final String title, final AwContents awContents) {
        AwActivityTestRule.pollInstrumentationThread(
                () -> title.equals(mActivityTestRule.getTitleOnUiThread(awContents)));
    }

    private int getSelectionChangeCountForSelectionUpdateTest(
            AwContents awContents, TestAwContentsClient client) throws Exception {
        mTitleIdx++;
        String expectedTitle = Integer.toString(mTitleIdx);
        // Since selectionchange event is posted on a message loop, we run another message loop
        // before we get the result. On Chromium both run on the same message loop.
        JSUtils.executeJavaScriptAndWaitForResult(
                InstrumentationRegistry.getInstrumentation(),
                awContents,
                client.getOnEvaluateJavaScriptResultHelper(),
                "setTimeout(function() { document.title = '" + expectedTitle + "'; });");
        pollTitleAs(expectedTitle, awContents);

        String result =
                JSUtils.executeJavaScriptAndWaitForResult(
                        InstrumentationRegistry.getInstrumentation(),
                        awContents,
                        client.getOnEvaluateJavaScriptResultHelper(),
                        "window.cnt");
        // Clean up
        JSUtils.executeJavaScriptAndWaitForResult(
                InstrumentationRegistry.getInstrumentation(),
                awContents,
                client.getOnEvaluateJavaScriptResultHelper(),
                "window.cnt = 0;");
        return Integer.parseInt(result);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Selection"})
    public void testDoNotUpdateSelectionOnMutatingSelectionRange() throws Throwable {
        selectionUpdateOnMutatingSelectionRangeTest(true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Selection"})
    public void testUpdateSelectionOnMutatingSelectionRange() throws Throwable {
        selectionUpdateOnMutatingSelectionRangeTest(false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testGetUpdatedXRWAllowList() throws Throwable {
        TestAwContentsClient contentClient = new TestAwContentsClient();
        AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
        AwContents awContents = testContainerView.getAwContents();
        AwSettings awSettings = mActivityTestRule.getAwSettingsOnUiThread(awContents);

        final Set<String> allowList = Set.of("https://*.example.com", "https://*.google.com");

        Assert.assertEquals(
                Collections.emptySet(), awSettings.getRequestedWithHeaderOriginAllowList());

        awSettings.setRequestedWithHeaderOriginAllowList(allowList);

        Assert.assertEquals(allowList, awSettings.getRequestedWithHeaderOriginAllowList());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    public void testXRequestedWithAllowListSetByManifest() throws Throwable {
        final Set<String> allowList = Set.of("https://*.example.com", "https://*.google.com");
        try (var a = ManifestMetadataUtil.setXRequestedWithAllowListScopedForTesting(allowList)) {
            TestAwContentsClient contentClient = new TestAwContentsClient();
            AwTestContainerView testContainerView =
                    mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient);
            AwContents awContents = testContainerView.getAwContents();
            AwSettings awSettings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
            Set<String> changedList = awSettings.getRequestedWithHeaderOriginAllowList();
            Assert.assertEquals(allowList, changedList);
        }
    }

    static class ViewPair {
        private final AwTestContainerView mContainer0;
        private final TestAwContentsClient mClient0;
        private final AwTestContainerView mContainer1;
        private final TestAwContentsClient mClient1;

        ViewPair(
                AwTestContainerView container0,
                TestAwContentsClient client0,
                AwTestContainerView container1,
                TestAwContentsClient client1) {
            this.mContainer0 = container0;
            this.mClient0 = client0;
            this.mContainer1 = container1;
            this.mClient1 = client1;
        }

        AwTestContainerView getContainer0() {
            return mContainer0;
        }

        AwContents getContents0() {
            return mContainer0.getAwContents();
        }

        TestAwContentsClient getClient0() {
            return mClient0;
        }

        AwTestContainerView getContainer1() {
            return mContainer1;
        }

        AwContents getContents1() {
            return mContainer1.getAwContents();
        }

        TestAwContentsClient getClient1() {
            return mClient1;
        }
    }

    /**
     * Verifies the following statements about a setting:
     *  - initially, the setting has a default value;
     *  - the setting can be switched to an alternate value and back;
     *  - switching a setting in the first WebView doesn't affect the setting
     *    state in the second WebView and vice versa.
     *
     * @param helper0 Test helper for the first ContentView
     * @param helper1 Test helper for the second ContentView
     */
    private void runPerViewSettingsTest(
            AwSettingsTestHelper<?> helper0, AwSettingsTestHelper<?> helper1) throws Throwable {
        helper0.ensureSettingHasInitialValue();
        helper1.ensureSettingHasInitialValue();

        helper1.setAlteredSettingValue();
        helper0.ensureSettingHasInitialValue();
        helper1.ensureSettingHasAlteredValue();

        helper1.setInitialSettingValue();
        helper0.ensureSettingHasInitialValue();
        helper1.ensureSettingHasInitialValue();

        helper0.setAlteredSettingValue();
        helper0.ensureSettingHasAlteredValue();
        helper1.ensureSettingHasInitialValue();

        helper0.setInitialSettingValue();
        helper0.ensureSettingHasInitialValue();
        helper1.ensureSettingHasInitialValue();

        helper0.setAlteredSettingValue();
        helper0.ensureSettingHasAlteredValue();
        helper1.ensureSettingHasInitialValue();

        helper1.setAlteredSettingValue();
        helper0.ensureSettingHasAlteredValue();
        helper1.ensureSettingHasAlteredValue();

        helper0.setInitialSettingValue();
        helper0.ensureSettingHasInitialValue();
        helper1.ensureSettingHasAlteredValue();

        helper1.setInitialSettingValue();
        helper0.ensureSettingHasInitialValue();
        helper1.ensureSettingHasInitialValue();
    }

    private ViewPair createViews() {
        return createViews(false);
    }

    private ViewPair createViews(boolean supportsLegacyQuirks) {
        TestAwContentsClient client0 = new TestAwContentsClient();
        TestAwContentsClient client1 = new TestAwContentsClient();
        return new ViewPair(
                mActivityTestRule.createAwTestContainerViewOnMainSync(
                        client0, supportsLegacyQuirks),
                client0,
                mActivityTestRule.createAwTestContainerViewOnMainSync(
                        client1, supportsLegacyQuirks),
                client1);
    }

    static void assertFileIsReadable(String filePath) {
        File file = new File(filePath);
        try {
            Assert.assertTrue(
                    "Test file \""
                            + filePath
                            + "\" is not readable."
                            + "Please make sure that files from "
                            + "android_webview/test/data/device_files/ has been pushed to the "
                            + "device before testing",
                    file.canRead());
        } catch (SecurityException e) {
            Assert.fail("Got a SecurityException for \"" + filePath + "\": " + e.toString());
        }
    }

    /**
     * Verifies the number of resource requests made to the content provider.
     * @param resource Resource name
     * @param expectedCount Expected resource requests count
     */
    private void ensureResourceRequestCountInContentProvider(String resource, int expectedCount) {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        int actualCount = TestContentProvider.getResourceRequestCount(context, resource);
        Assert.assertEquals(expectedCount, actualCount);
    }

    private void resetResourceRequestCountInContentProvider(String resource) {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        TestContentProvider.resetResourceRequestCount(context, resource);
    }

    private String createContentUrl(final String target) {
        return TestContentProvider.createContentUrl(target);
    }

    private void simulateDoubleTapCenterOfWebViewOnUiThread(final AwTestContainerView webView) {
        final int x = (webView.getRight() - webView.getLeft()) / 2;
        final int y = (webView.getBottom() - webView.getTop()) / 2;
        final AwContents awContents = webView.getAwContents();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () ->
                                awContents
                                        .getWebContents()
                                        .getEventForwarder()
                                        .doubleTapForTest(SystemClock.uptimeMillis(), x, y));
    }

    private boolean isTablet() {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivityTestRule.getActivity());
    }
}
