// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Bundle;
import android.text.TextUtils;

import org.json.JSONArray;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.android.util.concurrent.RoboExecutorService;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.PathUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.test.BackgroundShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.background_task_scheduler.ChromeBackgroundTaskFactory;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.browserservices.intents.WebApkShareTarget;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.browser.browserservices.intents.WebappIntentUtils;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.ShadowUrlUtilities;
import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.components.webapps.WebApkDistributor;
import org.chromium.components.webapps.WebApkInstallResult;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.util.ColorUtils;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.splash.SplashLayout;
import org.chromium.webapk.test.WebApkTestHelper;

import java.io.File;
import java.io.FileOutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Unit tests for WebApkUpdateManager. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowUrlUtilities.class, BackgroundShadowAsyncTask.class})
@LooperMode(LooperMode.Mode.LEGACY)
public class WebApkUpdateManagerUnitTest {
    @Mock private Activity mActivityMock;

    @Rule public FakeTimeTestRule mClockRule = new FakeTimeTestRule();

    @Rule public JniMocker mJniMocker = new JniMocker();

    private static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.test_package";
    private static final String UNBOUND_WEBAPK_PACKAGE_NAME = "com.webapk.test_package";

    private static final int REQUEST_UPDATE_FOR_SHELL_APK_VERSION = 100;

    /** Web Manifest URL */
    private static final String WEB_MANIFEST_URL = "manifest.json";

    private static final String START_URL = "/start_url.html";
    private static final String SCOPE_URL = "/";
    private static final String NAME = "Long Name";
    private static final String SHORT_NAME = "Short Name";
    private static final String MANIFEST_ID = "manifestId";
    private static final String PRIMARY_ICON_URL = "/icon.png";
    private static final String PRIMARY_ICON_MURMUR2_HASH = "3";
    private static final @DisplayMode.EnumType int DISPLAY_MODE = DisplayMode.UNDEFINED;
    private static final int ORIENTATION = ScreenOrientationLockType.DEFAULT;
    private static final long THEME_COLOR = 1L;
    private static final long BACKGROUND_COLOR = 2L;
    private static final long DARK_THEME_COLOR = 3L;
    private static final long DARK_BACKGROUND_COLOR = 4L;
    private static final int DEFAULT_BACKGROUND_COLOR = 3;
    private static final String SHARE_TARGET_ACTION = "/share_action.html";
    private static final String SHARE_TARGET_PARAM_TITLE = "share_params_title";
    private static final String SHARE_TARGET_METHOD_GET = "GET";
    private static final String SHARE_TARGET_METHOD_POST = "POST";
    private static final String SHARE_TARGET_ENC_TYPE_MULTIPART = "multipart/form-data";
    private static final String[] SHARE_TARGET_FILE_NAMES = new String[] {"file_1", "file_2"};
    private static final String[][] SHARE_TARGET_ACCEPTS =
            new String[][] {
                new String[] {"file_1_accept_1", "file_1_accept_2"},
                new String[] {"file_2_accept_2", "file_2_accept_2"}
            };

    /** Different values than the ones used in {@link defaultManifestData()}. */
    private static final String DIFFERENT_NAME = "Different Name";

    private static final int DIFFERENT_BACKGROUND_COLOR = 42;

    /** The histograms involved in showing the App Identity update dialog. */
    private static final String HISTOGRAM_NOT_SHOWING = "Webapp.AppIdentityDialog.NotShowing";

    private static final String HISTOGRAM_SHOWING = "Webapp.AppIdentityDialog.Showing";
    private static final String HISTOGRAM_PRE_APPROVED = "Webapp.AppIdentityDialog.AlreadyApproved";

    /** Mock {@link WebApkUpdateDataFetcher}. */
    private static class TestWebApkUpdateDataFetcher extends WebApkUpdateDataFetcher {
        private boolean mStarted;

        public boolean wasStarted() {
            return mStarted;
        }

        @Override
        public boolean start(Tab tab, WebappInfo oldInfo, Observer observer) {
            mStarted = true;
            return true;
        }
    }

    private static class TestWebApkUpdateManagerJni implements WebApkUpdateManager.Natives {
        private static WebApkUpdateManager.WebApkUpdateCallback sUpdateCallback;

        public static WebApkUpdateManager.WebApkUpdateCallback getUpdateCallback() {
            return sUpdateCallback;
        }

        @Override
        public void storeWebApkUpdateRequestToFile(
                String updateRequestPath,
                String startUrl,
                String scope,
                String name,
                String shortName,
                boolean hasCustomName,
                String manifestId,
                String appKey,
                String primaryIconUrl,
                byte[] primaryIconData,
                boolean isPrimaryIconMaskable,
                String splashIconUrl,
                byte[] splashIconData,
                boolean isSplashIconMaskable,
                String[] iconUrls,
                String[] iconHashes,
                @DisplayMode.EnumType int displayMode,
                int orientation,
                long themeColor,
                long backgroundColor,
                long darkThemeColor,
                long darkBackgroundColor,
                String shareTargetAction,
                String shareTargetParamTitle,
                String shareTargetParamText,
                boolean shareTargetParamIsMethodPost,
                boolean shareTargetParamIsEncTypeMultipart,
                String[] shareTargetParamFileNames,
                Object[] shareTargetParamAccepts,
                String[][] shortcuts,
                byte[][] shortcutIconData,
                String manifestUrl,
                String webApkPackage,
                int webApkVersion,
                boolean isManifestStale,
                boolean isAppIdentityUpdateSupported,
                int[] updateReasons,
                Callback<Boolean> callback) {}

        @Override
        public void updateWebApkFromFile(
                String updateRequestPath, WebApkUpdateManager.WebApkUpdateCallback callback) {
            sUpdateCallback = callback;
        }

        @Override
        public int getWebApkTargetShellVersion() {
            return REQUEST_UPDATE_FOR_SHELL_APK_VERSION;
        }
    }

    private static class TestWebApkUpdateManager extends WebApkUpdateManager {
        private Callback<Boolean> mStoreUpdateRequestCallback;
        private TestWebApkUpdateDataFetcher mFetcher;
        private String mUpdateName;
        private String mAppKey;
        private boolean mDestroyedFetcher;

        /**
         * Whether App Identity updates should be enabled. If either of those is true when the tests
         * run, all App Identity update dialogs will be pre-approved (without showing).
         */
        private boolean mNameUpdatesEnabled;

        private boolean mIconUpdatesEnabled;

        public TestWebApkUpdateManager(Activity activity) {
            this(activity, /* nameUpdatesEnabled= */ false, /* iconUpdatesEnabled= */ false);
        }

        public TestWebApkUpdateManager(
                Activity activity, boolean nameUpdatesEnabled, boolean iconUpdatesEnabled) {
            this(activity, buildMockTabProvider(), Mockito.mock(ActivityLifecycleDispatcher.class));
            mNameUpdatesEnabled = nameUpdatesEnabled;
            mIconUpdatesEnabled = iconUpdatesEnabled;
        }

        private static ActivityTabProvider buildMockTabProvider() {
            Tab mockTab = Mockito.mock(Tab.class);
            ActivityTabProvider tabProvider = Mockito.mock(ActivityTabProvider.class);
            Mockito.when(tabProvider.get()).thenReturn(mockTab);
            return tabProvider;
        }

        private TestWebApkUpdateManager(
                Activity activity,
                ActivityTabProvider tabProvider,
                ActivityLifecycleDispatcher activityLifecycleDispatcher) {
            super(activity, tabProvider, activityLifecycleDispatcher);
        }

        /** Returns whether the is-update-needed check has been triggered. */
        public boolean updateCheckStarted() {
            return mFetcher != null && mFetcher.wasStarted();
        }

        /** Returns whether an update has been requested. */
        public boolean updateRequested() {
            return mStoreUpdateRequestCallback != null;
        }

        /** Returns the "name" from the requested update. Null if an update has not been requested. */
        public String requestedUpdateName() {
            return mUpdateName;
        }

        /**
         * Returns the "app_key" from the requested update. Null if an update has not been
         * requested.
         */
        public String requestedAppKey() {
            return mAppKey;
        }

        public boolean destroyedFetcher() {
            return mDestroyedFetcher;
        }

        public Callback<Boolean> getStoreUpdateRequestCallback() {
            return mStoreUpdateRequestCallback;
        }

        @Override
        protected boolean iconUpdateDialogEnabled() {
            return mIconUpdatesEnabled;
        }

        @Override
        protected boolean nameUpdateDialogEnabled() {
            return mNameUpdatesEnabled;
        }

