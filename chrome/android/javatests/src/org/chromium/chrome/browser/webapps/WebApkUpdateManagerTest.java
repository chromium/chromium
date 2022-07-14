// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;

import androidx.annotation.IntDef;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
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
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.webapps.WebappTestPage;
import org.chromium.components.webapps.WebApkDistributor;
import org.chromium.components.webapps.WebApkUpdateReason;
import org.chromium.components.webapps.WebappsIconUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
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
public class WebApkUpdateManagerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @IntDef({EnabledFeature.NONE, EnabledFeature.NAME_UPDATES, EnabledFeature.ICON_UPDATES,
            EnabledFeature.NAME_AND_ICON_UPDATES})
    @Retention(RetentionPolicy.SOURCE)
    private @interface EnabledFeature {
        int NONE = 0;
        int NAME_UPDATES = 1;
        int ICON_UPDATES = 2;
        int NAME_AND_ICON_UPDATES = 3;
    }

    /**
     * The parameters for the App Identity tests (for which flag is enabled).
     */
    public static class FeatureResolveParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Arrays.asList(new ParameterSet().value(EnabledFeature.NONE).name("none"),
                    new ParameterSet().value(EnabledFeature.NAME_UPDATES).name("nameUpdates"),
                    new ParameterSet().value(EnabledFeature.ICON_UPDATES).name("iconUpdates"),
                    new ParameterSet()
                            .value(EnabledFeature.NAME_AND_ICON_UPDATES)
                            .name("nameAndIconUpdates"));
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
    private static final long WEBAPK_THEME_COLOR = 2147483648L;
    private static final long WEBAPK_BACKGROUND_COLOR = 2147483648L;

    private ChromeActivity mActivity;
    private Tab mTab;
    private EmbeddedTestServer mTestServer;

    private List<Integer> mLastUpdateReasons;

    // Whether the dialog, to warn about icons changing, should be shown.
    private boolean mAllowUpdateDialogForIcon;

    // Whether the dialog, to warn about names changing, should be shown.
    private boolean mAllowUpdateDialogForName;

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
        private boolean mAcceptDialogIfAppears;

        public TestWebApkUpdateManager(CallbackHelper waiter, ActivityTabProvider tabProvider,
                ActivityLifecycleDispatcher lifecycleDispatcher, boolean acceptDialogIfAppears) {
            super(tabProvider, lifecycleDispatcher);
            mWaiter = waiter;
            mLastUpdateReasons = new ArrayList<>();
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
        }

        @Override
        protected boolean iconUpdateDialogEnabled() {
            return mAllowUpdateDialogForIcon;
        }

        @Override
        protected boolean nameUpdateDialogEnabled() {
            return mAllowUpdateDialogForName;
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

        TestFetchStorageCallback callback = new TestFetchStorageCallback();
        WebappRegistry.getInstance().register(WEBAPK_ID, callback);
        callback.waitForCallback(0);
    }

     /** Checks whether a WebAPK update is needed. */
    private boolean checkUpdateNeeded(
            final CreationData creationData, boolean acceptDialogIfAppears) throws Exception {
        CallbackHelper waiter = new CallbackHelper();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TestWebApkUpdateManager updateManager =
                    new TestWebApkUpdateManager(waiter, mActivity.getActivityTabProvider(),
                            mActivity.getLifecycleDispatcher(), acceptDialogIfAppears);
            WebappDataStorage storage =
                    WebappRegistry.getInstance().getWebappDataStorage(WEBAPK_ID);
            BrowserServicesIntentDataProvider intentDataProvider =
                    WebApkIntentDataProviderFactory.create(new Intent(), "", creationData.scope,
                            null, null, creationData.name, creationData.shortName,
                            creationData.displayMode, creationData.orientation, 0,
                            creationData.themeColor, creationData.backgroundColor, 0,
                            creationData.isPrimaryIconMaskable, false /* isSplashIconMaskable */,
                            "", 1000 /* shellApkVersion */, creationData.manifestUrl,
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

    private void assertUpdateReasonsEqual(@WebApkUpdateReason Integer... reasons) {
        Assert.assertEquals(Arrays.asList(reasons), mLastUpdateReasons);
    }

    private void enableUpdateDialogForIcon(boolean enabled) {
        mAllowUpdateDialogForIcon = enabled;
    }

    private void enableUpdateDialogForName(boolean enabled) {
        mAllowUpdateDialogForName = enabled;
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
        final String maskableManifestUrl = "/chrome/test/data/banners/manifest_maskable.json";

        CreationData creationData = new CreationData();
        creationData.manifestUrl = mTestServer.getURL(maskableManifestUrl);
        creationData.startUrl =
                mTestServer.getURL("/chrome/test/data/banners/manifest_test_page.html");
        creationData.scope = mTestServer.getURL("/chrome/test/data/banners/");
        creationData.name = "Manifest test app";
        creationData.shortName = creationData.name;

        creationData.iconUrlToMurmur2HashMap = new HashMap<String, String>();
        creationData.iconUrlToMurmur2HashMap.put(
                mTestServer.getURL("/chrome/test/data/banners/launcher-icon-4x.png"),
                "8692598279279335241");
        creationData.iconUrlToMurmur2HashMap.put(
                mTestServer.getURL("/chrome/test/data/banners/launcher-icon-3x.png"),
                "16812314236514539104");
        creationData.displayMode = DisplayMode.STANDALONE;
        creationData.orientation = ScreenOrientationLockType.LANDSCAPE;
        creationData.themeColor = 2147483648L;
        creationData.backgroundColor = 2147483648L;
        creationData.isPrimaryIconMaskable = false;
        creationData.shortcuts = new ArrayList<>();

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, maskableManifestUrl);

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

    private void resolveFeatureParams(@EnabledFeature int enabledFeature) {
        enableUpdateDialogForName(enabledFeature == EnabledFeature.NAME_UPDATES
                || enabledFeature == EnabledFeature.NAME_AND_ICON_UPDATES);
        enableUpdateDialogForIcon(enabledFeature == EnabledFeature.ICON_UPDATES
                || enabledFeature == EnabledFeature.NAME_AND_ICON_UPDATES);
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    public void testMultipleUpdateReasons(@EnabledFeature int enabledFeature) throws Exception {
        resolveFeatureParams(enabledFeature);

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
                /* acceptDialogIfAppears= */ enabledFeature != EnabledFeature.NONE));

        switch (enabledFeature) {
            case EnabledFeature.NAME_UPDATES:
                assertUpdateReasonsEqual(WebApkUpdateReason.SHORT_NAME_DIFFERS,
                        WebApkUpdateReason.NAME_DIFFERS,
                        WebApkUpdateReason.BACKGROUND_COLOR_DIFFERS);
                break;
            case EnabledFeature.ICON_UPDATES:
                assertUpdateReasonsEqual(WebApkUpdateReason.PRIMARY_ICON_HASH_DIFFERS,
                        WebApkUpdateReason.SPLASH_ICON_HASH_DIFFERS,
                        WebApkUpdateReason.BACKGROUND_COLOR_DIFFERS);
                break;
            case EnabledFeature.NAME_AND_ICON_UPDATES:
                assertUpdateReasonsEqual(WebApkUpdateReason.PRIMARY_ICON_HASH_DIFFERS,
                        WebApkUpdateReason.SPLASH_ICON_HASH_DIFFERS,
                        WebApkUpdateReason.SHORT_NAME_DIFFERS, WebApkUpdateReason.NAME_DIFFERS,
                        WebApkUpdateReason.BACKGROUND_COLOR_DIFFERS);
                break;
            case EnabledFeature.NONE:
                assertUpdateReasonsEqual(WebApkUpdateReason.BACKGROUND_COLOR_DIFFERS);
                break;
        }
    }

    private void testAppIdentityChange(@EnabledFeature int enabledFeature, boolean changeName,
            boolean changeShortName, boolean changeIcon) throws Exception {
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

            expectIconChange = (enabledFeature == EnabledFeature.ICON_UPDATES)
                    || (enabledFeature == EnabledFeature.NAME_AND_ICON_UPDATES);
            if (expectIconChange) {
                expectedUpdateReasons.add(WebApkUpdateReason.PRIMARY_ICON_HASH_DIFFERS);
                expectedUpdateReasons.add(WebApkUpdateReason.SPLASH_ICON_HASH_DIFFERS);
            }
        }
        if (changeShortName) {
            creationData.shortName += "!";

            expectNameChange = (enabledFeature == EnabledFeature.NAME_UPDATES)
                    || (enabledFeature == EnabledFeature.NAME_AND_ICON_UPDATES);
            if (expectNameChange) {
                expectedUpdateReasons.add(WebApkUpdateReason.SHORT_NAME_DIFFERS);
            }
        }
        if (changeName) {
            creationData.name += "!";

            expectNameChange = (enabledFeature == EnabledFeature.NAME_UPDATES)
                    || (enabledFeature == EnabledFeature.NAME_AND_ICON_UPDATES);
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

        Assert.assertEquals(requestingChange && expectingChange, mIconOrNameUpdateDialogShown);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnNoChange(@EnabledFeature int enabledFeature) throws Exception {
        resolveFeatureParams(enabledFeature);

        testAppIdentityChange(enabledFeature,
                /* changeName= */ false,
                /* changeShortName= */ false,
                /* changeIcon = */ false);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnIconChange(@EnabledFeature int enabledFeature) throws Exception {
        resolveFeatureParams(enabledFeature);

        testAppIdentityChange(enabledFeature,
                /* changeName= */ false,
                /* changeShortName= */ false,
                /* changeIcon = */ true);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnShortnameChange(@EnabledFeature int enabledFeature)
            throws Exception {
        resolveFeatureParams(enabledFeature);
        testAppIdentityChange(enabledFeature,
                /* changeName= */ false,
                /* changeShortName= */ true,
                /* changeIcon = */ false);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnShortnameAndIconChange(@EnabledFeature int enabledFeature)
            throws Exception {
        resolveFeatureParams(enabledFeature);
        testAppIdentityChange(enabledFeature,
                /* changeName= */ false,
                /* changeShortName= */ true,
                /* changeIcon = */ true);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnNameChange(@EnabledFeature int enabledFeature) throws Exception {
        resolveFeatureParams(enabledFeature);
        testAppIdentityChange(enabledFeature,
                /* changeName= */ true,
                /* changeShortName= */ false,
                /* changeIcon = */ false);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnNameAndIconChange(@EnabledFeature int enabledFeature)
            throws Exception {
        resolveFeatureParams(enabledFeature);
        testAppIdentityChange(enabledFeature,
                /* changeName= */ true,
                /* changeShortName= */ false,
                /* changeIcon = */ true);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnNameAndShortnameChange(@EnabledFeature int enabledFeature)
            throws Exception {
        resolveFeatureParams(enabledFeature);
        testAppIdentityChange(enabledFeature,
                /* changeName= */ true,
                /* changeShortName= */ true,
                /* changeIcon = */ false);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureResolveParams.class)
    @Feature({"WebApk"})
    public void testUpdateWarningOnAllChange(@EnabledFeature int enabledFeature) throws Exception {
        resolveFeatureParams(enabledFeature);
        testAppIdentityChange(enabledFeature,
                /* changeName= */ true,
                /* changeShortName= */ true,
                /* changeIcon = */ true);
    }
}
