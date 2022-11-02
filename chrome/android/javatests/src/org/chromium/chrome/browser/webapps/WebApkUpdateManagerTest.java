// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.content.Intent;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.webapps.WebappTestPage;
import org.chromium.components.webapk.proto.WebApkProto;
import org.chromium.components.webapps.WebApkDistributor;
import org.chromium.components.webapps.WebApkUpdateReason;
import org.chromium.components.webapps.WebappsIconUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Tests WebApkUpdateManager. This class contains tests which cannot be done as JUnit tests.
 */
@RunWith(ParameterizedRunner.class)
@DoNotBatch(reason = "The update pipeline runs once per startup.")
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeSwitches.CHECK_FOR_WEB_MANIFEST_UPDATE_ON_STARTUP})
@EnableFeatures(ChromeFeatureList.WEB_APK_UNIQUE_ID)
public class WebApkUpdateManagerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    /**
     * The parameters for the App Identity tests (for which flag is enabled).
     */
    public static class FeatureResolveParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Arrays.asList(new ParameterSet().value(false, false, false).name("none"),
                    new ParameterSet().value(true, false, false).name("nameUpdates"),
                    new ParameterSet().value(false, true, false).name("iconUpdates"),
                    new ParameterSet().value(true, true, false).name("nameAndIconUpdates"),
                    new ParameterSet().value(false, false, true).name("allowForShellVersion"));
        }
    }

    private static final String WEBAPK_ID = "webapk_id";
    private static final String WEBAPK_MANIFEST_URL =
            "/chrome/test/data/banners/manifest_one_icon.json";
    private static final String WEBAPK_MANIFEST_TOO_MANY_SHORTCUTS_URL =
            "/chrome/test/data/banners/manifest_too_many_shortcuts.json";

    // manifest_one_icon_maskable.json is the same as manifest_one_icon.json except that it has an
    // additional icon of purpose maskable and of same size.
    private static final String WEBAPK_MANIFEST_WITH_MASKABLE_ICON_URL =
            "/chrome/test/data/banners/manifest_maskable.json";

    // Data contained in {@link WEBAPK_MANIFEST_URL}.
    private static final String WEBAPK_START_URL =
            "/chrome/test/data/banners/manifest_test_page.html";
    private static final String WEBAPK_SCOPE_URL = "/chrome/test/data/banners/";
    private static final String WEBAPK_NAME = "Manifest test app";
    private static final String WEBAPK_SHORT_NAME = "Manifest test app";
    private static final String WEBAPK_ICON_URL = "/chrome/test/data/banners/image-512px.png";
    private static final String WEBAPK_ICON_MURMUR2_HASH = "7742433188808797392";
    private static final @DisplayMode.EnumType int WEBAPK_DISPLAY_MODE = DisplayMode.STANDALONE;
    private static final int WEBAPK_ORIENTATION = ScreenOrientationLockType.LANDSCAPE;
    private static final int WEBAPK_SHELL_VERSION = 1000;
    private static final long WEBAPK_THEME_COLOR = 2147483648L;
    private static final long WEBAPK_BACKGROUND_COLOR = 2147483648L;

    private ChromeActivity mActivity;
    private Tab mTab;
    private EmbeddedTestServer mTestServer;
    private FeatureList.TestValues mTestValues;

    private List<Integer> mLastUpdateReasons;
    private String mUpdateRequestPath;

    // Whether the dialog, to warn about icon/names changing, was shown.
    private boolean mIconOrNameUpdateDialogShown;

    // Whether an update was requested in the end.
    private boolean mUpdateRequested;

    /**
     * Subclass of {@link WebApkUpdateManager} which notifies the {@link CallbackHelper} passed to
     * the constructor when it has been determined whether an update is needed.
     */
    private class TestWebApkUpdateManager extends WebApkUpdateManager {
        private CallbackHelper mWaiter;
        private CallbackHelper mCompleteCallback;
        private boolean mAcceptDialogIfAppears;

        public TestWebApkUpdateManager(CallbackHelper waiter, CallbackHelper complete,
                ActivityTabProvider tabProvider, ActivityLifecycleDispatcher lifecycleDispatcher,
                boolean acceptDialogIfAppears) {
            super(tabProvider, lifecycleDispatcher);
            mWaiter = waiter;
            mCompleteCallback = complete;
            mLastUpdateReasons = new ArrayList<>();
            mUpdateRequestPath = null;
            mAcceptDialogIfAppears = acceptDialogIfAppears;
        }

        @Override
        public void onGotManifestData(BrowserServicesIntentDataProvider fetchedInfo,
                String primaryIconUrl, String splashIconUrl) {
            super.onGotManifestData(fetchedInfo, primaryIconUrl, splashIconUrl);
            mWaiter.notifyCalled();
        }

        @Override
        protected void encodeIconsInBackground(String updateRequestPath, WebappInfo info,
                String primaryIconUrl, String splashIconUrl, boolean isManifestStale,
                boolean isAppIdentityUpdateSupported, List<Integer> updateReasons,
                Callback<Boolean> callback) {
            mLastUpdateReasons = updateReasons;
            mUpdateRequestPath = updateRequestPath;
            super.encodeIconsInBackground(updateRequestPath, info, primaryIconUrl, splashIconUrl,
                    isManifestStale, isAppIdentityUpdateSupported, updateReasons, callback);
        }

        @Override
        protected void showIconOrNameUpdateDialog(
                boolean iconChanging, boolean shortNameChanging, boolean nameChanging) {
            mIconOrNameUpdateDialogShown = true;
            super.showIconOrNameUpdateDialog(iconChanging, shortNameChanging, nameChanging);
            ModalDialogManager modalDialogManager =
                    mActivityTestRule.getActivity().getModalDialogManager();
            modalDialogManager.getCurrentPresenterForTest().dismissCurrentDialog(
                    mAcceptDialogIfAppears ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                           : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }

        @Override
        protected void onUserApprovedUpdate(int dismissalCause) {
            mUpdateRequested = dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED;
            super.onUserApprovedUpdate(dismissalCause);
        }

        @Override
        protected void scheduleUpdate() {
            if (mCompleteCallback != null) mCompleteCallback.notifyCalled();
        }
    }

    private static class CreationData {
        public String manifestUrl;
        public String startUrl;
        public String scope;
        public String name;
        public String shortName;
        public String manifestId;
        public String appKey;
        public Map<String, String> iconUrlToMurmur2HashMap;
        public @DisplayMode.EnumType int displayMode;
        public int orientation;
        public int shellVersion;
        public long themeColor;
        public long backgroundColor;
        public boolean isPrimaryIconMaskable;
        public List<WebApkExtras.ShortcutItem> shortcuts;
    }

    public CreationData defaultCreationData() {
        CreationData creationData = new CreationData();
        creationData.manifestUrl = mTestServer.getURL(WEBAPK_MANIFEST_URL);
        creationData.startUrl = mTestServer.getURL(WEBAPK_START_URL);
        creationData.scope = mTestServer.getURL(WEBAPK_SCOPE_URL);
        creationData.manifestId = mTestServer.getURL(WEBAPK_START_URL);
        creationData.appKey = mTestServer.getURL(WEBAPK_MANIFEST_URL);
        creationData.name = WEBAPK_NAME;
        creationData.shortName = WEBAPK_SHORT_NAME;

        creationData.iconUrlToMurmur2HashMap = new HashMap<String, String>();
        creationData.iconUrlToMurmur2HashMap.put(
                mTestServer.getURL(WEBAPK_ICON_URL), WEBAPK_ICON_MURMUR2_HASH);

        creationData.displayMode = WEBAPK_DISPLAY_MODE;
        creationData.orientation = WEBAPK_ORIENTATION;
        creationData.themeColor = WEBAPK_THEME_COLOR;
        creationData.backgroundColor = WEBAPK_BACKGROUND_COLOR;
        creationData.shellVersion = WEBAPK_SHELL_VERSION;
        creationData.isPrimaryIconMaskable = false;
        creationData.shortcuts = new ArrayList<>();
        return creationData;
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        mTab = mActivity.getActivityTab();
        mTestServer = mTestServerRule.getServer();

        mTestValues = new FeatureList.TestValues();
        FeatureList.setTestValues(mTestValues);

        TestFetchStorageCallback callback = new TestFetchStorageCallback();
        WebappRegistry.getInstance().register(WEBAPK_ID, callback);
        callback.waitForCallback(0);
    }

     /** Checks whether a WebAPK update is needed. */
    private boolean checkUpdateNeeded(
            final CreationData creationData, boolean acceptDialogIfAppears) throws Exception {
        return checkUpdateNeeded(creationData, null, acceptDialogIfAppears);
    }

    private boolean checkUpdateNeeded(final CreationData creationData,
            CallbackHelper completeCallback, boolean acceptDialogIfAppears) throws Exception {
        CallbackHelper waiter = new CallbackHelper();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(waiter,
                    completeCallback, mActivity.getActivityTabProvider(),
                    mActivity.getLifecycleDispatcher(), acceptDialogIfAppears);
            WebappDataStorage storage =
                    WebappRegistry.getInstance().getWebappDataStorage(WEBAPK_ID);
            BrowserServicesIntentDataProvider intentDataProvider =
                    WebApkIntentDataProviderFactory.create(new Intent(), "", creationData.scope,
                            null, null, creationData.name, creationData.shortName,
                            creationData.displayMode, creationData.orientation, 0,
                            creationData.themeColor, creationData.backgroundColor, 0,
                            creationData.isPrimaryIconMaskable, false /* isSplashIconMaskable */,
                            "", creationData.shellVersion, creationData.manifestUrl,
                            creationData.startUrl, creationData.manifestId, creationData.appKey,
                            WebApkDistributor.BROWSER, creationData.iconUrlToMurmur2HashMap, null,
                            false /* forceNavigation */, false /* isSplashProvidedByWebApk */,
                            null /* shareData */, creationData.shortcuts,
                            1 /* webApkVersionCode */);
            updateManager.updateIfNeeded(storage, intentDataProvider);
        });
        waiter.waitForCallback(0);
        return !mLastUpdateReasons.isEmpty();
    }

    /* Check that an update is needed and wait for it to complete. */
    private void waitForUpdate(final CreationData creationData) throws Exception {
        CallbackHelper waiter = new CallbackHelper();
        Assert.assertTrue(
                checkUpdateNeeded(creationData, waiter, true /* acceptDialogIfAppears */));
        waiter.waitForCallback(0);
    }

    private void assertUpdateReasonsEqual(@WebApkUpdateReason Integer... reasons) {
        Assert.assertEquals(Arrays.asList(reasons), mLastUpdateReasons);
    }

    private WebApkProto.WebApk parseRequestProto(String path) throws Exception {
        FileInputStream requestFile = new FileInputStream(path);
        return WebApkProto.WebApk.parseFrom(requestFile);
    }

    private void enableUpdateDialogForIcon(boolean enabled) {
        mTestValues.addFeatureFlagOverride(ChromeFeatureList.PWA_UPDATE_DIALOG_FOR_ICON, enabled);
        FeatureList.setTestValues(mTestValues);
    }

    private void enableUpdateDialogForName(boolean enabled) {
        mTestValues.addFeatureFlagOverride(ChromeFeatureList.PWA_UPDATE_DIALOG_FOR_NAME, enabled);
        FeatureList.setTestValues(mTestValues);
    }

    /**
     * Test that the canonicalized URLs are used in determining whether the fetched Web Manifest
     * data differs from the metadata in the WebAPK's Android Manifest. This is important because
     * the URLs in the Web Manifest have been modified by the WebAPK server prior to being stored in
     * the WebAPK Android Manifest. Chrome and the WebAPK server parse URLs differently.
     */
    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testCanonicalUrlsIdenticalShouldNotUpgrade() throws Exception {
        // URL canonicalization should replace "%74" with 't'.
        CreationData creationData = defaultCreationData();
        creationData.startUrl =
                mTestServer.getURL("/chrome/test/data/banners/manifest_%74est_page.html");

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);
        Assert.assertFalse(checkUpdateNeeded(creationData, /* acceptDialogIfAppears= */ false));
    }

    /**
     * Test that an upgraded WebAPK is requested if the canonicalized "start URLs" are different.
     */
    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testCanonicalUrlsDifferentShouldUpgrade() throws Exception {
        // URL canonicalization should replace "%62" with 'b'.
        CreationData creationData = defaultCreationData();
        creationData.startUrl =
                mTestServer.getURL("/chrome/test/data/banners/manifest_%62est_page.html");

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);
        Assert.assertTrue(checkUpdateNeeded(creationData, /* acceptDialogIfAppears= */ false));
        assertUpdateReasonsEqual(WebApkUpdateReason.START_URL_DIFFERS);
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testNoUpdateForPagesWithoutWST() throws Exception {
        CreationData creationData = defaultCreationData();
        creationData.startUrl =
                mTestServer.getURL("/chrome/test/data/banners/manifest_test_page.html");

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);
        Assert.assertFalse(checkUpdateNeeded(creationData, /* acceptDialogIfAppears= */ false));
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testMaskableIconShouldUpdate() throws Exception {
        CreationData creationData = defaultCreationData();
        creationData.manifestUrl = mTestServer.getURL(WEBAPK_MANIFEST_WITH_MASKABLE_ICON_URL);

        creationData.iconUrlToMurmur2HashMap = new HashMap<String, String>();
        creationData.iconUrlToMurmur2HashMap.put(
                mTestServer.getURL("/chrome/test/data/banners/launcher-icon-4x.png"),
                "8692598279279335241");
        creationData.iconUrlToMurmur2HashMap.put(
                mTestServer.getURL("/chrome/test/data/banners/launcher-icon-3x.png"),
                "16812314236514539104");

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_WITH_MASKABLE_ICON_URL);

        // Icon changes should trigger the warning dialog, if the platform supports maskable icons.
        enableUpdateDialogForIcon(true);
        Assert.assertEquals(WebappsIconUtils.doesAndroidSupportMaskableIcons(),
                checkUpdateNeeded(creationData, /* acceptDialogIfAppears= */ true));
        boolean supportsMaskableIcons = WebappsIconUtils.doesAndroidSupportMaskableIcons();
        if (supportsMaskableIcons) {
            assertUpdateReasonsEqual(WebApkUpdateReason.PRIMARY_ICON_MASKABLE_DIFFERS);
        }
        Assert.assertEquals(supportsMaskableIcons, mUpdateRequested);
        Assert.assertEquals(supportsMaskableIcons, mIconOrNameUpdateDialogShown);
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testManifestWithExtraShortcutsDoesNotCauseUpdate() throws Exception {
        CreationData creationData = defaultCreationData();
        creationData.startUrl =
                mTestServer.getURL("/chrome/test/data/banners/manifest_test_page.html");

        creationData.manifestUrl = mTestServer.getURL(WEBAPK_MANIFEST_TOO_MANY_SHORTCUTS_URL);
        for (int i = 0; i < 4; i++) {
            creationData.shortcuts.add(new WebApkExtras.ShortcutItem("name" + String.valueOf(i),
                    "short_name", mTestServer.getURL(WEBAPK_SCOPE_URL + "launch_url"), "", "",
                    new WebappIcon()));
        }

        // The fifth shortcut should be ignored.
        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_TOO_MANY_SHORTCUTS_URL);
        Assert.assertFalse(checkUpdateNeeded(creationData, /* acceptDialogIfAppears= */ false));
    }

    private void resolveFeatureParams(
            boolean nameDialogEnabled, boolean iconDialogEnabled, boolean allowShellVersion) {
        enableUpdateDialogForName(nameDialogEnabled);
        enableUpdateDialogForIcon(iconDialogEnabled);

        if (allowShellVersion) {
            mTestValues.addFieldTrialParamOverride(ChromeFeatureList.WEB_APK_ALLOW_ICON_UPDATA,
                    "shell_version", Integer.toString(WEBAPK_SHELL_VERSION));
        }
        FeatureList.setTestValues(mTestValues);
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    public void testMultipleUpdateReasons(
            boolean nameEnabled, boolean iconEnabled, boolean allowShellVersion) throws Exception {
        resolveFeatureParams(nameEnabled, iconEnabled, allowShellVersion);

        CreationData creationData = defaultCreationData();
        creationData.startUrl =
                mTestServer.getURL("/chrome/test/data/banners/manifest_test_page.html");

        creationData.name += "!";
        creationData.shortName += "!";
        creationData.backgroundColor -= 1;
        creationData.iconUrlToMurmur2HashMap.put(
                mTestServer.getURL(WEBAPK_ICON_URL), WEBAPK_ICON_MURMUR2_HASH + "1");

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);
        Assert.assertTrue(checkUpdateNeeded(creationData,
                /* acceptDialogIfAppears= */ nameEnabled || iconEnabled));

        List<Integer> expectedUpdateReasons = new ArrayList<Integer>();
        if (iconEnabled || allowShellVersion) {
            expectedUpdateReasons.add(WebApkUpdateReason.PRIMARY_ICON_HASH_DIFFERS);
            expectedUpdateReasons.add(WebApkUpdateReason.SPLASH_ICON_HASH_DIFFERS);
        }
        if (nameEnabled) {
            expectedUpdateReasons.add(WebApkUpdateReason.SHORT_NAME_DIFFERS);
            expectedUpdateReasons.add(WebApkUpdateReason.NAME_DIFFERS);
        }
        expectedUpdateReasons.add(WebApkUpdateReason.BACKGROUND_COLOR_DIFFERS);
        assertUpdateReasonsEqual(
                expectedUpdateReasons.toArray(new Integer[expectedUpdateReasons.size()]));
    }

    private void testAppIdentityChange(boolean nameEnabled, boolean iconEnabled,
            boolean allowShellVersion, boolean changeName, boolean changeShortName,
            boolean changeIcon) throws Exception {
        mIconOrNameUpdateDialogShown = false;

        CreationData creationData = defaultCreationData();
        creationData.startUrl =
                mTestServer.getURL("/chrome/test/data/banners/manifest_test_page.html");

        // Add to this list in order of increasing numerical value represented by the enum.
        List<Integer> expectedUpdateReasons = new ArrayList<Integer>();

        boolean expectIconChange = false;
        boolean expectNameChange = false;

        if (changeIcon) {
            creationData.iconUrlToMurmur2HashMap.put(
                    mTestServer.getURL(WEBAPK_ICON_URL), WEBAPK_ICON_MURMUR2_HASH + "1");

            expectIconChange = iconEnabled || allowShellVersion;
            if (expectIconChange) {
                expectedUpdateReasons.add(WebApkUpdateReason.PRIMARY_ICON_HASH_DIFFERS);
                expectedUpdateReasons.add(WebApkUpdateReason.SPLASH_ICON_HASH_DIFFERS);
            }
        }
        if (changeShortName) {
            creationData.shortName += "!";

            expectNameChange = nameEnabled;
            if (expectNameChange) {
                expectedUpdateReasons.add(WebApkUpdateReason.SHORT_NAME_DIFFERS);
            }
        }
        if (changeName) {
            creationData.name += "!";

            expectNameChange = nameEnabled;
            if (expectNameChange) {
                expectedUpdateReasons.add(WebApkUpdateReason.NAME_DIFFERS);
            }
        }

        // Always include a trivial change, to ensure there's always an update request.
        creationData.backgroundColor -= 1;
        expectedUpdateReasons.add(WebApkUpdateReason.BACKGROUND_COLOR_DIFFERS);

        boolean requestingChange = changeIcon || changeName || changeShortName;
        boolean expectingChange = expectIconChange || expectNameChange;
        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);
        Assert.assertTrue(checkUpdateNeeded(
                creationData, /* acceptDialogIfAppears= */ requestingChange && expectingChange));

        assertUpdateReasonsEqual(
                expectedUpdateReasons.toArray(new Integer[expectedUpdateReasons.size()]));
        Assert.assertTrue(mUpdateRequested);

        boolean expectingDialog = (expectIconChange && iconEnabled) || expectNameChange;
        Assert.assertEquals(expectingDialog, mIconOrNameUpdateDialogShown);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnNoChange(
            boolean nameEnabled, boolean iconEnabled, boolean allowShellVersion) throws Exception {
        resolveFeatureParams(nameEnabled, iconEnabled, allowShellVersion);

        testAppIdentityChange(nameEnabled, iconEnabled, allowShellVersion,
                /* changeName= */ false,
                /* changeShortName= */ false,
                /* changeIcon = */ false);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnIconChange(
            boolean nameEnabled, boolean iconEnabled, boolean allowShellVersion) throws Exception {
        resolveFeatureParams(nameEnabled, iconEnabled, allowShellVersion);

        testAppIdentityChange(nameEnabled, iconEnabled, allowShellVersion,
                /* changeName= */ false,
                /* changeShortName= */ false,
                /* changeIcon = */ true);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnShortnameChange(
            boolean nameEnabled, boolean iconEnabled, boolean allowShellVersion) throws Exception {
        resolveFeatureParams(nameEnabled, iconEnabled, allowShellVersion);
        testAppIdentityChange(nameEnabled, iconEnabled, allowShellVersion,
                /* changeName= */ false,
                /* changeShortName= */ true,
                /* changeIcon = */ false);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnShortnameAndIconChange(
            boolean nameEnabled, boolean iconEnabled, boolean allowShellVersion) throws Exception {
        resolveFeatureParams(nameEnabled, iconEnabled, allowShellVersion);
        testAppIdentityChange(nameEnabled, iconEnabled, allowShellVersion,
                /* changeName= */ false,
                /* changeShortName= */ true,
                /* changeIcon = */ true);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnNameChange(
            boolean nameEnabled, boolean iconEnabled, boolean allowShellVersion) throws Exception {
        resolveFeatureParams(nameEnabled, iconEnabled, allowShellVersion);
        testAppIdentityChange(nameEnabled, iconEnabled, allowShellVersion,
                /* changeName= */ true,
                /* changeShortName= */ false,
                /* changeIcon = */ false);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnNameAndIconChange(
            boolean nameEnabled, boolean iconEnabled, boolean allowShellVersion) throws Exception {
        resolveFeatureParams(nameEnabled, iconEnabled, allowShellVersion);
        testAppIdentityChange(nameEnabled, iconEnabled, allowShellVersion,
                /* changeName= */ true,
                /* changeShortName= */ false,
                /* changeIcon = */ true);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnNameAndShortnameChange(
            boolean nameEnabled, boolean iconEnabled, boolean allowShellVersion) throws Exception {
        resolveFeatureParams(nameEnabled, iconEnabled, allowShellVersion);
        testAppIdentityChange(nameEnabled, iconEnabled, allowShellVersion,
                /* changeName= */ true,
                /* changeShortName= */ true,
                /* changeIcon = */ false);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnAllChange(
            boolean nameEnabled, boolean iconEnabled, boolean allowShellVersion) throws Exception {
        resolveFeatureParams(nameEnabled, iconEnabled, allowShellVersion);
        testAppIdentityChange(nameEnabled, iconEnabled, allowShellVersion,
                /* changeName= */ true,
                /* changeShortName= */ true,
                /* changeIcon = */ true);
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testAllowIconUpdateForVersion() throws Exception {
        mTestValues.addFieldTrialParamOverride(ChromeFeatureList.WEB_APK_ALLOW_ICON_UPDATA,
                "shell_version", Integer.toString(WEBAPK_SHELL_VERSION));
        FeatureList.setTestValues(mTestValues);

        CreationData creationData = defaultCreationData();
        creationData.iconUrlToMurmur2HashMap.put(
                mTestServer.getURL(WEBAPK_ICON_URL), WEBAPK_ICON_MURMUR2_HASH + "1");

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);

        Assert.assertTrue(checkUpdateNeeded(creationData, /* acceptDialogIfAppears= */ false));
        assertUpdateReasonsEqual(WebApkUpdateReason.PRIMARY_ICON_HASH_DIFFERS,
                WebApkUpdateReason.SPLASH_ICON_HASH_DIFFERS);
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testAllowIconUpdateNoUpdateForNewerShell() throws Exception {
        mTestValues.addFieldTrialParamOverride(ChromeFeatureList.WEB_APK_ALLOW_ICON_UPDATA,
                "shell_version", Integer.toString(WEBAPK_SHELL_VERSION - 1));
        FeatureList.setTestValues(mTestValues);

        CreationData creationData = defaultCreationData();
        creationData.iconUrlToMurmur2HashMap.put(
                mTestServer.getURL(WEBAPK_ICON_URL), WEBAPK_ICON_MURMUR2_HASH + "1");

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);

        Assert.assertFalse(checkUpdateNeeded(creationData, /* acceptDialogIfAppears= */ false));
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testUniqueIdUpdateFromLegacyApp() throws Exception {
        CreationData legacyWebApkData = defaultCreationData();
        legacyWebApkData.manifestId = null;
        legacyWebApkData.backgroundColor -= 1;

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);
        waitForUpdate(legacyWebApkData);

        assertNotNull(mUpdateRequestPath);
        WebApkProto.WebApk proto = parseRequestProto(mUpdateRequestPath);

        assertEquals(proto.getAppKey(), mTestServer.getURL(WEBAPK_MANIFEST_URL));
        assertEquals(proto.getManifest().getId(), mTestServer.getURL(WEBAPK_START_URL));
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testUniqueIdUpdateKeepId() throws Exception {
        CreationData creationData = defaultCreationData();
        creationData.manifestId = mTestServer.getURL(WEBAPK_START_URL);
        creationData.appKey = mTestServer.getURL("/appKey");
        creationData.backgroundColor -= 1;

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);
        waitForUpdate(creationData);

        assertNotNull(mUpdateRequestPath);
        WebApkProto.WebApk proto = parseRequestProto(mUpdateRequestPath);

        assertEquals(proto.getAppKey(), creationData.appKey);
        assertEquals(proto.getManifest().getId(), creationData.manifestId);
    }
}