        @Override
        protected void showIconOrNameUpdateDialog(
                boolean iconChanging, boolean shortNameChanging, boolean nameChanging) {
            // This function is overridden because the parent class can't show the dialog (since
            // WindowAndroid is null in this test) so there not much to do besides auto-approving
            // the update (if the change is expected).
            boolean expectNameChange = mNameUpdatesEnabled && (shortNameChanging || nameChanging);
            boolean expectIconChange = mIconUpdatesEnabled && iconChanging;

            super.onUserApprovedUpdate(
                    expectNameChange || expectIconChange
                            ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                            : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
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
            storeWebApkUpdateRequestToFile(
                    updateRequestPath,
                    info,
                    primaryIconUrl,
                    new byte[] {},
                    splashIconUrl,
                    new byte[] {},
                    isManifestStale,
                    isAppIdentityUpdateSupported,
                    updateReasons,
                    callback);
        }

        @Override
        protected WebApkUpdateDataFetcher buildFetcher() {
            mFetcher = new TestWebApkUpdateDataFetcher();
            return mFetcher;
        }

        @Override
        protected void storeWebApkUpdateRequestToFile(
                String updateRequestPath,
                WebappInfo info,
                String primaryIconUrl,
                byte[] primaryIconData,
                String splashIconUrl,
                byte[] splashIconData,
                boolean isManifestStale,
                boolean isAppIdentityUpdateSupported,
                List<Integer> updateReasons,
                Callback<Boolean> callback) {
            mStoreUpdateRequestCallback = callback;
            mUpdateName = info.name();
            mAppKey = info.appKey();
            writeRandomTextToFile(updateRequestPath);
        }

        @Override
        protected void destroyFetcher() {
            mFetcher = null;
            mDestroyedFetcher = true;
        }
    }

    private static class ManifestData {
        public String startUrl;
        public String scopeUrl;
        public String name;
        public String shortName;
        public boolean hasCustomName;
        public String id;
        public String appKey;
        public Map<String, String> iconUrlToMurmur2HashMap;
        public String primaryIconUrl;
        public Bitmap primaryIcon;
        public @DisplayMode.EnumType int displayMode;
        public int orientation;
        public long themeColor;
        public long backgroundColor;
        public long darkThemeColor;
        public long darkBackgroundColor;
        public int defaultBackgroundColor;
        public String shareTargetAction;
        public String shareTargetParamTitle;
        public String shareTargetMethod;
        public String shareTargetEncType;
        public String[] shareTargetFileNames;
        public String[][] shareTargetFileAccepts;

        // Injected directly into the resulting WebAPKInfo.
        public List<WebApkExtras.ShortcutItem> shortcuts = new ArrayList<>();
    }

    private static class FakeDefaultBackgroundColorResource extends Resources {
        private static final int ID = 10;
        private int mColorValue;

        public FakeDefaultBackgroundColorResource(int colorValue) {
            super(new AssetManager(), null, null);
            mColorValue = colorValue;
        }

        @Override
        public int getColor(int id, Resources.Theme theme) {
            if (id != ID) {
                throw new Resources.NotFoundException("id 0x" + Integer.toHexString(id));
            }
            return mColorValue;
        }
    }

    private void registerStorageForWebApkPackage(String webApkPackageName) throws Exception {
        CallbackHelper helper = new CallbackHelper();
        WebappRegistry.getInstance()
                .register(
                        WebappIntentUtils.getIdForWebApkPackage(webApkPackageName),
                        new WebappRegistry.FetchWebappDataStorageCallback() {
                            @Override
                            public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
                                helper.notifyCalled();
                            }
                        });
            BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        helper.waitForOnly();
    }

    private static WebappDataStorage getStorage(String packageName) {
        return WebappRegistry.getInstance()
                .getWebappDataStorage(WebappIntentUtils.getIdForWebApkPackage(packageName));
    }

    /**
     * Create a WebAPK metadata bundle from ManifestData.
     *
     * @param manifestData <meta-data> values for WebAPK's Android Manifest.
     * @param shellApkVersionCode WebAPK's version of the //chrome/android/webapk/shell_apk code.
     */
    private Bundle createWebApkMetadata(ManifestData manifestData, int shellApkVersionCode) {
        Bundle metaData = new Bundle();
        metaData.putInt(WebApkMetaDataKeys.SHELL_APK_VERSION, shellApkVersionCode);
        metaData.putString(WebApkMetaDataKeys.START_URL, manifestData.startUrl);
        metaData.putString(WebApkMetaDataKeys.SCOPE, manifestData.scopeUrl);
        metaData.putString(WebApkMetaDataKeys.NAME, manifestData.name);
        metaData.putString(WebApkMetaDataKeys.SHORT_NAME, manifestData.shortName);
        metaData.putString(WebApkMetaDataKeys.THEME_COLOR, manifestData.themeColor + "L");
        metaData.putString(WebApkMetaDataKeys.BACKGROUND_COLOR, manifestData.backgroundColor + "L");
        metaData.putString(WebApkMetaDataKeys.DARK_THEME_COLOR, manifestData.darkThemeColor + "L");
        metaData.putString(
                WebApkMetaDataKeys.DARK_BACKGROUND_COLOR, manifestData.darkBackgroundColor + "L");
        metaData.putInt(
                WebApkMetaDataKeys.DEFAULT_BACKGROUND_COLOR_ID,
                FakeDefaultBackgroundColorResource.ID);
        metaData.putString(WebApkMetaDataKeys.WEB_MANIFEST_URL, WEB_MANIFEST_URL);
        metaData.putString(WebApkMetaDataKeys.WEB_MANIFEST_ID, manifestData.id);
        metaData.putString(WebApkMetaDataKeys.APP_KEY, manifestData.appKey);

        String iconUrlsAndIconMurmur2Hashes = "";
        for (Map.Entry<String, String> mapEntry : manifestData.iconUrlToMurmur2HashMap.entrySet()) {
            String murmur2Hash = mapEntry.getValue();
            if (murmur2Hash == null) {
                murmur2Hash = "0";
            }
            iconUrlsAndIconMurmur2Hashes += " " + mapEntry.getKey() + " " + murmur2Hash;
        }
        iconUrlsAndIconMurmur2Hashes = iconUrlsAndIconMurmur2Hashes.trim();
        metaData.putString(
                WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES, iconUrlsAndIconMurmur2Hashes);
        return metaData;
    }

    /**
     * Registers WebAPK with default package name. Overwrites previous registrations.
     *
     * @param packageName Package name for which to register the WebApk.
     * @param manifestData <meta-data> values for WebAPK's Android Manifest.
     * @param shellApkVersionCode WebAPK's version of the //chrome/android/webapk/shell_apk code.
     */
    private void registerWebApk(
            String packageName, ManifestData manifestData, int shellApkVersionCode) {
        Bundle metadata = createWebApkMetadata(manifestData, shellApkVersionCode);
        Bundle shareTargetMetaData = new Bundle();
        shareTargetMetaData.putString(
                WebApkMetaDataKeys.SHARE_ACTION, manifestData.shareTargetAction);
        shareTargetMetaData.putString(
                WebApkMetaDataKeys.SHARE_PARAM_TITLE, manifestData.shareTargetParamTitle);

        shareTargetMetaData.putString(
                WebApkMetaDataKeys.SHARE_METHOD, manifestData.shareTargetMethod);
        shareTargetMetaData.putString(
                WebApkMetaDataKeys.SHARE_ENCTYPE, manifestData.shareTargetEncType);

        shareTargetMetaData.remove(WebApkMetaDataKeys.SHARE_PARAM_NAMES);
        if (manifestData.shareTargetFileNames != null) {
            JSONArray fileNamesJson =
                    new JSONArray(Arrays.asList(manifestData.shareTargetFileNames));
            shareTargetMetaData.putString(
                    WebApkMetaDataKeys.SHARE_PARAM_NAMES, fileNamesJson.toString());
        }

        shareTargetMetaData.remove(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS);
        if (manifestData.shareTargetFileAccepts != null) {
            JSONArray acceptJson = new JSONArray();

            for (String[] acceptArr : manifestData.shareTargetFileAccepts) {
                acceptJson.put(new JSONArray(Arrays.asList(acceptArr)));
            }
            shareTargetMetaData.putString(
                    WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, acceptJson.toString());
        }

        WebApkTestHelper.registerWebApkWithMetaData(
                packageName, metadata, new Bundle[] {shareTargetMetaData});
        WebApkTestHelper.setResource(
                packageName,
                new FakeDefaultBackgroundColorResource(manifestData.defaultBackgroundColor));
    }

