// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.text.format.DateUtils;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
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
import org.chromium.chrome.test.util.browser.webapps.WebappTestPage;
import org.chromium.components.webapk.proto.WebApkProto;
import org.chromium.components.webapps.WebApkDistributor;
import org.chromium.components.webapps.WebApkUpdateReason;
import org.chromium.components.webapps.WebappsIconUtils;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Tests WebApkUpdateManager. This class contains tests which cannot be done as JUnit tests. */
@RunWith(ParameterizedRunner.class)
@Batch(Batch.PER_CLASS)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.CHECK_FOR_WEB_MANIFEST_UPDATE_ON_STARTUP
})
public class WebApkUpdateManagerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    /** The parameters for the App Identity tests (for which flag is enabled). */
    public static class FeatureResolveParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            // The parameters provided for the tests below are:
            // iconDialogEnabled: whether updates to app identity are enabled via app id dialog.
            // allowShellVersion: whether a silent icon update should occur.
            // iconChangeSignificant: whether to treat the icon change as above threshold (blocked).
            return Arrays.asList(
                    new ParameterSet().value(false, false, true).name("majorIconUpdateNoFlag"),
                    new ParameterSet().value(false, false, false).name("minorIconUpdateNoFlag"),
                    new ParameterSet().value(true, false, true).name("majorIconUpdateDlgEnabled"),
                    new ParameterSet().value(true, false, false).name("minorIconUpdateDlgEnabled"),
                    new ParameterSet().value(false, true, true).name("majorIconUpdateViaShell"),
                    new ParameterSet().value(false, true, false).name("minorIconUpdateViaShell"),
                    new ParameterSet().value(true, true, true).name("majorIconUpdateBothFlags"),
                    new ParameterSet().value(true, true, false).name("minorIconUpdateBothFlags"));
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
    private static final long WEBAPK_DARK_THEME_COLOR = 2147483648L;
    private static final long WEBAPK_DARK_BACKGROUND_COLOR = 2147483648L;

    private static final String HISTOGRAM = "WebApk.AppIdentityDialog.PendingImageUpdateDiffValue";
    private static final String HISTOGRAM_SCALED =
            "WebApk.AppIdentityDialog.PendingImageUpdateDiffValueScaled";

    private static final Bitmap BLACK_1X1 =
            Bitmap.createBitmap(new int[] {0x00000000, 0x00000000}, 1, 1, Bitmap.Config.ARGB_8888);
    private static final Bitmap BLACK_2X2 =
            Bitmap.createBitmap(
                    new int[] {0x00000000, 0x00000000, 0x00000000, 0x00000000},
                    2,
                    2,
                    Bitmap.Config.ARGB_8888);
    private static final Bitmap WHITE_2X2 =
            Bitmap.createBitmap(
                    new int[] {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
                    2,
                    2,
                    Bitmap.Config.ARGB_8888);
    private static final Bitmap CHECKERED_2X2 =
            Bitmap.createBitmap(
                    new int[] {0x00000000, 0xffffffff, 0x00000000, 0xffffffff},
                    2,
                    2,
                    Bitmap.Config.ARGB_8888);
    private static final Bitmap CONFIG_MISMATCH_2X2 =
            Bitmap.createBitmap(
                    new int[] {0x00000000, 0x00000000, 0x00000000, 0x00000000},
                    2,
                    2,
                    Config.ALPHA_8);

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

        public TestWebApkUpdateManager(
                Activity activity,
                CallbackHelper waiter,
                CallbackHelper complete,
                ActivityTabProvider tabProvider,
                ActivityLifecycleDispatcher lifecycleDispatcher,
                boolean acceptDialogIfAppears) {
            super(activity, tabProvider, lifecycleDispatcher);
            mWaiter = waiter;
            mCompleteCallback = complete;
            mLastUpdateReasons = new ArrayList<>();
            mUpdateRequestPath = null;
            mAcceptDialogIfAppears = acceptDialogIfAppears;
        }

        @Override
        public void onGotManifestData(
                BrowserServicesIntentDataProvider fetchedInfo,
                String primaryIconUrl,
                String splashIconUrl) {
            super.onGotManifestData(fetchedInfo, primaryIconUrl, splashIconUrl);
            mWaiter.notifyCalled();
        }

        @Override
        protected void encodeIconsInBackground(
                String updateRequestPath,
                WebappInfo info,
                String primaryIconUrl,
                String splashIconUrl,
                boolean isManifestStale,
                boolean isAppIdentityUpdateSupported,
                List<Integer> updateReasons,
                Callback<Boolean> callback) {
            mLastUpdateReasons = updateReasons;
            mUpdateRequestPath = updateRequestPath;
            super.encodeIconsInBackground(
                    updateRequestPath,
                    info,
                    primaryIconUrl,
                    splashIconUrl,
                    isManifestStale,
                    isAppIdentityUpdateSupported,
                    updateReasons,
                    callback);
        }

        @Override
        protected void showIconOrNameUpdateDialog(
                boolean iconChanging, boolean shortNameChanging, boolean nameChanging) {
            mIconOrNameUpdateDialogShown = true;
            super.showIconOrNameUpdateDialog(iconChanging, shortNameChanging, nameChanging);
            ModalDialogManager modalDialogManager =
                    mActivityTestRule.getActivity().getModalDialogManager();
            modalDialogManager
                    .getCurrentPresenterForTest()
                    .dismissCurrentDialog(
                            mAcceptDialogIfAppears
                                    ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                    : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }

        @Override
        protected void onUserApprovedUpdate(int dismissalCause) {
            mUpdateRequested = dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED;
            super.onUserApprovedUpdate(dismissalCause);
        }

        @Override
        protected void scheduleUpdate(int shellVersion) {
            if (mCompleteCallback != null) mCompleteCallback.notifyCalled();
        }

        @Override
        protected long updateTimeoutMilliseconds() {
            return DateUtils.SECOND_IN_MILLIS * 3;
        }
    }

    private static class CreationData {
        public String manifestUrl;
        public String startUrl;
        public String scope;
        public WebappIcon primaryIcon;
        public WebappIcon splashIcon;
        public String name;
        public String shortName;
        public boolean hasCustomName;
        public String manifestId;
        public String appKey;
        public Map<String, String> iconUrlToMurmur2HashMap;
        public @DisplayMode.EnumType int displayMode;
        public int orientation;
        public int shellVersion;
        public long themeColor;
        public long backgroundColor;
        public long darkThemeColor;
        public long darkBackgroundColor;
        public boolean isPrimaryIconMaskable;
        public List<WebApkExtras.ShortcutItem> shortcuts;
    }

    public CreationData defaultCreationData() {
        CreationData creationData = new CreationData();
        creationData.manifestUrl = mTestServer.getURL(WEBAPK_MANIFEST_URL);
        creationData.startUrl = mTestServer.getURL(WEBAPK_START_URL);
        creationData.primaryIcon = new WebappIcon(BLACK_2X2);
        creationData.splashIcon = null;
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
        creationData.darkThemeColor = WEBAPK_DARK_THEME_COLOR;
        creationData.darkBackgroundColor = WEBAPK_DARK_BACKGROUND_COLOR;
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
        mTestServer = mActivityTestRule.getTestServer();

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

    private boolean checkUpdateNeeded(
            final CreationData creationData,
            CallbackHelper completeCallback,
            boolean acceptDialogIfAppears)
            throws Exception {
        CallbackHelper waiter = new CallbackHelper();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TestWebApkUpdateManager updateManager =
                            new TestWebApkUpdateManager(
                                    mActivity,
                                    waiter,
                                    completeCallback,
                                    mActivity.getActivityTabProvider(),
                                    mActivity.getLifecycleDispatcher(),
                                    acceptDialogIfAppears);
                    WebappDataStorage storage =
                            WebappRegistry.getInstance().getWebappDataStorage(WEBAPK_ID);
                    BrowserServicesIntentDataProvider intentDataProvider =
                            WebApkIntentDataProviderFactory.create(
                                    new Intent(),
                                    "",
                                    creationData.scope,
                                    creationData.primaryIcon,
                                    creationData.splashIcon,
                                    creationData.name,
                                    creationData.shortName,
                                    creationData.hasCustomName,
                                    creationData.displayMode,
                                    creationData.orientation,
                                    0,
                                    creationData.themeColor,
                                    creationData.backgroundColor,
                                    creationData.darkThemeColor,
                                    creationData.darkBackgroundColor,
                                    0,
                                    creationData.isPrimaryIconMaskable,
                                    /* isSplashIconMaskable= */ false,
                                    "",
                                    creationData.shellVersion,
                                    creationData.manifestUrl,
                                    creationData.startUrl,
                                    creationData.manifestId,
                                    creationData.appKey,
                                    WebApkDistributor.BROWSER,
                                    creationData.iconUrlToMurmur2HashMap,
                                    null,
                                    /* forceNavigation= */ false,
                                    /* isSplashProvidedByWebApk= */ false,
                                    /* shareData= */ null,
                                    creationData.shortcuts,
                                    /* webApkVersionCode= */ 1,
                                    /* lastUpdateTime= */ TimeUtils.currentTimeMillis());
                    updateManager.updateIfNeeded(storage, intentDataProvider);
                });
        waiter.waitForCallback(0);
        return !mLastUpdateReasons.isEmpty();
    }

    /* Check that an update is needed and wait for it to complete. */
    private void waitForUpdate(final CreationData creationData) throws Exception {
        CallbackHelper waiter = new CallbackHelper();
        Assert.assertTrue(
                checkUpdateNeeded(creationData, waiter, /* acceptDialogIfAppears= */ true));
        waiter.waitForCallback(0);
    }

    private void assertUpdateReasonsEqual(@WebApkUpdateReason Integer... reasons) {
        List<Integer> reasonsArray = Arrays.asList(reasons);
        Collections.sort(reasonsArray);
        Collections.sort(mLastUpdateReasons);
        Assert.assertEquals(reasonsArray, mLastUpdateReasons);
    }

    private WebApkProto.WebApk parseRequestProto(String path) throws Exception {
        FileInputStream requestFile = new FileInputStream(path);
        return WebApkProto.WebApk.parseFrom(requestFile);
    }

    private void enableUpdateDialogForIcon(boolean enabled) {
        mTestValues.addFeatureFlagOverride(ChromeFeatureList.PWA_UPDATE_DIALOG_FOR_ICON, enabled);
        FeatureList.setTestValues(mTestValues);
    }

    /**
     * Test that the canonicalized URLs are used in determining whether the fetched Web Manifest
     * data differs from the metadata in the WebAPK's Android Manifest. This is important because
     * the URLs in the Web Manifest have been modified by the WebAPK server prior to being stored in
     * the WebAPK Android Manifest. Chrome and the WebAPK server used to parse URLs differently.
     *
     * <p>TODO(crbug.com/40279669): We probably no longer need this test because
     * https://crbug.com/1252531 was fixed. Someone familiar with the context of this test might
     * want to update or remove this test.
     */
    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testCanonicalUrlsDifferentShouldUpgrade() throws Exception {
        // URL canonicalization in Chrome used to replace "%74" with 't', however, that is no longer
        // true. "%74" should not be replaced with 't'.
        CreationData creationData = defaultCreationData();
        creationData.startUrl =
                mTestServer.getURL("/chrome/test/data/banners/manifest_%74est_page.html");

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
        Assert.assertEquals(
                WebappsIconUtils.doesAndroidSupportMaskableIcons(),
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
            creationData.shortcuts.add(
                    new WebApkExtras.ShortcutItem(
                            "name" + String.valueOf(i),
                            "short_name",
                            mTestServer.getURL(WEBAPK_SCOPE_URL + "launch_url"),
                            "",
                            "",
                            new WebappIcon()));
        }

        // The fifth shortcut should be ignored.
        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_TOO_MANY_SHORTCUTS_URL);
        Assert.assertFalse(checkUpdateNeeded(creationData, /* acceptDialogIfAppears= */ false));
    }

    private void resolveFeatureParams(
            boolean iconDialogEnabled, boolean allowShellVersion, boolean iconChangeSignificant) {
        enableUpdateDialogForIcon(iconDialogEnabled);

        if (allowShellVersion) {
            mTestValues.addFieldTrialParamOverride(
                    ChromeFeatureList.WEB_APK_ALLOW_ICON_UPDATE,
                    "shell_version",
                    Integer.toString(WEBAPK_SHELL_VERSION));
        }

        FeatureList.setTestValues(mTestValues);

        // The same icon is always used for tests, but the threshold is raised or lowered, depending
        // on the outcome we want.
        WebApkUpdateManager.setIconThresholdForTesting(iconChangeSignificant ? 0 : 101);
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    public void testMultipleUpdateReasons(
            boolean iconEnabled, boolean allowShellVersion, boolean iconChangeSignificant)
            throws Exception {
        resolveFeatureParams(iconEnabled, allowShellVersion, iconChangeSignificant);

        CreationData creationData = defaultCreationData();
        creationData.startUrl =
                mTestServer.getURL("/chrome/test/data/banners/manifest_test_page.html");

        List<Integer> expectedUpdateReasons = new ArrayList<Integer>();
        creationData.name += "!";
        creationData.shortName += "!";
        creationData.backgroundColor -= 1;
        creationData.darkBackgroundColor -= 1;
        creationData.iconUrlToMurmur2HashMap.put(
                mTestServer.getURL(WEBAPK_ICON_URL), WEBAPK_ICON_MURMUR2_HASH + "1");

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);
        Assert.assertTrue(checkUpdateNeeded(creationData, /* acceptDialogIfAppears= */ true));

        if (iconEnabled || allowShellVersion || !iconChangeSignificant) {
            expectedUpdateReasons.add(WebApkUpdateReason.PRIMARY_ICON_HASH_DIFFERS);
            expectedUpdateReasons.add(WebApkUpdateReason.SPLASH_ICON_HASH_DIFFERS);
            if (allowShellVersion) {
                expectedUpdateReasons.add(WebApkUpdateReason.PRIMARY_ICON_CHANGE_SHELL_UPDATE);
            }
            if (!iconChangeSignificant) {
                expectedUpdateReasons.add(WebApkUpdateReason.PRIMARY_ICON_CHANGE_BELOW_THRESHOLD);
            }
        }
        expectedUpdateReasons.add(WebApkUpdateReason.SHORT_NAME_DIFFERS);
        expectedUpdateReasons.add(WebApkUpdateReason.NAME_DIFFERS);
        expectedUpdateReasons.add(WebApkUpdateReason.BACKGROUND_COLOR_DIFFERS);
        expectedUpdateReasons.add(WebApkUpdateReason.DARK_BACKGROUND_COLOR_DIFFERS);
        assertUpdateReasonsEqual(
                expectedUpdateReasons.toArray(new Integer[expectedUpdateReasons.size()]));
    }

    private void testAppIdentityChange(
            boolean iconEnabled,
            boolean allowShellVersion,
            boolean iconChangeSignificant,
            boolean changeName,
            boolean changeShortName,
            boolean changeIcon)
            throws Exception {
        mIconOrNameUpdateDialogShown = false;
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(WEBAPK_ID);
        storage.updateLastWebApkUpdateHashAccepted("");

        CreationData creationData = defaultCreationData();
        creationData.startUrl =
                mTestServer.getURL("/chrome/test/data/banners/manifest_test_page.html");

        List<Integer> expectedUpdateReasons = new ArrayList<Integer>();

        boolean expectIconChange = false;

        if (changeIcon) {
            creationData.iconUrlToMurmur2HashMap.put(
                    mTestServer.getURL(WEBAPK_ICON_URL), WEBAPK_ICON_MURMUR2_HASH + "1");
            creationData.primaryIcon = new WebappIcon(WHITE_2X2);

            expectIconChange = iconEnabled || allowShellVersion || !iconChangeSignificant;
            if (expectIconChange) {
                expectedUpdateReasons.add(WebApkUpdateReason.PRIMARY_ICON_HASH_DIFFERS);
                expectedUpdateReasons.add(WebApkUpdateReason.SPLASH_ICON_HASH_DIFFERS);
                if (allowShellVersion) {
                    expectedUpdateReasons.add(WebApkUpdateReason.PRIMARY_ICON_CHANGE_SHELL_UPDATE);
                }
                if (!iconChangeSignificant) {
                    expectedUpdateReasons.add(
                            WebApkUpdateReason.PRIMARY_ICON_CHANGE_BELOW_THRESHOLD);
                }
            }
        }
        if (changeShortName) {
            creationData.shortName += "!";
            expectedUpdateReasons.add(WebApkUpdateReason.SHORT_NAME_DIFFERS);
        }
        if (changeName) {
            creationData.name += "!";
            expectedUpdateReasons.add(WebApkUpdateReason.NAME_DIFFERS);
        }

        // Include a trivial change, to ensure there's always an update request.
        creationData.backgroundColor -= 1;
        expectedUpdateReasons.add(WebApkUpdateReason.BACKGROUND_COLOR_DIFFERS);

        boolean requestingChange = changeIcon || changeName || changeShortName;
        boolean expectingChange = expectIconChange || changeName || changeShortName;
        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);
        Assert.assertTrue(
                checkUpdateNeeded(
                        creationData,
                        /* acceptDialogIfAppears= */ requestingChange && expectingChange));

        assertUpdateReasonsEqual(
                expectedUpdateReasons.toArray(new Integer[expectedUpdateReasons.size()]));
        Assert.assertTrue(mUpdateRequested);

        // The App Identity Update dialog should always appear if:
        // - Either the name or shortName changes.
        // - Icon changes are enabled via the dialog, and there is a *significant* icon change.
        //   - Except when shell version is being updated (those should be silent).
        boolean expectingDialog =
                changeShortName
                        || changeName
                        || (expectIconChange
                                && iconEnabled
                                && iconChangeSignificant
                                && !allowShellVersion);
        Assert.assertEquals(expectingDialog, mIconOrNameUpdateDialogShown);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnNoChange(
            boolean iconEnabled, boolean allowShellVersion, boolean iconChangeSignificant)
            throws Exception {
        resolveFeatureParams(iconEnabled, allowShellVersion, iconChangeSignificant);

        testAppIdentityChange(
                iconEnabled,
                allowShellVersion,
                iconChangeSignificant,
                /* changeName= */ false,
                /* changeShortName= */ false,
                /* changeIcon= */ false);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnIconChange(
            boolean iconEnabled, boolean allowShellVersion, boolean iconChangeSignificant)
            throws Exception {
        resolveFeatureParams(iconEnabled, allowShellVersion, iconChangeSignificant);

        testAppIdentityChange(
                iconEnabled,
                allowShellVersion,
                iconChangeSignificant,
                /* changeName= */ false,
                /* changeShortName= */ false,
                /* changeIcon= */ true);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnShortnameChange(
            boolean iconEnabled, boolean allowShellVersion, boolean iconChangeSignificant)
            throws Exception {
        resolveFeatureParams(iconEnabled, allowShellVersion, iconChangeSignificant);
        testAppIdentityChange(
                iconEnabled,
                allowShellVersion,
                iconChangeSignificant,
                /* changeName= */ false,
                /* changeShortName= */ true,
                /* changeIcon= */ false);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnShortnameAndIconChange(
            boolean iconEnabled, boolean allowShellVersion, boolean iconChangeSignificant)
            throws Exception {
        resolveFeatureParams(iconEnabled, allowShellVersion, iconChangeSignificant);
        testAppIdentityChange(
                iconEnabled,
                allowShellVersion,
                iconChangeSignificant,
                /* changeName= */ false,
                /* changeShortName= */ true,
                /* changeIcon= */ true);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnNameChange(
            boolean iconEnabled, boolean allowShellVersion, boolean iconChangeSignificant)
            throws Exception {
        resolveFeatureParams(iconEnabled, allowShellVersion, iconChangeSignificant);
        testAppIdentityChange(
                iconEnabled,
                allowShellVersion,
                iconChangeSignificant,
                /* changeName= */ true,
                /* changeShortName= */ false,
                /* changeIcon= */ false);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnNameAndIconChange(
            boolean iconEnabled, boolean allowShellVersion, boolean iconChangeSignificant)
            throws Exception {
        resolveFeatureParams(iconEnabled, allowShellVersion, iconChangeSignificant);
        testAppIdentityChange(
                iconEnabled,
                allowShellVersion,
                iconChangeSignificant,
                /* changeName= */ true,
                /* changeShortName= */ false,
                /* changeIcon= */ true);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnNameAndShortnameChange(
            boolean iconEnabled, boolean allowShellVersion, boolean iconChangeSignificant)
            throws Exception {
        resolveFeatureParams(iconEnabled, allowShellVersion, iconChangeSignificant);
        testAppIdentityChange(
                iconEnabled,
                allowShellVersion,
                iconChangeSignificant,
                /* changeName= */ true,
                /* changeShortName= */ true,
                /* changeIcon= */ false);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnAllChange(
            boolean iconEnabled, boolean allowShellVersion, boolean iconChangeSignificant)
            throws Exception {
        resolveFeatureParams(iconEnabled, allowShellVersion, iconChangeSignificant);
        testAppIdentityChange(
                iconEnabled,
                allowShellVersion,
                iconChangeSignificant,
                /* changeName= */ true,
                /* changeShortName= */ true,
                /* changeIcon= */ true);
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testAllowIconUpdateForVersion() throws Exception {
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.WEB_APK_ALLOW_ICON_UPDATE,
                "shell_version",
                Integer.toString(WEBAPK_SHELL_VERSION));
        FeatureList.setTestValues(mTestValues);

        CreationData creationData = defaultCreationData();
        creationData.iconUrlToMurmur2HashMap.put(
                mTestServer.getURL(WEBAPK_ICON_URL), WEBAPK_ICON_MURMUR2_HASH + "1");

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);

        Assert.assertTrue(checkUpdateNeeded(creationData, /* acceptDialogIfAppears= */ false));
        assertUpdateReasonsEqual(
                WebApkUpdateReason.PRIMARY_ICON_HASH_DIFFERS,
                WebApkUpdateReason.SPLASH_ICON_HASH_DIFFERS,
                WebApkUpdateReason.PRIMARY_ICON_CHANGE_SHELL_UPDATE);
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testAllowIconUpdateNoUpdateForNewerShell() throws Exception {
        mTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.WEB_APK_ALLOW_ICON_UPDATE,
                "shell_version",
                Integer.toString(WEBAPK_SHELL_VERSION - 1));
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

    /*
     *Test update will not be trigger with different startUrl and manifestUrl.
     */
    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testEmptyUniqueIdNotUpdateWithDifferentUrls() throws Exception {
        CreationData legacyWebApkData = defaultCreationData();
        // Set the original WebAPK startUrl and manifestUrl to be different ones.
        legacyWebApkData.startUrl = "https://www.example.com";
        legacyWebApkData.manifestUrl = "https://www.example.com";
        legacyWebApkData.manifestId = null;
        legacyWebApkData.backgroundColor -= 1;

        // Navigate to a page with different manifestUrl and startUrl.
        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);
        Assert.assertFalse(checkUpdateNeeded(legacyWebApkData, /* acceptDialogIfAppears= */ false));
    }

    /*
     *Test trigger an update with same startUrl but different manifestUrl.
     */
    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testEmptyUniqueIdUpdateWithDifferentManifestUrl() throws Exception {
        // Test trigger an update with same startUrl but different manifestUrl.
        CreationData legacyWebApkData = defaultCreationData();
        legacyWebApkData.manifestUrl = "https://www.example.com";
        legacyWebApkData.manifestId = null;
        legacyWebApkData.backgroundColor -= 1;

        // Navigate to a page with different manifestUrl.
        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);
        Assert.assertTrue(checkUpdateNeeded(legacyWebApkData, /* acceptDialogIfAppears= */ false));
    }

    /*
     *Test trigger an update with same manifestUrl but different startUrl.
     */
    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testEmptyUniqueIdUpdateWithDifferentStartUrl() throws Exception {
        CreationData legacyWebApkData = defaultCreationData();
        legacyWebApkData.startUrl = "https://www.example.com";
        legacyWebApkData.manifestId = null;
        legacyWebApkData.backgroundColor -= 1;

        // Navigate to a page with different manifestUrl.
        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);
        Assert.assertTrue(checkUpdateNeeded(legacyWebApkData, /* acceptDialogIfAppears= */ false));
    }

    /*
     * Test navigate to page with manifest under different scope will not trigger updates.
     */
    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testNoUpdateForPageOutOfScope() throws Exception {
        CreationData creationData = defaultCreationData();
        creationData.scope = mTestServer.getURL("/chrome/test/data/another_scope/");
        creationData.backgroundColor -= 1;

        // Navigate to a page under different scope.
        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);
        Assert.assertFalse(checkUpdateNeeded(creationData, /* acceptDialogIfAppears= */ false));
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testEmptyUniqueIdStaleManifestUpdate() throws Exception {
        CreationData creationData = defaultCreationData();
        creationData.manifestId = null;
        // Set a small shellVersion to force a stale manifest update.
        creationData.shellVersion = -1;

        mActivityTestRule.loadUrl(mTestServer.getURL("/"));

        waitForUpdate(creationData);
        assertUpdateReasonsEqual(WebApkUpdateReason.OLD_SHELL_APK);

        assertNotNull(mUpdateRequestPath);
        WebApkProto.WebApk proto = parseRequestProto(mUpdateRequestPath);
        assertEquals(proto.getAppKey(), mTestServer.getURL(WEBAPK_MANIFEST_URL));
        assertEquals(proto.getManifest().getId(), mTestServer.getURL(WEBAPK_START_URL));
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testImageDiff() throws Exception {
        // A black image compared against a white one should register as all changed.
        assertEquals(100, TestWebApkUpdateManager.imageDiffValue(BLACK_2X2, WHITE_2X2));
        // Alternating black and white pixels should show as half changed when compared against all
        // black image.
        assertEquals(50, TestWebApkUpdateManager.imageDiffValue(BLACK_2X2, CHECKERED_2X2));
        // Alternating black and white pixels should show as half changed when compared against all
        // white image.
        assertEquals(50, TestWebApkUpdateManager.imageDiffValue(CHECKERED_2X2, WHITE_2X2));
        // Two all black images should register as unchanged.
        assertEquals(0, TestWebApkUpdateManager.imageDiffValue(CHECKERED_2X2, CHECKERED_2X2));

        // Two null images register as unchanged.
        assertEquals(0, TestWebApkUpdateManager.imageDiffValue(null, null));
        // If 'before' is provided, but 'after' is null, they should register as 100% different.
        assertEquals(100, TestWebApkUpdateManager.imageDiffValue(BLACK_2X2, null));
        // If 'after' is provided, but 'before' is null, they should register as 100% different.
        assertEquals(100, TestWebApkUpdateManager.imageDiffValue(null, WHITE_2X2));
        // Images with different color configurations should register as 100% different.
        assertEquals(100, TestWebApkUpdateManager.imageDiffValue(BLACK_2X2, CONFIG_MISMATCH_2X2));
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testImageDiffHistograms() throws Exception {
        // Comparing two identical bitmaps should record a 0% change on the main histogram.
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder().expectIntRecord(HISTOGRAM, 0).build();
        TestWebApkUpdateManager.logIconDiffs(BLACK_2X2, BLACK_2X2);
        histograms.assertExpected();

        // Comparing two black bitmaps of different dimensions should procude a 0% change on the
        // scaled histogram.
        histograms = HistogramWatcher.newBuilder().expectIntRecord(HISTOGRAM_SCALED, 0).build();
        TestWebApkUpdateManager.logIconDiffs(BLACK_1X1, BLACK_2X2);
        histograms.assertExpected();

        // Comparing a black bitmap to a larger white bitmap should produce a 100% change
        // on the scaled histogram.
        histograms = HistogramWatcher.newBuilder().expectIntRecord(HISTOGRAM_SCALED, 100).build();
        TestWebApkUpdateManager.logIconDiffs(BLACK_1X1, WHITE_2X2);
        histograms.assertExpected();

        // A checkered image compared to a white one should produce a 50% change
        // on the main histogram.
        histograms = HistogramWatcher.newBuilder().expectIntRecord(HISTOGRAM, 50).build();
        TestWebApkUpdateManager.logIconDiffs(CHECKERED_2X2, WHITE_2X2);
        histograms.assertExpected();
    }

    /*
     * Test update when old WebAPK contains a custom name.
     */
    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testUpdateWithCustomName() throws Exception {
        CreationData creationData = defaultCreationData();
        creationData.name = "custom name";
        creationData.shortName = "custom short name";
        creationData.hasCustomName = true;
        creationData.shellVersion = -1;

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);

        Assert.assertTrue(checkUpdateNeeded(creationData, /* acceptDialogIfAppears= */ false));

        assertUpdateReasonsEqual(WebApkUpdateReason.OLD_SHELL_APK);
        Assert.assertFalse(mIconOrNameUpdateDialogShown);
    }
}
