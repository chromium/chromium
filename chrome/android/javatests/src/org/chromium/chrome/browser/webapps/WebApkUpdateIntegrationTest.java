// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.PackageManagerWrapper;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.test.MockCertVerifierRuleAndroid;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.components.webapk.proto.WebApkProto;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.io.FileInputStream;

/** Integration tests for WebAPK feature. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "The update pipeline runs once per startup.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeSwitches.CHECK_FOR_WEB_MANIFEST_UPDATE_ON_STARTUP})
public class WebApkUpdateIntegrationTest {
    public final WebApkActivityTestRule mActivityTestRule = new WebApkActivityTestRule();

    public MockCertVerifierRuleAndroid mCertVerifierRule =
            new MockCertVerifierRuleAndroid(0 /* net::OK */);

    @Rule
    public RuleChain mRuleChain =
            RuleChain.emptyRuleChain().around(mActivityTestRule).around(mCertVerifierRule);

    private static final String TAG = "WebApkIntegratTest";

    private static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.test";

    // Android Manifest meta data for {@link WEBAPK_PACKAGE_NAME}.
    // TODO(eirage): change all to use mTestServer.
    private static final String WEBAPK_MANIFEST_URL = "/chrome/test/data/banners/manifest.json";
    private static final String WEBAPK_START_URL =
            "/chrome/test/data/banners/manifest_test_page.html";
    private static final String WEBAPK_SCOPE_URL = "/chrome/test/data/banners/";
    private static final String WEBAPK_NAME = "Manifest test app";
    private static final String WEBAPK_SHORT_NAME = "Manifest test app";
    private static final String ICON_URL = "/chrome/test/data/banners/image-512px.png";
    private static final String ICON_MURMUR2_HASH = "7742433188808797392";
    private static final String DISPLAY_MODE = "standalone";
    private static final String ORIENTATION = "portrait";
    private static final int SHELL_APK_VERSION = 1000;
    private static final String THEME_COLOR = "1L";
    private static final String BACKGROUND_COLOR = "2L";

    private EmbeddedTestServer mTestServer;
    private Context mContextToRestore;
    private TestContext mTestContext;

    private Bundle mTestMetaData;

    private class TestContext extends ContextWrapper {
        public TestContext(Context baseContext) {
            super(baseContext);
        }

        @Override
        public PackageManager getPackageManager() {
            return new PackageManagerWrapper(super.getPackageManager()) {
                @Override
                public ApplicationInfo getApplicationInfo(String packageName, int flags) {
                    try {
                        ApplicationInfo ai = super.getApplicationInfo(packageName, flags);
                        if (TextUtils.equals(packageName, WEBAPK_PACKAGE_NAME)) {
                            ai.metaData = mTestMetaData;
                        }
                        return ai;
                    } catch (Exception e) {
                        Log.e(TAG, "Failed to get application info for %s", packageName, e);
                    }
                    return null;
                }
            };
        }
    }

    @Before
    public void setUp() {
        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        mContextToRestore = ContextUtils.getApplicationContext();
        mTestContext = new TestContext(mContextToRestore);
        ContextUtils.initApplicationContextForTests(mTestContext);
        mTestServer = mActivityTestRule.getTestServer();
        mTestMetaData = defaultMetaData();

        WebApkValidator.setDisableValidationForTesting(true);
        WebApkUpdateManager.setUpdatesEnabledForTesting(true);
    }

    @After
    public void tearDown() {
        if (mContextToRestore != null) {
            ContextUtils.initApplicationContextForTests(mContextToRestore);
        }
    }

    private Bundle defaultMetaData() {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.NAME, WEBAPK_NAME);
        bundle.putString(WebApkMetaDataKeys.SHORT_NAME, WEBAPK_SHORT_NAME);
        bundle.putString(WebApkMetaDataKeys.DISPLAY_MODE, DISPLAY_MODE);
        bundle.putString(WebApkMetaDataKeys.ORIENTATION, ORIENTATION);
        bundle.putString(WebApkMetaDataKeys.THEME_COLOR, THEME_COLOR);
        bundle.putString(WebApkMetaDataKeys.BACKGROUND_COLOR, BACKGROUND_COLOR);
        bundle.putInt(WebApkMetaDataKeys.SHELL_APK_VERSION, SHELL_APK_VERSION);
        bundle.putString(
                WebApkMetaDataKeys.WEB_MANIFEST_URL, mTestServer.getURL(WEBAPK_MANIFEST_URL));
        bundle.putString(WebApkMetaDataKeys.START_URL, mTestServer.getURL(WEBAPK_START_URL));
        bundle.putString(WebApkMetaDataKeys.SCOPE, mTestServer.getURL(WEBAPK_SCOPE_URL));
        bundle.putString(WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES,
                ICON_URL + " " + ICON_MURMUR2_HASH);
        return bundle;
    }

    // Wait for the name change dialog and dismiss it.
    private void waitForDialog() {
        CriteriaHelper.pollUiThread(() -> {
            ModalDialogManager manager = mActivityTestRule.getActivity().getModalDialogManager();
            PropertyModel dialog = manager.getCurrentDialogForTest();
            if (dialog == null) return false;
            dialog.get(ModalDialogProperties.CONTROLLER)
                    .onClick(dialog, ModalDialogProperties.ButtonType.POSITIVE);
            return true;
        });
    }

    private void waitForHistogram(String name, int count) {
        CriteriaHelper.pollUiThread(() -> {
            return RecordHistogram.getHistogramTotalCountForTesting(name) >= count;
        }, "waitForHistogram timeout", 10000, 200);
    }

    private WebApkProto.WebApk parseRequestProto(String path) throws Exception {
        FileInputStream requestFile = new FileInputStream(path);
        return WebApkProto.WebApk.parseFrom(requestFile);
    }

    /*
     * Test update flow triggered after WebAPK launch.
     */
    @Test
    @LargeTest
    @Feature({"Webapps"})
    public void testStoreUpdateRequestToFile() throws Exception {
        String pageUrl = mTestServer.getURL(WEBAPK_START_URL);

        WebappActivity activity = mActivityTestRule.startWebApkActivity(pageUrl);
        assertEquals(ActivityType.WEB_APK, activity.getActivityType());
        assertEquals(pageUrl, activity.getIntentDataProvider().getUrlToLoad());

        waitForDialog();
        waitForHistogram("WebApk.Update.RequestQueued", 1);

        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(
                WebApkConstants.WEBAPK_ID_PREFIX + WEBAPK_PACKAGE_NAME);
        String updateRequestPath = storage.getPendingUpdateRequestPath();
        assertNotNull(updateRequestPath);

        WebApkProto.WebApk proto = parseRequestProto(updateRequestPath);

        assertEquals(proto.getPackageName(), WEBAPK_PACKAGE_NAME);
        assertEquals(proto.getVersion(), "1");
        assertEquals(proto.getManifestUrl(), mTestServer.getURL(WEBAPK_MANIFEST_URL));
        assertEquals(proto.getAppKey(), mTestServer.getURL(WEBAPK_MANIFEST_URL));
        assertEquals(proto.getManifest().getName(), WEBAPK_NAME);
        assertEquals(proto.getManifest().getShortName(), WEBAPK_SHORT_NAME);
        assertEquals(proto.getManifest().getStartUrl(), mTestServer.getURL(WEBAPK_START_URL));
        assertEquals(proto.getManifest().getScopes(0), mTestServer.getURL(WEBAPK_SCOPE_URL));
        assertEquals(proto.getManifest().getId(), mTestServer.getURL(WEBAPK_START_URL));
        assertEquals(proto.getManifest().getIconsCount(), 2);
        assertEquals(proto.getManifest().getOrientation(), "landscape");
        assertEquals(proto.getManifest().getDisplayMode(), "standalone");
    }
}