    private static ManifestData defaultManifestData() {
        ManifestData manifestData = new ManifestData();
        manifestData.startUrl = START_URL;
        manifestData.scopeUrl = SCOPE_URL;
        manifestData.name = NAME;
        manifestData.shortName = SHORT_NAME;
        manifestData.hasCustomName = false;
        manifestData.id = MANIFEST_ID;
        manifestData.appKey = MANIFEST_ID;

        manifestData.iconUrlToMurmur2HashMap = new HashMap<>();
        manifestData.iconUrlToMurmur2HashMap.put(PRIMARY_ICON_URL, PRIMARY_ICON_MURMUR2_HASH);

        manifestData.primaryIconUrl = PRIMARY_ICON_URL;
        manifestData.primaryIcon = createBitmap(Color.GREEN);
        manifestData.displayMode = DISPLAY_MODE;
        manifestData.orientation = ORIENTATION;
        manifestData.themeColor = THEME_COLOR;
        manifestData.backgroundColor = BACKGROUND_COLOR;
        manifestData.darkThemeColor = DARK_THEME_COLOR;
        manifestData.darkBackgroundColor = DARK_BACKGROUND_COLOR;
        manifestData.defaultBackgroundColor = DEFAULT_BACKGROUND_COLOR;
        manifestData.shareTargetAction = SHARE_TARGET_ACTION;
        manifestData.shareTargetParamTitle = SHARE_TARGET_PARAM_TITLE;

        manifestData.shareTargetMethod = SHARE_TARGET_METHOD_GET;
        manifestData.shareTargetEncType = SHARE_TARGET_ENC_TYPE_MULTIPART;
        manifestData.shareTargetFileNames = SHARE_TARGET_FILE_NAMES.clone();
        manifestData.shareTargetFileAccepts =
                Arrays.stream(SHARE_TARGET_ACCEPTS)
                        .map(strings -> strings.clone())
                        .toArray(i -> new String[i][]);
        manifestData.shortcuts = new ArrayList<>();
        return manifestData;
    }

    private static BrowserServicesIntentDataProvider intentDataProviderFromManifestData(
            ManifestData manifestData) {
        if (manifestData == null) return null;

        final String kPackageName = "org.random.webapk";
        WebApkShareTarget shareTarget =
                TextUtils.isEmpty(manifestData.shareTargetAction)
                        ? null
                        : new WebApkShareTarget(
                                manifestData.shareTargetAction,
                                manifestData.shareTargetParamTitle,
                                null,
                                manifestData.shareTargetMethod != null
                                        && manifestData.shareTargetMethod.equals(
                                                SHARE_TARGET_METHOD_POST),
                                manifestData.shareTargetEncType != null
                                        && manifestData.shareTargetEncType.equals(
                                                SHARE_TARGET_ENC_TYPE_MULTIPART),
                                manifestData.shareTargetFileNames,
                                manifestData.shareTargetFileAccepts);
        return WebApkIntentDataProviderFactory.create(
                new Intent(),
                "",
                manifestData.scopeUrl,
                new WebappIcon(manifestData.primaryIcon),
                null,
                manifestData.name,
                manifestData.shortName,
                manifestData.hasCustomName,
                manifestData.displayMode,
                manifestData.orientation,
                -1,
                manifestData.themeColor,
                manifestData.backgroundColor,
                manifestData.darkThemeColor,
                manifestData.darkBackgroundColor,
                manifestData.defaultBackgroundColor,
                /* isPrimaryIconMaskable= */ false,
                /* isSplashIconMaskable= */ false,
                kPackageName,
                -1,
                WEB_MANIFEST_URL,
                manifestData.startUrl,
                manifestData.id,
                manifestData.appKey,
                WebApkDistributor.BROWSER,
                manifestData.iconUrlToMurmur2HashMap,
                shareTarget,
                /* forceNavigation= */ false,
                /* isSplashProvidedByWebApk= */ false,
                /* shareData= */ null,
                /* shortcutItems= */ manifestData.shortcuts,
                /* webApkVersionCode= */ 1,
                /* lastUpdateTime= */ TimeUtils.currentTimeMillis());
    }

    /**
     * Creates 1x1 bitmap.
     * @param color The bitmap color.
     */
    private static Bitmap createBitmap(int color) {
        int[] colors = {color};
        return Bitmap.createBitmap(colors, 1, 1, Bitmap.Config.ALPHA_8);
    }

    private static void updateIfNeeded(
            String packageName,
            WebApkUpdateManager updateManager,
            List<WebApkExtras.ShortcutItem> shortcuts) {
        // Use the intent version of {@link WebApkInfo#create()} in order to test default values
        // set by the intent version of {@link WebApkInfo#create()}.
        Intent intent = new Intent();
        intent.putExtra(WebappConstants.EXTRA_URL, "");
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, packageName);
        BrowserServicesIntentDataProvider intentDataProvider =
                WebApkIntentDataProviderFactory.create(intent);
        intentDataProvider.getWebApkExtras().shortcutItems.clear();
        intentDataProvider.getWebApkExtras().shortcutItems.addAll(shortcuts);

