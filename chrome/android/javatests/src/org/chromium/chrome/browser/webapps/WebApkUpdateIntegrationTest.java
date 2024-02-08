// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
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
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.CHECK_FOR_WEB_MANIFEST_UPDATE_ON_STARTUP
})
public class WebApkUpdateIntegrationTest {
    public final WebApkActivityTestRule mActivityTestRule = new WebApkActivityTestRule();

    public MockCertVerifierRuleAndroid mCertVerifierRule =
            new MockCertVerifierRuleAndroid(0 /* net::OK */);

    @Rule
    public RuleChain mRuleChain =
            RuleChain.emptyRuleChain().around(mActivityTestRule).around(mCertVerifierRule);

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
    private static final String ICON_URL2 = "/chrome/test/data/banners/512x512-red.png";
    private static final String ICON_MURMUR2_HASH2 = "7742433188808797392";
    private static final String DISPLAY_MODE = "standalone";
    private static final String ORIENTATION = "portrait";
    private static final int SHELL_APK_VERSION = 1000;
    private static final String THEME_COLOR = "1L";
    private static final String BACKGROUND_COLOR = "2L";
    private static final String DARK_THEME_COLOR = "3L";
    private static final String DARK_BACKGROUND_COLOR = "4L";

    private EmbeddedTestServer mTestServer;
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
                    }
                    return null;
                }
            };
        }
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        mTestContext = new TestContext(ContextUtils.getApplicationContext());
        ContextUtils.initApplicationContextForTests(mTestContext);
        mTestServer = mActivityTestRule.getTestServer();
        mTestMetaData = defaultMetaData();

        WebApkValidator.setDisableValidationForTesting(true);
        WebApkUpdateManager.setUpdatesDisabledForTesting(false);
    }

    private Bundle defaultMetaData() throws Exception {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.NAME, WEBAPK_NAME);
        bundle.putString(WebApkMetaDataKeys.SHORT_NAME, WEBAPK_SHORT_NAME);
        bundle.putString(WebApkMetaDataKeys.DISPLAY_MODE, DISPLAY_MODE);
        bundle.putString(WebApkMetaDataKeys.ORIENTATION, ORIENTATION);
        bundle.putString(WebApkMetaDataKeys.THEME_COLOR, THEME_COLOR);
        bundle.putString(WebApkMetaDataKeys.BACKGROUND_COLOR, BACKGROUND_COLOR);
        bundle.putString(WebApkMetaDataKeys.DARK_THEME_COLOR, DARK_THEME_COLOR);
        bundle.putString(WebApkMetaDataKeys.DARK_BACKGROUND_COLOR, DARK_BACKGROUND_COLOR);
        bundle.putInt(WebApkMetaDataKeys.SHELL_APK_VERSION, SHELL_APK_VERSION);
        bundle.putString(
                WebApkMetaDataKeys.WEB_MANIFEST_URL, mTestServer.getURL(WEBAPK_MANIFEST_URL));
        bundle.putString(WebApkMetaDataKeys.START_URL, mTestServer.getURL(WEBAPK_START_URL));
        bundle.putString(WebApkMetaDataKeys.SCOPE, mTestServer.getURL(WEBAPK_SCOPE_URL));
        Resources res =
                mTestContext.getPackageManager().getResourcesForApplication(WEBAPK_PACKAGE_NAME);
        bundle.putInt(
                WebApkMetaDataKeys.ICON_ID,
                res.getIdentifier("app_icon", "mipmap", WEBAPK_PACKAGE_NAME));
        bundle.putInt(
                WebApkMetaDataKeys.SPLASH_ID,
                res.getIdentifier("splash_icon", "drawable", WEBAPK_PACKAGE_NAME));

        bundle.putString(
                WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES,
                String.join(
                        " ",
                        mTestServer.getURL(ICON_URL),
                        ICON_MURMUR2_HASH,
                        mTestServer.getURL(ICON_URL2),
                        ICON_MURMUR2_HASH2));
        return bundle;
    }

    // Wait for the name change dialog and dismiss it.
    private void waitForDialog() {
        CriteriaHelper.pollUiThread(
                () -> {
                    ModalDialogManager manager =
                            mActivityTestRule.getActivity().getModalDialogManager();
                    PropertyModel dialog = manager.getCurrentDialogForTest();
                    if (dialog == null) return false;
                    dialog.get(ModalDialogProperties.CONTROLLER)
                            .onClick(dialog, ModalDialogProperties.ButtonType.POSITIVE);
                    return true;
                });
    }

    private void waitForHistogram() {}

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
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "WebApk.Update.ShellVersion", SHELL_APK_VERSION);

        WebappActivity activity = mActivityTestRule.startWebApkActivity(pageUrl);
        assertEquals(ActivityType.WEB_APK, activity.getActivityType());
        assertEquals(pageUrl, activity.getIntentDataProvider().getUrlToLoad());

        waitForDialog();
        histogramWatcher.pollInstrumentationThreadUntilSatisfied();

        WebappDataStorage storage =
                WebappRegistry.getInstance()
                        .getWebappDataStorage(
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
        assertEquals(proto.getManifest().getOrientation(), "landscape");
        assertEquals(proto.getManifest().getDisplayMode(), "standalone");

        assertEquals(proto.getManifest().getIconsCount(), 3);
        // 1st: primary icon from old shell icon, has image data but no hash.
        WebApkProto.Image icon1 = proto.getManifest().getIconsList().get(0);
        assertFalse(icon1.hasSrc());
        assertFalse(icon1.hasHash());
        assertTrue(icon1.hasImageData());
        assertFalse(icon1.getImageData().isEmpty());
        assertEquals(icon1.getPurposesCount(), 1);
        assertEquals(icon1.getPurposesList().get(0), WebApkProto.Image.Purpose.ANY);
        assertEquals(icon1.getUsagesCount(), 1);
        assertEquals(icon1.getUsagesList().get(0), WebApkProto.Image.Usage.PRIMARY_ICON);

        // 2nd: splash icon url matches the hash map. has image data and hash.
        WebApkProto.Image icon2 = proto.getManifest().getIconsList().get(1);
        assertEquals(icon2.getSrc(), mTestServer.getURL(ICON_URL));
        assertEquals(icon2.getHash(), ICON_MURMUR2_HASH);
        assertTrue(icon2.hasImageData());
        assertFalse(icon2.getImageData().isEmpty());
        assertEquals(icon2.getPurposesCount(), 1);
        assertEquals(icon2.getPurposesList().get(0), WebApkProto.Image.Purpose.ANY);
        assertEquals(icon2.getUsagesCount(), 1);
        assertEquals(icon2.getUsagesList().get(0), WebApkProto.Image.Usage.SPLASH_ICON);

        // 3nd icon from the url2hash map, has url and hash but no data.
        WebApkProto.Image icon3 = proto.getManifest().getIconsList().get(2);
        assertEquals(icon3.getSrc(), mTestServer.getURL(ICON_URL2));
        assertEquals(icon3.getHash(), ICON_MURMUR2_HASH2);
        assertFalse(icon3.hasImageData());
    }
}