        updateManager.updateIfNeeded(getStorage(packageName), intentDataProvider);
    }

    private static void updateIfNeeded(String packageName, WebApkUpdateManager updateManager) {
        updateIfNeeded(packageName, updateManager, new ArrayList<>());
    }

    private static void onGotUnchangedWebManifestData(WebApkUpdateManager updateManager) {
        onGotManifestData(updateManager, defaultManifestData());
    }

    private static void onGotDifferentData(WebApkUpdateManager updateManager) {
        ManifestData manifestData = defaultManifestData();
        // Note: Avoid using name/icon changes just to trigger updates, as there are special
        // considerations involved in updating them (App Identity changes).
        manifestData.backgroundColor = DIFFERENT_BACKGROUND_COLOR;
        onGotManifestData(updateManager, manifestData);
    }

    private static void onGotManifestData(
            WebApkUpdateManager updateManager, ManifestData fetchedManifestData) {
        String primaryIconUrl = randomIconUrl(fetchedManifestData);
        String splashIconUrl = randomIconUrl(fetchedManifestData);
        updateManager.onGotManifestData(
                intentDataProviderFromManifestData(fetchedManifestData),
                primaryIconUrl,
                splashIconUrl);
    }

    /**
     * Tries to complete update request.
     *
     * @param result The result of the update task. Emulates the proto creation as always
     *     succeeding.
     */
    private static void tryCompletingUpdate(
            TestWebApkUpdateManager updateManager,
            WebappDataStorage storage,
            @WebApkInstallResult int result) {
        // Emulate proto creation as always succeeding.
        Callback<Boolean> storeUpdateRequestCallback =
                updateManager.getStoreUpdateRequestCallback();
        if (storeUpdateRequestCallback == null) return;

        storeUpdateRequestCallback.onResult(true);

        WebApkUpdateManager.updateWhileNotRunning(storage, Mockito.mock(Runnable.class));
        WebApkUpdateManager.WebApkUpdateCallback updateCallback =
                TestWebApkUpdateManagerJni.getUpdateCallback();
        if (updateCallback == null) return;

        updateCallback.onResultFromNative(result, /* relaxUpdates= */ false);
    }

    private static void writeRandomTextToFile(String path) {
        File file = new File(path);
        new File(file.getParent()).mkdirs();
        try (FileOutputStream out = new FileOutputStream(file)) {
            out.write(ApiCompatibilityUtils.getBytesUtf8("something"));
        } catch (Exception e) {
        }
    }

    private static String randomIconUrl(ManifestData fetchedManifestData) {
        if (fetchedManifestData == null || fetchedManifestData.iconUrlToMurmur2HashMap.isEmpty()) {
            return null;
        }
        return fetchedManifestData.iconUrlToMurmur2HashMap.keySet().iterator().next();
    }

    private boolean checkUpdateNeededForFetchedManifest(
            ManifestData androidManifestData, ManifestData fetchedManifestData) {
        return checkUpdateNeededForFetchedManifest(
                androidManifestData,
                fetchedManifestData,
                /* nameUpdatesEnabled= */ false,
                /* iconUpdatesEnabled= */ false);
    }

    /**
     * Checks whether the WebAPK is updated given data from the WebAPK's Android Manifest and data
     * from the fetched Web Manifest.
     */
    private boolean checkUpdateNeededForFetchedManifest(
            ManifestData androidManifestData,
            ManifestData fetchedManifestData,
            boolean nameUpdatesEnabled,
            boolean iconUpdatesEnabled) {
        registerWebApk(
                WEBAPK_PACKAGE_NAME, androidManifestData, REQUEST_UPDATE_FOR_SHELL_APK_VERSION);
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager =
                new TestWebApkUpdateManager(mActivityMock, nameUpdatesEnabled, iconUpdatesEnabled);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager, androidManifestData.shortcuts);
        assertTrue(updateManager.updateCheckStarted());
        updateManager.onGotManifestData(
                intentDataProviderFromManifestData(fetchedManifestData),
                fetchedManifestData.primaryIconUrl,
                null);
        return updateManager.updateRequested();
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        PathUtils.setPrivateDataDirectorySuffix("chrome");
        PostTask.setPrenativeThreadPoolExecutorForTesting(new RoboExecutorService());

        mJniMocker.mock(WebApkUpdateManagerJni.TEST_HOOKS, new TestWebApkUpdateManagerJni());

        WebappRegistry.refreshSharedPrefsForTesting();
        registerWebApk(
                WEBAPK_PACKAGE_NAME, defaultManifestData(), REQUEST_UPDATE_FOR_SHELL_APK_VERSION);
        registerStorageForWebApkPackage(WEBAPK_PACKAGE_NAME);

        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        storage.updateTimeOfLastCheckForUpdatedWebManifest();
        storage.updateTimeOfLastWebApkUpdateRequestCompletion();
        storage.updateDidLastWebApkUpdateRequestSucceed(true);

        ChromeBackgroundTaskFactory.setAsDefault();
    }

    /**
     * Test that the is-update-needed check is tried the next time that the WebAPK is launched if
     * Chrome is killed prior to the initial URL finishing loading.
     */
    @Test
    public void testCheckOnNextLaunchIfClosePriorToFirstPageLoad() {
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);
        {
            TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
            updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
            assertTrue(updateManager.updateCheckStarted());
        }

        // Chrome is killed. {@link WebApkUpdateManager#OnGotManifestData()} is not called.

        {
            // Relaunching the WebAPK should do an is-update-needed check.
            TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
            updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
            assertTrue(updateManager.updateCheckStarted());
            onGotUnchangedWebManifestData(updateManager);
        }

        {
            // Relaunching the WebAPK should not do an is-update-needed-check.
            TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
            updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
            assertFalse(updateManager.updateCheckStarted());
        }
    }

    /**
     * Test that the completion time of the previous WebAPK update is not modified if:
     * - The previous WebAPK update succeeded.
     * AND
     * - A WebAPK update is not required.
     */
    @Test
    public void testUpdateNotNeeded() {
        long initialTime = TimeUtils.currentTimeMillis();
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotUnchangedWebManifestData(updateManager);
        assertFalse(updateManager.updateRequested());

        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        assertTrue(storage.getDidLastWebApkUpdateRequestSucceed());
        assertEquals(initialTime, storage.getLastWebApkUpdateRequestCompletionTimeMs());
    }

    /**
     * Test that the last WebAPK update is marked as having succeeded if:
     * - The previous WebAPK update failed.
     * AND
     * - A WebAPK update is no longer required.
     */
    @Test
    public void testMarkUpdateAsSucceededIfUpdateNoLongerNeeded() {
        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        storage.updateDidLastWebApkUpdateRequestSucceed(false);
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotUnchangedWebManifestData(updateManager);
        assertFalse(updateManager.updateRequested());

        assertTrue(storage.getDidLastWebApkUpdateRequestSucceed());
        assertEquals(
                TimeUtils.currentTimeMillis(),
                storage.getLastWebApkUpdateRequestCompletionTimeMs());
    }

    /**
     * Test that the WebAPK update is marked as having failed if Chrome is killed prior to the
     * WebAPK update completing.
     */
    @Test
    public void testMarkUpdateAsFailedIfClosePriorToUpdateCompleting() {
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotDifferentData(updateManager);
        assertTrue(updateManager.updateRequested());

        // Chrome is killed. {@link WebApkUpdateCallback#onResultFromNative} is never called.

        // Check {@link WebappDataStorage} state.
        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        assertFalse(storage.getDidLastWebApkUpdateRequestSucceed());
        assertEquals(
                TimeUtils.currentTimeMillis(),
                storage.getLastWebApkUpdateRequestCompletionTimeMs());
    }

    /**
     * Test that the pending update file is deleted after update completes regardless of whether
     * update succeeded.
     */
    @Test
    public void testPendingUpdateFileDeletedAfterUpdateCompletion() {
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);

        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);

        onGotDifferentData(updateManager);
        assertTrue(updateManager.updateRequested());
        String updateRequestPath = storage.getPendingUpdateRequestPath();
        assertNotNull(updateRequestPath);
        assertTrue(new File(updateRequestPath).exists());

        tryCompletingUpdate(updateManager, storage, WebApkInstallResult.FAILURE);

        assertNull(storage.getPendingUpdateRequestPath());
        assertFalse(new File(updateRequestPath).exists());
    }

    /**
     * Test that the pending update file is deleted if
     * {@link WebApkUpdateManager#nativeStoreWebApkUpdateRequestToFile} creates the pending update
     * file but fails.
     */
    @Test
    public void testFileDeletedIfStoreWebApkUpdateRequestToFileFails() {
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);

        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);

        onGotDifferentData(updateManager);
        assertTrue(updateManager.updateRequested());
        String updateRequestPath = storage.getPendingUpdateRequestPath();
        assertNotNull(updateRequestPath);
        assertTrue(new File(updateRequestPath).exists());

        updateManager.getStoreUpdateRequestCallback().onResult(false);

        assertNull(storage.getPendingUpdateRequestPath());
        assertFalse(new File(updateRequestPath).exists());
    }

    /**
     * Test that an update with data from the WebAPK's Android manifest is done if:
     * - WebAPK's code is out of date
     * AND
     * - WebAPK's start_url does not refer to a Web Manifest.
     *
     * It is good to minimize the number of users with out of date WebAPKs. We try to keep WebAPKs
     * up to date even if the web developer has removed the Web Manifest from their site.
     */
    @Test
    public void testShellApkOutOfDateNoWebManifest() {
        registerWebApk(
                WEBAPK_PACKAGE_NAME,
                defaultManifestData(),
                REQUEST_UPDATE_FOR_SHELL_APK_VERSION - 1);
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTrue(updateManager.updateRequested());
        assertEquals(NAME, updateManager.requestedUpdateName());
        assertEquals(MANIFEST_ID, updateManager.requestedAppKey());

        // Check that the {@link WebApkUpdateDataFetcher} has been destroyed. This prevents
        // {@link #onGotManifestData()} from getting called.
        assertTrue(updateManager.destroyedFetcher());
    }

    /**
     * Test that an update is not done if:
     * - WebAPK's code is out of date
     * AND
     * - WebApkUpdateManager has been destroyed.
     */
    @Test
    public void testDontRequestUpdateAfterManagerDestroyed() {
        registerWebApk(
                WEBAPK_PACKAGE_NAME,
                defaultManifestData(),
                REQUEST_UPDATE_FOR_SHELL_APK_VERSION - 1);
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());

        updateManager.onDestroy();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertFalse(updateManager.updateRequested());
    }

    /**
     * Test that an update with data from the fetched Web Manifest is done if the WebAPK's code is
     * out of date and the WebAPK's start_url refers to a Web Manifest.
     */
    @Test
    public void testShellApkOutOfDateStillHasWebManifest() {
        registerWebApk(
                WEBAPK_PACKAGE_NAME,
                defaultManifestData(),
                REQUEST_UPDATE_FOR_SHELL_APK_VERSION - 1);
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());

        onGotManifestData(updateManager, defaultManifestData());
        assertTrue(updateManager.updateRequested());
        assertEquals(NAME, updateManager.requestedUpdateName());
        assertEquals(MANIFEST_ID, updateManager.requestedAppKey());

        assertTrue(updateManager.destroyedFetcher());
    }

    /**
     * Test that an update is requested if:
     * - start_url does not refer to a Web Manifest.
     * AND
     * - The user eventually navigates to a page pointing to a Web Manifest with the correct URL.
     * AND
     * - The Web Manifest has changed.
     */
    @Test
    public void testStartUrlRedirectsToPageWithUpdatedWebManifest() {
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager =
                new TestWebApkUpdateManager(
                        mActivityMock,
                        /* nameUpdatesEnabled= */ true,
                        /* iconUpdatesEnabled= */ false);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());

        // start_url does not have a Web Manifest. {@link #onGotManifestData} is called as a result
        // of update manager timing out. No update should be requested.
        updateManager.onGotManifestData(null, null, null);
        assertFalse(updateManager.updateRequested());
        // {@link WebApkUpdateDataFetcher} should still be alive so that it can get
        // {@link #onGotManifestData} when page with the Web Manifest finishes loading.
        assertFalse(updateManager.destroyedFetcher());

        // User eventually navigates to page with Web Manifest.

        ManifestData manifestData = defaultManifestData();
        manifestData.name = DIFFERENT_NAME;
        onGotManifestData(updateManager, manifestData);
        assertTrue(updateManager.updateRequested());
        assertEquals(DIFFERENT_NAME, updateManager.requestedUpdateName());

        assertTrue(updateManager.destroyedFetcher());
    }

    /**
     * Test that an update is not requested if:
     * - start_url does not refer to a Web Manifest.
     * AND
     * - The user eventually navigates to a page pointing to a Web Manifest with the correct URL.
     * AND
     * - The Web Manifest has not changed.
     */
    @Test
    public void testStartUrlRedirectsToPageWithUnchangedWebManifest() {
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);

        // Update manager times out.
        updateManager.onGotManifestData(null, null, null);
        onGotManifestData(updateManager, defaultManifestData());
        assertFalse(updateManager.updateRequested());

        // We got the Web Manifest. The {@link WebApkUpdateDataFetcher} should be destroyed to stop
        // it from fetching the Web Manifest for subsequent page loads.
        assertTrue(updateManager.destroyedFetcher());
    }

    @Test
    public void testManifestDoesNotUpgrade() {
        assertFalse(
                checkUpdateNeededForFetchedManifest(defaultManifestData(), defaultManifestData()));
    }

    /** Test that a webapk with an unexpected package name does not request updates. */
    @Test
    public void testUnboundWebApkDoesNotUpgrade() {
        ManifestData androidManifestData = defaultManifestData();

        registerWebApk(
                UNBOUND_WEBAPK_PACKAGE_NAME,
                androidManifestData,
                REQUEST_UPDATE_FOR_SHELL_APK_VERSION);
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(UNBOUND_WEBAPK_PACKAGE_NAME, updateManager);
        assertFalse(updateManager.updateCheckStarted());
        assertFalse(updateManager.updateRequested());
    }

    /**
     * Test that an upgrade is requested when the share target specified in the Web Manifest
     * changes.
     */
    @Test
    public void testShareTargetChangedShouldUpgrade() {
        ManifestData oldData = defaultManifestData();
        ManifestData fetchedData = defaultManifestData();
        fetchedData.shareTargetAction = "/action2.html";
        assertTrue(checkUpdateNeededForFetchedManifest(oldData, fetchedData));
    }

    @Test
    public void testShareTargetV2ChangedShouldUpgrade() {
        ManifestData oldData = defaultManifestData();

        ManifestData fetchedData1 = defaultManifestData();
        fetchedData1.shareTargetFileNames[0] = "changed";
        assertTrue(checkUpdateNeededForFetchedManifest(oldData, fetchedData1));

        ManifestData fetchedData2 = defaultManifestData();
        fetchedData2.shareTargetFileAccepts[1] = new String[] {};
        assertTrue(checkUpdateNeededForFetchedManifest(oldData, fetchedData2));

        ManifestData fetchedData3 = defaultManifestData();
        fetchedData3.shareTargetFileAccepts[1][1] = "changed";
        assertTrue(checkUpdateNeededForFetchedManifest(oldData, fetchedData3));
    }

    @Test
    public void testShareTargetV2UpgradeFromV1() {
        ManifestData oldNoShareTarget = defaultManifestData();
        oldNoShareTarget.shareTargetAction = null;
        oldNoShareTarget.shareTargetParamTitle = null;
        oldNoShareTarget.shareTargetMethod = null;
        oldNoShareTarget.shareTargetEncType = null;
        oldNoShareTarget.shareTargetFileNames = null;
        oldNoShareTarget.shareTargetFileAccepts = null;

        ManifestData fetchedNoShareTarget2 = defaultManifestData();
        fetchedNoShareTarget2.shareTargetAction = null;
        fetchedNoShareTarget2.shareTargetParamTitle = null;
        fetchedNoShareTarget2.shareTargetMethod = null;
        fetchedNoShareTarget2.shareTargetEncType = null;
        fetchedNoShareTarget2.shareTargetFileNames = null;
        fetchedNoShareTarget2.shareTargetFileAccepts = null;

        assertFalse(checkUpdateNeededForFetchedManifest(oldNoShareTarget, fetchedNoShareTarget2));

        ManifestData oldV1ShareTarget = defaultManifestData();
        oldV1ShareTarget.shareTargetMethod = null;
        oldV1ShareTarget.shareTargetEncType = null;
        oldV1ShareTarget.shareTargetFileNames = null;
        oldV1ShareTarget.shareTargetFileAccepts = null;

        ManifestData fetchedV1ShareTarget = defaultManifestData();
        fetchedV1ShareTarget.shareTargetMethod = null;
        fetchedV1ShareTarget.shareTargetEncType = null;
        fetchedV1ShareTarget.shareTargetFileNames = null;
        fetchedV1ShareTarget.shareTargetFileAccepts = null;
        assertFalse(checkUpdateNeededForFetchedManifest(oldV1ShareTarget, fetchedV1ShareTarget));

        ManifestData oldV2ShareTarget = defaultManifestData();
        ManifestData fetchedV2ShareTarget = defaultManifestData();
        assertFalse(checkUpdateNeededForFetchedManifest(oldV2ShareTarget, fetchedV2ShareTarget));

        assertTrue(checkUpdateNeededForFetchedManifest(oldNoShareTarget, fetchedV1ShareTarget));
        assertTrue(checkUpdateNeededForFetchedManifest(oldNoShareTarget, fetchedV2ShareTarget));
        assertTrue(checkUpdateNeededForFetchedManifest(oldV1ShareTarget, fetchedV2ShareTarget));
        assertTrue(checkUpdateNeededForFetchedManifest(fetchedV2ShareTarget, fetchedV1ShareTarget));
        assertTrue(checkUpdateNeededForFetchedManifest(fetchedV2ShareTarget, oldNoShareTarget));
        assertTrue(checkUpdateNeededForFetchedManifest(fetchedV1ShareTarget, oldNoShareTarget));
    }

    /** Test that an upgrade is requested when the Web Manifest 'scope' changes. */
    @Test
    public void testManifestScopeChangedShouldUpgrade() {
        ManifestData oldData = defaultManifestData();
        // webapk_installer.cc sets the scope to the default scope if the scope is empty.
        oldData.scopeUrl = "/scope1/";
        ManifestData fetchedData = defaultManifestData();
        fetchedData.scopeUrl = "/scope2/";
        assertTrue(checkUpdateNeededForFetchedManifest(oldData, fetchedData));
    }

    /**
     * Test that an upgrade is not requested when the Web Manifest did not change and the Web
     * Manifest scope is empty. TODO(crbug.com/40827678): Re-enable test.
     */
    @Ignore
    @Test
    public void testManifestEmptyScopeShouldNotUpgrade() {
        ManifestData oldData = defaultManifestData();
        // webapk_installer.cc sets the scope to the default scope if the scope is empty.
        oldData.scopeUrl = ShortcutHelper.getScopeFromUrl(oldData.startUrl);
        ManifestData fetchedData = defaultManifestData();
        fetchedData.scopeUrl = "";
        assertTrue(!oldData.scopeUrl.equals(fetchedData.scopeUrl));
        assertFalse(checkUpdateNeededForFetchedManifest(oldData, fetchedData));
    }

    /**
     * Test that an upgrade is requested when the Web Manifest is updated from using a non-empty
     * scope to an empty scope.
     */
    @Test
    public void testManifestNonEmptyScopeToEmptyScopeShouldUpgrade() {
        ManifestData oldData = defaultManifestData();
        oldData.startUrl = "/fancy/scope/special/snowflake.html";
        oldData.scopeUrl = "/fancy/scope/";
        assertTrue(!oldData.scopeUrl.equals(ShortcutHelper.getScopeFromUrl(oldData.startUrl)));
        ManifestData fetchedData = defaultManifestData();
        fetchedData.startUrl = "/fancy/scope/special/snowflake.html";
        fetchedData.scopeUrl = "";

        assertTrue(checkUpdateNeededForFetchedManifest(oldData, fetchedData));
    }

    /**
     * Test that an upgrade is requested when:
     * - WebAPK was generated using icon at {@link PRIMARY_ICON_URL} from Web Manifest.
     * - Bitmap at {@link PRIMARY_ICON_URL} has changed.
     */
    @Test
    public void testPrimaryIconChangeShouldUpgrade() {
        ManifestData fetchedData = defaultManifestData();
        fetchedData.iconUrlToMurmur2HashMap.put(
                fetchedData.primaryIconUrl, PRIMARY_ICON_MURMUR2_HASH + "1");
        fetchedData.primaryIcon = createBitmap(Color.BLUE);
        assertTrue(
                checkUpdateNeededForFetchedManifest(
                        defaultManifestData(),
                        fetchedData,
                        /* nameUpdatesEnabled= */ false,
                        /* iconUpdatesEnabled= */ true));
    }

    /**
     * Test that an upgrade is requested when:
     * - WebAPK is generated using icon at {@link PRIMARY_ICON_URL} from Web Manifest.
     * - A new icon URL is added to the Web Manifest. And InstallableManager selects the new icon as
     *   the primary icon.
     */
    @Test
    public void testPrimaryIconUrlChangeShouldUpgrade() {
        ManifestData fetchedData = defaultManifestData();
        fetchedData.iconUrlToMurmur2HashMap.put("/icon2.png", "22");
        fetchedData.primaryIconUrl = "/icon2.png";
        assertTrue(
                checkUpdateNeededForFetchedManifest(
                        defaultManifestData(),
                        fetchedData,
                        /* nameUpdatesEnabled= */ false,
                        /* iconUpdatesEnabled= */ true));
    }

    /**
     * Test that an upgrade is not requested if:
     * - icon URL is added to the Web Manifest
     * AND
     * - "best" icon URL for the primary icon did not change.
     * AND
     * - "best" icon URL for the monochrome icon did not change.
     */
    @Test
    public void
            testIconUrlsChangeShouldNotUpgradeIfPrimaryIconUrlAndMonochromeIconUrlDoNotChange() {
        ManifestData fetchedData = defaultManifestData();
        fetchedData.iconUrlToMurmur2HashMap.put("/icon2.png", null);
        assertFalse(checkUpdateNeededForFetchedManifest(defaultManifestData(), fetchedData));
    }

    /**
     * Test that an upgrade is not requested if:
     * - the WebAPK's meta data has murmur2 hashes for all of the icons.
     * AND
     * - the Web Manifest has not changed
     * AND
     * - the computed best icon URLs are different from the one stored in the WebAPK's meta data.
     */
    @Test
    public void testWebManifestSameButBestIconUrlChangedShouldNotUpgrade() {
        String iconUrl1 = "/icon1.png";
        String iconUrl2 = "/icon2.png";
        String monochromeUrl1 = "/monochrome1.png";
        String monochromeUrl2 = "/monochrome2.png";
        String hash1 = "11";
        String hash2 = "22";
        String hash3 = "33";
        String hash4 = "44";

        ManifestData androidManifestData = defaultManifestData();
        androidManifestData.primaryIconUrl = iconUrl1;
        androidManifestData.iconUrlToMurmur2HashMap.clear();
        androidManifestData.iconUrlToMurmur2HashMap.put(iconUrl1, hash1);
        androidManifestData.iconUrlToMurmur2HashMap.put(iconUrl2, hash2);
        androidManifestData.iconUrlToMurmur2HashMap.put(monochromeUrl1, hash3);
        androidManifestData.iconUrlToMurmur2HashMap.put(monochromeUrl2, hash4);

        ManifestData fetchedManifestData = defaultManifestData();
        fetchedManifestData.primaryIconUrl = iconUrl2;
        fetchedManifestData.iconUrlToMurmur2HashMap.clear();
        fetchedManifestData.iconUrlToMurmur2HashMap.put(iconUrl1, null);
        fetchedManifestData.iconUrlToMurmur2HashMap.put(iconUrl2, hash2);
        fetchedManifestData.iconUrlToMurmur2HashMap.put(monochromeUrl1, null);
        fetchedManifestData.iconUrlToMurmur2HashMap.put(monochromeUrl2, hash4);

        assertFalse(checkUpdateNeededForFetchedManifest(androidManifestData, fetchedManifestData));
    }

    /** Test that an upgrade is requested when the Web Manifest 'short_name' changes. */
    @Test
    public void testManifestShortNameChangedShouldUpgrade() {
        ManifestData fetchedData = defaultManifestData();
        fetchedData.shortName = SHORT_NAME + "2";
        assertTrue(
                checkUpdateNeededForFetchedManifest(
                        defaultManifestData(),
                        fetchedData,
                        /* nameUpdatesEnabled= */ true,
                        /* iconUpdatesEnabled= */ false));
    }

    /** Test that an upgrade is requested when the Web Manifest 'name' changes. */
    @Test
    public void testManifestNameChangedShouldUpgrade() {
        ManifestData fetchedData = defaultManifestData();
        fetchedData.name = NAME + "2";
        assertTrue(
                checkUpdateNeededForFetchedManifest(
                        defaultManifestData(),
                        fetchedData,
                        /* nameUpdatesEnabled= */ true,
                        /* iconUpdatesEnabled= */ false));
    }

    /** Test that an upgrade is requested when the Web Manifest 'display' changes. */
    @Test
    public void testManifestDisplayModeChangedShouldUpgrade() {
        ManifestData oldData = defaultManifestData();
        oldData.displayMode = DisplayMode.STANDALONE;
        ManifestData fetchedData = defaultManifestData();
        fetchedData.displayMode = DisplayMode.FULLSCREEN;
        assertTrue(checkUpdateNeededForFetchedManifest(oldData, fetchedData));
    }

    /** Test that an upgrade is requested when the Web Manifest 'orientation' changes. */
    @Test
    public void testManifestOrientationChangedShouldUpgrade() {
        ManifestData oldData = defaultManifestData();
        oldData.orientation = ScreenOrientationLockType.LANDSCAPE;
        ManifestData fetchedData = defaultManifestData();
        fetchedData.orientation = ScreenOrientationLockType.PORTRAIT;
        assertTrue(checkUpdateNeededForFetchedManifest(oldData, fetchedData));
    }

    /** Test that an upgrade is requested when the Web Manifest 'theme_color' changes. */
    @Test
    public void testManifestThemeColorChangedShouldUpgrade() {
        ManifestData oldData = defaultManifestData();
        oldData.themeColor = 1L;
        ManifestData fetchedData = defaultManifestData();
        fetchedData.themeColor = 2L;
        assertTrue(checkUpdateNeededForFetchedManifest(oldData, fetchedData));
    }

    /** Test that an upgrade is requested when the Web Manifest 'background_color' changes. */
    @Test
    public void testManifestBackgroundColorChangedShouldUpgrade() {
        ManifestData oldData = defaultManifestData();
        oldData.backgroundColor = 1L;
        ManifestData fetchedData = defaultManifestData();
        fetchedData.backgroundColor = 2L;
        assertTrue(checkUpdateNeededForFetchedManifest(oldData, fetchedData));
    }

    /** Test that an upgrade is requested when the Web Manifest 'dark_theme_color' changes. */
    @Test
    public void testManifestDarkThemeColorChangedShouldUpgrade() {
        ManifestData oldData = defaultManifestData();
        oldData.themeColor = 3L;
        ManifestData fetchedData = defaultManifestData();
        fetchedData.themeColor = 4L;
        assertTrue(checkUpdateNeededForFetchedManifest(oldData, fetchedData));
    }

    /** Test that an upgrade is requested when the Web Manifest 'dark_background_color' changes. */
    @Test
    public void testManifestDarkBackgroundColorChangedShouldUpgrade() {
        ManifestData oldData = defaultManifestData();
        oldData.backgroundColor = 4L;
        ManifestData fetchedData = defaultManifestData();
        fetchedData.backgroundColor = 5L;
        assertTrue(checkUpdateNeededForFetchedManifest(oldData, fetchedData));
    }

    /**
     * Test that an upgrade is not requested if the AndroidManifest does not have a valid background
     * color and the default background color in the WebAPK's resources is different than
     * {@link SplashLayout#getDefaultBackgroundColor()} (due to a change in the return value of
     * {@link SplashLayout#getDefaultBackgroundColor()} in a new Chrome version).
     */
    @Test
    public void testDefaultBackgroundColorHasChangedShouldNotUpgrade() {
        int oldDefaultBackgroundColor = 3;
        int splashLayoutDefaultBackgroundColor =
                SplashLayout.getDefaultBackgroundColor(RuntimeEnvironment.application);
        assertNotEquals(oldDefaultBackgroundColor, splashLayoutDefaultBackgroundColor);

        ManifestData androidManifestData = defaultManifestData();
        androidManifestData.backgroundColor = ColorUtils.INVALID_COLOR;
        androidManifestData.defaultBackgroundColor = oldDefaultBackgroundColor;

        ManifestData fetchedManifestData = defaultManifestData();
        fetchedManifestData.backgroundColor = ColorUtils.INVALID_COLOR;
        fetchedManifestData.defaultBackgroundColor = splashLayoutDefaultBackgroundColor;

        assertFalse(checkUpdateNeededForFetchedManifest(androidManifestData, fetchedManifestData));
    }

    /** Test that an upgrade is requested when the Web Manifest 'start_url' changes. */
    @Test
    public void testManifestStartUrlChangedShouldUpgrade() {
        ManifestData oldData = defaultManifestData();
        oldData.startUrl = "/old_start_url.html";
        ManifestData fetchedData = defaultManifestData();
        fetchedData.startUrl = "/new_start_url.html";
        assertTrue(checkUpdateNeededForFetchedManifest(oldData, fetchedData));
    }

    /**
     * Tests that a WebAPK update is requested immediately if:
     * the Shell APK is out of date,
     * AND
     * there wasn't a previous request for this ShellAPK version.
     */
    @Test
    public void testShellApkOutOfDate() {
        registerWebApk(
                WEBAPK_PACKAGE_NAME,
                defaultManifestData(),
                REQUEST_UPDATE_FOR_SHELL_APK_VERSION - 1);
        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);

        // There have not been any update requests for the current ShellAPK version. A WebAPK update
        // should be requested immediately.
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotManifestData(updateManager, defaultManifestData());
        assertTrue(updateManager.updateRequested());
        tryCompletingUpdate(updateManager, storage, WebApkInstallResult.FAILURE);

        mClockRule.advanceMillis(1);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertFalse(updateManager.updateCheckStarted());

        // A previous update request was made for the current ShellAPK version. A WebAPK update
        // should be requested after the regular delay.
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL - 1);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotManifestData(updateManager, defaultManifestData());
        assertTrue(updateManager.updateRequested());
    }

    /**
     * Tests that a WebAPK update is requested if the Shell APK hasn't been updated for a long time
     * even if is is newer than the requested version.
     */
    @Test
    public void testShellApkOutOfDateInterval() {
        ShadowPackageManager pm =
                Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager());
        PackageInfo packageInfo =
                WebApkTestHelper.newPackageInfo(
                        WEBAPK_PACKAGE_NAME,
                        createWebApkMetadata(
                                defaultManifestData(), REQUEST_UPDATE_FOR_SHELL_APK_VERSION),
                        null,
                        null);
        packageInfo.lastUpdateTime = TimeUtils.currentTimeMillis();
        pm.addPackage(packageInfo);

        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);

        // Update is not needed when no manifest change and shell version is the same as
        // REQUEST_UPDATE_FOR_SHELL_APK_VERSION.
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotManifestData(updateManager, defaultManifestData());
        assertFalse(updateManager.updateRequested());
        tryCompletingUpdate(updateManager, storage, WebApkInstallResult.FAILURE);

        // Update is requested when a shell hasn't been updated for a long time.
        mClockRule.advanceMillis(WebApkUpdateManager.OLD_SHELL_NEEDS_UPDATE_INTERVAL);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotManifestData(updateManager, defaultManifestData());
        assertTrue(updateManager.updateRequested());
    }

    /**
     * Tests that a forced update is requested and performed immediately if there is a material
     * change to the manifest.
     */
    @Test
    public void testForcedUpdateSuccess() {
        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        storage.setShouldForceUpdate(true);
        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotDifferentData(updateManager);
        assertTrue(updateManager.updateRequested());
        tryCompletingUpdate(updateManager, storage, WebApkInstallResult.SUCCESS);
        assertFalse(storage.shouldForceUpdate());
    }

    /**
     * Tests that a forced update is requested, but not performed if there is no material change to
     * the manifest.
     */
    @Test
    public void testForcedUpdateNotNeeded() {
        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        storage.setShouldForceUpdate(true);
        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotManifestData(updateManager, defaultManifestData());
        assertFalse(updateManager.updateRequested());
        assertFalse(storage.shouldForceUpdate());
    }

    /** Tests that a forced update handles failure gracefully. */
    @Test
    public void testForcedUpdateFailure() {
        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        storage.setShouldForceUpdate(true);
        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotDifferentData(updateManager);
        assertTrue(updateManager.updateRequested());
        tryCompletingUpdate(updateManager, storage, WebApkInstallResult.FAILURE);
        assertFalse(storage.shouldForceUpdate());
    }

    /** Tests that a forced update handles failing to retrieve the manifest. */
    @Test
    public void testForcedUpdateManifestNotRetrieved() {
        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        storage.setShouldForceUpdate(true);
        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());
        onGotManifestData(updateManager, null);
        assertFalse(updateManager.updateRequested());
        assertFalse(storage.shouldForceUpdate());
    }

    /** Test that WebappDataStorage#setShouldForceUpdate() is a no-op for unbound WebAPKs. */
    @Test
    public void testForceUpdateUnboundWebApk() throws Exception {
        registerWebApk(
                UNBOUND_WEBAPK_PACKAGE_NAME,
                defaultManifestData(),
                REQUEST_UPDATE_FOR_SHELL_APK_VERSION);
        registerStorageForWebApkPackage(UNBOUND_WEBAPK_PACKAGE_NAME);
        WebappDataStorage storage = getStorage(UNBOUND_WEBAPK_PACKAGE_NAME);
        storage.updateWebApkPackageNameForTests(UNBOUND_WEBAPK_PACKAGE_NAME);
        // Should no-op for an unbound WebAPK.
        storage.setShouldForceUpdate(true);
        assertFalse(storage.shouldForceUpdate());

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(UNBOUND_WEBAPK_PACKAGE_NAME, updateManager);
        assertFalse(updateManager.updateCheckStarted());
        assertFalse(updateManager.updateRequested());
    }

    /** Test that an update is required if a shortcut has been added. */
    @Test
    public void testUpdateIfShortcutIsAdded() {
        ManifestData fetchedData = defaultManifestData();
        fetchedData.shortcuts.add(
                new WebApkExtras.ShortcutItem(
                        "name", "shortName", "launchUrl", "iconUrl", "iconHash", new WebappIcon()));
        assertTrue(checkUpdateNeededForFetchedManifest(defaultManifestData(), fetchedData));
    }

    /** Test that an update is required if a shortcut name has been changed. */
    @Test
    public void testUpdateIfShortcutHasChangedName() {
        ManifestData androidData = defaultManifestData();
        androidData.shortcuts.add(
                new WebApkExtras.ShortcutItem(
                        "name1",
                        "shortName",
                        "launchUrl",
                        "iconUrl",
                        "iconHash",
                        new WebappIcon("appName", 42)));
        ManifestData fetchedData = defaultManifestData();
        fetchedData.shortcuts.add(
                new WebApkExtras.ShortcutItem(
                        "name2",
                        "shortName",
                        "launchUrl",
                        "iconUrl",
                        "iconHash",
                        new WebappIcon()));
        assertTrue(checkUpdateNeededForFetchedManifest(androidData, fetchedData));
    }

    /** Test that an update is required if a shortcut short name has been changed. */
    @Test
    public void testUpdateIfShortcutHasChangedShortName() {
        ManifestData androidData = defaultManifestData();
        androidData.shortcuts.add(
                new WebApkExtras.ShortcutItem(
                        "name",
                        "shortName1",
                        "launchUrl",
                        "iconUrl",
                        "iconHash",
                        new WebappIcon("appName", 42)));
        ManifestData fetchedData = defaultManifestData();
        fetchedData.shortcuts.add(
                new WebApkExtras.ShortcutItem(
                        "name",
                        "shortName2",
                        "launchUrl",
                        "iconUrl",
                        "iconHash",
                        new WebappIcon()));
        assertTrue(checkUpdateNeededForFetchedManifest(androidData, fetchedData));
    }

    /** Test that an update is required if a shortcut launch URL has been changed. */
    @Test
    public void testUpdateIfShortcutHasChangedLaunchUrl() {
        ManifestData androidData = defaultManifestData();
        androidData.shortcuts.add(
                new WebApkExtras.ShortcutItem(
                        "name",
                        "shortName",
                        "launchUrl1",
                        "iconUrl",
                        "iconHash",
                        new WebappIcon("appName", 42)));
        ManifestData fetchedData = defaultManifestData();
        fetchedData.shortcuts.add(
                new WebApkExtras.ShortcutItem(
                        "name",
                        "shortName",
                        "launchUrl2",
                        "iconUrl",
                        "iconHash",
                        new WebappIcon()));
        assertTrue(checkUpdateNeededForFetchedManifest(androidData, fetchedData));
    }

    /** Test that an update is required if a shortcut icon hash has been changed. */
    @Test
    public void testUpdateIfShortcutHasChangedIconHash() {
        ManifestData androidData = defaultManifestData();
        androidData.shortcuts.add(
                new WebApkExtras.ShortcutItem(
                        "name",
                        "shortName",
                        "launchUrl",
                        "iconUrl",
                        "iconHash1",
                        new WebappIcon("appName", 42)));
        ManifestData fetchedData = defaultManifestData();
        fetchedData.shortcuts.add(
                new WebApkExtras.ShortcutItem(
                        "name",
                        "shortName",
                        "launchUrl",
                        "iconUrl",
                        "iconHash2",
                        new WebappIcon()));
        assertTrue(checkUpdateNeededForFetchedManifest(androidData, fetchedData));
    }

    /** Test that an update is not required if a shortcut url has changed but the hash hasn't. */
    @Test
    public void testNoUpdateIfShortcutHasOnlyIconUrlChanges() {
        ManifestData androidData = defaultManifestData();
        androidData.shortcuts.add(
                new WebApkExtras.ShortcutItem(
                        "name",
                        "shortName",
                        "launchUrl",
                        "iconUrl1",
                        "iconHash",
                        new WebappIcon("appName", 42)));
        ManifestData fetchedData = defaultManifestData();
        fetchedData.shortcuts.add(
                new WebApkExtras.ShortcutItem(
                        "name",
                        "shortName",
                        "launchUrl",
                        "iconUrl2",
                        "iconHash",
                        new WebappIcon()));
        assertFalse(checkUpdateNeededForFetchedManifest(androidData, fetchedData));
    }

    private void verifyHistograms(String name, int expectedCallCount) {
        assertEquals(
                "Histogram record count doesn't match.",
                expectedCallCount,
                RecordHistogram.getHistogramTotalCountForTesting(name));
    }

    @Test
    public void testDialogSuppressed() {
        WebappDataStorage storage = getStorage(WEBAPK_PACKAGE_NAME);
        ManifestData androidData = defaultManifestData();
        ManifestData fetchedData = defaultManifestData();

        boolean iconUpdatesEnabled = false;
        boolean nameUpdatesEnabled = false;

        // Try with unchanged manifest data.
        assertFalse(
                checkUpdateNeededForFetchedManifest(
                        androidData, fetchedData, nameUpdatesEnabled, iconUpdatesEnabled));
        verifyHistograms(HISTOGRAM_NOT_SHOWING, 0);
        verifyHistograms(HISTOGRAM_SHOWING, 0);
        verifyHistograms(HISTOGRAM_PRE_APPROVED, 0);

        // Change the app name (and pre-approve the update), but don't enable
        // naming updates (then the old name will be used and no update detected).
        fetchedData.name = "foo";
        String hash = "foo|Short Name|3|NotAdaptive";
        storage.updateLastWebApkUpdateHashAccepted(hash);
        assertFalse(
                checkUpdateNeededForFetchedManifest(
                        androidData, fetchedData, nameUpdatesEnabled, iconUpdatesEnabled));
        verifyHistograms(HISTOGRAM_NOT_SHOWING, 0);
        verifyHistograms(HISTOGRAM_SHOWING, 0);
        verifyHistograms(HISTOGRAM_PRE_APPROVED, 0);

        nameUpdatesEnabled = true;

        // Now try again, but with naming updates allowed.
        hash = "foo|Short Name|3|NotAdaptive";
        storage.updateLastWebApkUpdateHashAccepted(hash);
        assertTrue(
                checkUpdateNeededForFetchedManifest(
                        androidData, fetchedData, nameUpdatesEnabled, iconUpdatesEnabled));
        verifyHistograms(HISTOGRAM_NOT_SHOWING, 0);
        verifyHistograms(HISTOGRAM_SHOWING, 0);
        verifyHistograms(HISTOGRAM_PRE_APPROVED, 1);

        // Revert the name change.
        fetchedData.name = NAME;

        // Now change the short name (and pre-approve the update).
        fetchedData.shortName = "bar";
        hash = "Long Name|bar|3|NotAdaptive";
        storage.updateLastWebApkUpdateHashAccepted(hash);
        assertTrue(
                checkUpdateNeededForFetchedManifest(
                        androidData, fetchedData, nameUpdatesEnabled, iconUpdatesEnabled));
        verifyHistograms(HISTOGRAM_NOT_SHOWING, 0);
        verifyHistograms(HISTOGRAM_SHOWING, 0);
        verifyHistograms(HISTOGRAM_PRE_APPROVED, 2);

        // Revert the shortName change.
        fetchedData.shortName = SHORT_NAME;

        // Also change the icon hash (and pre-approve the update), but don't allow updates of the
        // icon (which will cause the old info to be used).
        fetchedData.iconUrlToMurmur2HashMap.put(PRIMARY_ICON_URL, "42");
        hash = "Long Name|Short Name|3|NotAdaptive";
        storage.updateLastWebApkUpdateHashAccepted(hash);
        assertFalse(
                checkUpdateNeededForFetchedManifest(
                        androidData, fetchedData, nameUpdatesEnabled, iconUpdatesEnabled));
        verifyHistograms(HISTOGRAM_NOT_SHOWING, 0);
        verifyHistograms(HISTOGRAM_SHOWING, 0);
        verifyHistograms(HISTOGRAM_PRE_APPROVED, 2);

        iconUpdatesEnabled = true;

        // Now try again, but with icon updates allowed.
        hash = "Long Name|Short Name|42|NotAdaptive";
        storage.updateLastWebApkUpdateHashAccepted(hash);
        assertTrue(
                checkUpdateNeededForFetchedManifest(
                        androidData, fetchedData, nameUpdatesEnabled, iconUpdatesEnabled));
        verifyHistograms(HISTOGRAM_NOT_SHOWING, 0);
        verifyHistograms(HISTOGRAM_SHOWING, 0);
        verifyHistograms(HISTOGRAM_PRE_APPROVED, 3);

        // Now try with both name and icon updates.
        fetchedData.name = "foo";
        fetchedData.shortName = "bar";
        hash = "foo|bar|42|NotAdaptive";
        storage.updateLastWebApkUpdateHashAccepted(hash);
        assertTrue(
                checkUpdateNeededForFetchedManifest(
                        androidData, fetchedData, nameUpdatesEnabled, iconUpdatesEnabled));
        verifyHistograms(HISTOGRAM_NOT_SHOWING, 0);
        verifyHistograms(HISTOGRAM_SHOWING, 0);
        verifyHistograms(HISTOGRAM_PRE_APPROVED, 4);
    }

    @Test
    public void testEmptyAppIdentityHash() {
        // Setup the test to trigger a null mFetchedInfo within WebApkUpdateManager.
        registerWebApk(
                WEBAPK_PACKAGE_NAME,
                defaultManifestData(),
                REQUEST_UPDATE_FOR_SHELL_APK_VERSION - 1);
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTrue(updateManager.updateRequested());
        assertEquals(NAME, updateManager.requestedUpdateName());

        // Given a null mFetchedInfo (see above) this path should exercise
        // WebApkUpdateManager#getAppIdentityHash returning a blank hash, which should not be
        // treated as a trigger for the pre-approved path when it comes to AppIdentity updates.
        ManifestData androidData = defaultManifestData();
        ManifestData fetchedData = defaultManifestData();

        assertFalse(checkUpdateNeededForFetchedManifest(androidData, fetchedData));
        verifyHistograms(HISTOGRAM_NOT_SHOWING, 1);
        verifyHistograms(HISTOGRAM_SHOWING, 0);
        verifyHistograms(HISTOGRAM_PRE_APPROVED, 0);
    }

    /** Test for crashing when IntentDataProvider is null, as per https://crbug.com/1342066. */
    @Test
    public void testDoesntCrashWithNullProvider() {
        ManifestData androidManifestData = defaultManifestData();
        registerWebApk(
                WEBAPK_PACKAGE_NAME, androidManifestData, REQUEST_UPDATE_FOR_SHELL_APK_VERSION);
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager, androidManifestData.shortcuts);
        assertTrue(updateManager.updateCheckStarted());
        updateManager.onGotManifestData(
                /* fetchedIntentDataProvider= */ null,
                /* primaryIconUrl= */ null,
                /* splashIconUrl= */ null);
    }

    @Test
    public void testManifestIdChangeShouldNotUpdate() {
        ManifestData androidData = defaultManifestData();
        ManifestData fetchedData = defaultManifestData();
        fetchedData.id = MANIFEST_ID + "1";
        assertFalse(checkUpdateNeededForFetchedManifest(androidData, fetchedData));
    }

    @Test
    public void testAppKeyNotChangeWhenUpdate() {
        ManifestData androidData = defaultManifestData();
        androidData.appKey = WEB_MANIFEST_URL;
        registerWebApk(WEBAPK_PACKAGE_NAME, androidData, REQUEST_UPDATE_FOR_SHELL_APK_VERSION);
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());

        ManifestData fetchedData = defaultManifestData();
        fetchedData.appKey = "another id";
        // Set a different backgroundColor to trigger an update.
        fetchedData.backgroundColor = DIFFERENT_BACKGROUND_COLOR;
        onGotManifestData(updateManager, fetchedData);

        assertTrue(updateManager.updateRequested());
        assertEquals(WEB_MANIFEST_URL, updateManager.requestedAppKey());
    }

    /**
     * Tests that WebAPK updates keeps the default appKey when no value specified from the WebAPK's
     * Android Manifest <meta-data>.
     */
    @Test
    public void testEmptyManifestAppKeyHasDefault() {
        ManifestData androidData = defaultManifestData();
        androidData.id = null;
        androidData.appKey = null;

        registerWebApk(WEBAPK_PACKAGE_NAME, androidData, REQUEST_UPDATE_FOR_SHELL_APK_VERSION);
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());

        onGotDifferentData(updateManager);
        assertTrue(updateManager.updateRequested());
        assertEquals(WEB_MANIFEST_URL, updateManager.requestedAppKey());
    }

    /**
     * Tests that WebAPK updates keeps the default appKey when no value specified from the WebAPK's
     * Android Manifest <meta-data>.
     */
    @Test
    public void testUpdateWithCustomName() {
        ManifestData androidData = defaultManifestData();
        androidData.name = "custom name";
        androidData.hasCustomName = true;

        registerWebApk(WEBAPK_PACKAGE_NAME, androidData, REQUEST_UPDATE_FOR_SHELL_APK_VERSION - 1);
        mClockRule.advanceMillis(WebappDataStorage.UPDATE_INTERVAL);

        TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(mActivityMock);
        updateIfNeeded(WEBAPK_PACKAGE_NAME, updateManager);
        assertTrue(updateManager.updateCheckStarted());

        onGotDifferentData(updateManager);
        assertTrue(updateManager.updateRequested());
        assertEquals(androidData.name, updateManager.requestedUpdateName());
    }
}
