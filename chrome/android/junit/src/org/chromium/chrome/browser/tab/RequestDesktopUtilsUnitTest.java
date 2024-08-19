// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.components.content_settings.PrefNames.DESKTOP_SITE_WINDOW_SETTING_ENABLED;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.Window;
import android.view.WindowManager;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.SysUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.RequestDesktopUtilsUnitTest.ShadowDisplayAndroid;
import org.chromium.chrome.browser.tab.RequestDesktopUtilsUnitTest.ShadowDisplayAndroidManager;
import org.chromium.chrome.browser.tab.RequestDesktopUtilsUnitTest.ShadowDisplayUtil;
import org.chromium.chrome.browser.tab.RequestDesktopUtilsUnitTest.ShadowSysUtils;
import org.chromium.chrome.browser.tab.RequestDesktopUtilsUnitTest.ShadowTabUtils;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettingsConstants;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;

/** Unit tests for {@link RequestDesktopUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {
            ShadowSysUtils.class,
            ShadowDisplayAndroid.class,
            ShadowDisplayAndroidManager.class,
            ShadowTabUtils.class,
            ShadowDisplayUtil.class
        })
public class RequestDesktopUtilsUnitTest {
    @Rule public JniMocker mJniMocker = new JniMocker();

    /** Shadows {@link SysUtils} class for testing. */
    @Implements(SysUtils.class)
    public static class ShadowSysUtils {
        private static int sMemoryInMB;

        public static void setMemoryInMB(int memoryInMB) {
            sMemoryInMB = memoryInMB;
        }

        @Implementation
        public static int amountOfPhysicalMemoryKB() {
            return sMemoryInMB * ConversionUtils.KILOBYTES_PER_MEGABYTE;
        }
    }

    @Implements(DisplayAndroid.class)
    static class ShadowDisplayAndroid {
        private static DisplayAndroid sDisplayAndroid;

        public static void setDisplayAndroid(DisplayAndroid displayAndroid) {
            sDisplayAndroid = displayAndroid;
        }

        @Implementation
        public static DisplayAndroid getNonMultiDisplay(Context context) {
            return sDisplayAndroid;
        }
    }

    @Implements(DisplayAndroidManager.class)
    static class ShadowDisplayAndroidManager {
        private static Display sDisplay;

        public static void setDisplay(Display display) {
            sDisplay = display;
        }

        @Implementation
        public static Display getDefaultDisplayForContext(Context context) {
            return sDisplay;
        }
    }

    @Implements(TabUtils.class)
    static class ShadowTabUtils {
        private static boolean sIsGlobalSetting;

        public static void setIsGlobalSetting(Boolean isGlobalSetting) {
            sIsGlobalSetting = isGlobalSetting;
        }

        @Implementation
        public static boolean isRequestDesktopSiteContentSettingsGlobal(Profile profile, GURL url) {
            return sIsGlobalSetting;
        }
    }

    @Implements(DisplayUtil.class)
    static class ShadowDisplayUtil {
        private static int sSmallestScreenWidthDp;

        public static void setCurrentSmallestScreenWidth(int smallestScreenWidthDp) {
            sSmallestScreenWidthDp = smallestScreenWidthDp;
        }

        @Implementation
        public static int getCurrentSmallestScreenWidth(Context context) {
            return sSmallestScreenWidthDp;
        }
    }

    @Mock private WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;
    @Mock private MessageDispatcher mMessageDispatcher;
    @Mock private Activity mActivity;
    @Mock private Window mWindow;
    @Mock private WindowManager.LayoutParams mLayoutParams;
    @Mock private DisplayMetrics mDisplayMetrics;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private ObservableSupplier<Tab> mCurrentTabSupplier;
    @Mock private DisplayAndroid mDisplayAndroid;
    @Mock private Display mDisplay;
    @Mock private UserPrefs.Natives mUserPrefsJni;
    @Mock private PrefService mPrefService;

    private Tab mTab;
    private @ContentSettingValues int mRdsDefaultValue;
    private boolean mWindowSetting;
    private SharedPreferencesManager mSharedPreferencesManager;

    private final Map<String, Integer> mContentSettingMap = new HashMap<>();
    private final GURL mGoogleUrl = JUnitTestGURLs.GOOGLE_URL;
    private final GURL mMapsUrl = JUnitTestGURLs.MAPS_URL;

    private Resources mResources;

    private final TestValues mTestValues = new TestValues();

    private static final String ANY_SUBDOMAIN_PATTERN = "[*.]";
    private static final String GOOGLE_COM = "[*.]google.com/";
    private ShadowPackageManager mShadowPackageManager;
    private boolean mIsDefaultValuePreference;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJniMock);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJni);

        mTab = createTab();

        doAnswer(invocation -> mRdsDefaultValue)
                .when(mWebsitePreferenceBridgeJniMock)
                .getDefaultContentSetting(any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE));

        doAnswer(
                        invocation -> {
                            mRdsDefaultValue =
                                    invocation.getArgument(2)
                                            ? ContentSettingValues.ALLOW
                                            : ContentSettingValues.BLOCK;
                            return null;
                        })
                .when(mWebsitePreferenceBridgeJniMock)
                .setContentSettingEnabled(
                        any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE), anyBoolean());

        doAnswer(
                        invocation -> {
                            mContentSettingMap.put(
                                    invocation.getArgument(2), invocation.getArgument(4));
                            return null;
                        })
                .when(mWebsitePreferenceBridgeJniMock)
                .setContentSettingCustomScope(
                        any(),
                        eq(ContentSettingsType.REQUEST_DESKTOP_SITE),
                        anyString(),
                        anyString(),
                        anyInt());
        doAnswer(invocation -> toDomainWildcardPattern(invocation.getArgument(0)))
                .when(mWebsitePreferenceBridgeJniMock)
                .toDomainWildcardPattern(anyString());

        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();

        mResources = ApplicationProvider.getApplicationContext().getResources();
        mResources.getConfiguration().smallestScreenWidthDp = 600;
        when(mActivity.getResources()).thenReturn(mResources);

        TrackerFactory.setTrackerForTests(mTracker);
        disableGlobalDefaultsExperimentFeatures();

        ShadowSysUtils.setMemoryInMB(7000);
        ShadowDisplayAndroid.setDisplayAndroid(mDisplayAndroid);
        when(mDisplayAndroid.getDisplayWidth()).thenReturn(1600);
        when(mDisplayAndroid.getDisplayHeight()).thenReturn(2560);
        when(mDisplayAndroid.getXdpi()).thenReturn(275.5f);
        when(mDisplayAndroid.getYdpi()).thenReturn(276.5f);
        ShadowDisplayAndroidManager.setDisplay(mDisplay);
        when(mDisplay.getDisplayId()).thenReturn(Display.DEFAULT_DISPLAY);
        ShadowDisplayUtil.setCurrentSmallestScreenWidth(800);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);
        doAnswer(invocation -> mWindowSetting)
                .when(mPrefService)
                .getBoolean(eq(DESKTOP_SITE_WINDOW_SETTING_ENABLED));
        doAnswer(invocation -> mIsDefaultValuePreference)
                .when(mPrefService)
                .isDefaultValuePreference(eq(DESKTOP_SITE_WINDOW_SETTING_ENABLED));
        doAnswer(
                        invocation -> {
                            mWindowSetting = invocation.getArgument(1);
                            return true;
                        })
                .when(mPrefService)
                .setBoolean(eq(DESKTOP_SITE_WINDOW_SETTING_ENABLED), anyBoolean());
        ShadowTabUtils.setIsGlobalSetting(true);
        when(mActivity.getWindow()).thenReturn(mWindow);
        when(mWindow.getAttributes()).thenReturn(mLayoutParams);
        mLayoutParams.width = -1;
        mDisplayMetrics.density = 1.0f;
        mDisplayMetrics.widthPixels = 800;
        mShadowPackageManager =
                Shadows.shadowOf(ContextUtils.getApplicationContext().getPackageManager());
        mShadowPackageManager.setSystemFeature(
                PackageManager.FEATURE_AUTOMOTIVE, /* supported= */ false);
        RequestDesktopUtils.setTestDisplayMetrics(mDisplayMetrics);
        BuildConfig.IS_DESKTOP_ANDROID = false;
        ResettersForTesting.register(() -> BuildConfig.IS_DESKTOP_ANDROID = false);
    }

    @After
    public void tearDown() {
        FeatureList.setTestValues(null);
        ShadowDisplayAndroid.setDisplayAndroid(null);
        if (mSharedPreferencesManager != null) {
            mSharedPreferencesManager.removeKey(
                    ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING);
            mSharedPreferencesManager.removeKey(
                    SingleCategorySettingsConstants
                            .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY);
        }
    }

    @Test
    public void testSetRequestDesktopSiteContentSettingsForUrl_DefaultBlock_Incognito() {
        // Incognito profile type.
        when(mProfile.isOffTheRecord()).thenReturn(true);
        when(mProfile.isPrimaryOTRProfile()).thenReturn(true);
        mRdsDefaultValue = ContentSettingValues.BLOCK;

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.ALLOW,
                mContentSettingMap.get(GOOGLE_COM).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.BLOCK,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void testSetRequestDesktopSiteContentSettingsForUrl_DefaultAllow_Incognito() {
        // Incognito profile type.
        when(mProfile.isOffTheRecord()).thenReturn(true);
        when(mProfile.isPrimaryOTRProfile()).thenReturn(true);
        mRdsDefaultValue = ContentSettingValues.ALLOW;

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.BLOCK,
                mContentSettingMap.get(GOOGLE_COM).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.ALLOW,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void
            testSetRequestDesktopSiteContentSettingsForUrl_DefaultBlock_SiteBlock_WindowSettingOn() {
        mWindowSetting = true;
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSettingValues.BLOCK;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSettingValues.BLOCK);
        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.ALLOW,
                mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals(
                "Request Desktop Site subdomain level setting should be removed.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting should not be removed "
                        + "when window setting is ON.",
                ContentSettingValues.BLOCK,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void
            testSetRequestDesktopSiteContentSettingsForUrl_DefaultBlock_SiteBlock_WindowSettingOff() {
        mWindowSetting = false;
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSettingValues.BLOCK;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSettingValues.BLOCK);
        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.ALLOW,
                mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals(
                "Request Desktop Site subdomain level setting should be removed.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting should be removed "
                        + "when window setting is OFF.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void
            testSetRequestDesktopSiteContentSettingsForUrl_DefaultBlock_SiteAllow_WindowSettingOn() {
        mWindowSetting = true;
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSettingValues.BLOCK;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSettingValues.ALLOW);
        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting should not be removed "
                        + "when window setting is ON.",
                ContentSettingValues.BLOCK,
                mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals(
                "Request Desktop Site subdomain level setting should be removed.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.ALLOW,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void
            testSetRequestDesktopSiteContentSettingsForUrl_DefaultBlock_SiteAllow_WindowSettingOff() {
        mWindowSetting = false;
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSettingValues.BLOCK;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSettingValues.ALLOW);
        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting should be removed "
                        + "when window setting is OFF.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals(
                "Request Desktop Site subdomain level setting should be removed.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.ALLOW,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void
            testSetRequestDesktopSiteContentSettingsForUrl_DefaultAllow_SiteAllow_WindowSettingOn() {
        mWindowSetting = true;
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSettingValues.ALLOW;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSettingValues.ALLOW);
        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.BLOCK,
                mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals(
                "Request Desktop Site subdomain level setting should be removed.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting should not be removed "
                        + "when window setting is ON.",
                ContentSettingValues.ALLOW,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void
            testSetRequestDesktopSiteContentSettingsForUrl_DefaultAllow_SiteAllow_WindowSettingOff() {
        mWindowSetting = false;
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSettingValues.ALLOW;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSettingValues.ALLOW);
        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.BLOCK,
                mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals(
                "Request Desktop Site subdomain level setting should be removed.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting should be removed "
                        + "when window setting is OFF.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void
            testSetRequestDesktopSiteContentSettingsForUrl_DefaultAllow_SiteBlock_WindowSettingOn() {
        mWindowSetting = true;
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSettingValues.ALLOW;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSettingValues.BLOCK);
        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting should not be removed "
                        + "when window setting is ON.",
                ContentSettingValues.ALLOW,
                mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals(
                "Request Desktop Site subdomain level setting should be removed.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.BLOCK,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void
            testSetRequestDesktopSiteContentSettingsForUrl_DefaultAllow_SiteBlock_WindowSettingOff() {
        mWindowSetting = false;
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSettingValues.ALLOW;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSettingValues.BLOCK);
        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, true);
        Assert.assertEquals(
                "Request Desktop Site domain level setting should be removed "
                        + "when window setting is OFF.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals(
                "Request Desktop Site subdomain level setting should be removed.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, false);
        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.BLOCK,
                mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    /**
     * Helper to get domain wildcard pattern from URL. The real implementation calls
     * {@link WebsitePreferenceBridge}.
     * @param origin A URL.
     * @return The domain wildcard pattern from the given URL.
     */
    private String toDomainWildcardPattern(String origin) {
        return ANY_SUBDOMAIN_PATTERN + origin.replaceAll(".*\\.(.+\\.[^.]+$)", "$1");
    }

    @Test
    public void testShouldDefaultEnableGlobalSetting_IsAndroidDesktop() {
        BuildConfig.IS_DESKTOP_ANDROID = true;
        ShadowSysUtils.setMemoryInMB(4000);
        boolean shouldDefaultEnable =
                RequestDesktopUtils.shouldDefaultEnableGlobalSetting(11, mActivity);
        Assert.assertTrue(
                "Desktop site global setting should be default-enabled on desktop "
                        + "Android, even for low memory",
                shouldDefaultEnable);
    }

    @Test
    public void testShouldDefaultEnableGlobalSetting_MemoryThreshold() {
        ShadowSysUtils.setMemoryInMB(6000);
        boolean shouldDefaultEnable =
                RequestDesktopUtils.shouldDefaultEnableGlobalSetting(
                        RequestDesktopUtils
                                .DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                        mActivity);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled on devices below the "
                        + "memory threshold.",
                shouldDefaultEnable);
    }

    @Test
    public void testShouldDefaultEnableGlobalSetting_CustomScreenSizeThreshold() {
        boolean shouldDefaultEnable =
                RequestDesktopUtils.shouldDefaultEnableGlobalSetting(11.0, mActivity);
        Assert.assertTrue(
                "Desktop site global setting should be default-enabled on 10\"+ " + "devices.",
                shouldDefaultEnable);
    }

    @Test
    public void testShouldDefaultEnableGlobalSetting_WithSmallDisplay() {
        boolean shouldDefaultEnable =
                RequestDesktopUtils.shouldDefaultEnableGlobalSetting(7, mActivity);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled", shouldDefaultEnable);
    }

    @Test
    public void testShouldDefaultEnableGlobalSetting_ExternalDisplay() {
        when(mDisplay.getDisplayId()).thenReturn(/*non built-in display*/ 2);
        boolean shouldDefaultEnable =
                RequestDesktopUtils.shouldDefaultEnableGlobalSetting(
                        RequestDesktopUtils
                                .DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                        mActivity);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled on external display",
                shouldDefaultEnable);

        when(mDisplay.getDisplayId()).thenReturn(Display.DEFAULT_DISPLAY);
        shouldDefaultEnable =
                RequestDesktopUtils.shouldDefaultEnableGlobalSetting(
                        RequestDesktopUtils
                                .DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                        mActivity);
        Assert.assertTrue(
                "Desktop site global setting should be default-enabled on built-in display",
                shouldDefaultEnable);
    }

    @Test
    public void testShouldDefaultEnableGlobalSetting_UserPreviouslyUpdatedSetting() {
        // This SharedPreference key will ideally be updated when the user explicitly requests for
        // an update to the desktop site global setting.
        mSharedPreferencesManager.writeBoolean(
                SingleCategorySettingsConstants
                        .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY,
                true);
        boolean shouldDefaultEnable =
                RequestDesktopUtils.shouldDefaultEnableGlobalSetting(
                        RequestDesktopUtils
                                .DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                        mActivity);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled if it has been "
                        + "previously updated by the user.",
                shouldDefaultEnable);
    }

    @Test
    public void testMaybeDefaultEnableGlobalSetting() {
        boolean didDefaultEnable =
                RequestDesktopUtils.maybeDefaultEnableGlobalSetting(
                        RequestDesktopUtils
                                .DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                        mProfile,
                        mActivity);
        Assert.assertTrue(
                "Desktop site global setting should be default-enabled on big screen devices.",
                didDefaultEnable);
        Assert.assertEquals(
                "Desktop site content setting should be set correctly.",
                ContentSettingValues.ALLOW,
                mRdsDefaultValue);
        Assert.assertTrue(
                "SharedPreference DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING should be true.",
                mSharedPreferencesManager.contains(
                                ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING)
                        && mSharedPreferencesManager.readBoolean(
                                ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING,
                                false));

        // Verify that the desktop site global setting will be default-enabled at most once.
        boolean shouldDefaultEnable =
                RequestDesktopUtils.shouldDefaultEnableGlobalSetting(
                        RequestDesktopUtils
                                .DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                        mActivity);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled more than once.",
                shouldDefaultEnable);
    }

    @Test
    public void testMaybeShowDefaultEnableGlobalSettingMessage() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.REQUEST_DESKTOP_SITE_DEFAULT_ON_FEATURE))
                .thenReturn(true);

        // Default-enable the global setting before the message is shown.
        RequestDesktopUtils.maybeDefaultEnableGlobalSetting(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                mProfile,
                mActivity);

        when(mWebsitePreferenceBridgeJniMock.isContentSettingEnabled(
                        mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE))
                .thenReturn(true);
        RequestDesktopUtils.maybeShowDefaultEnableGlobalSettingMessage(
                mProfile, mMessageDispatcher, mActivity);

        ArgumentCaptor<PropertyModel> message = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMessageDispatcher).enqueueWindowScopedMessage(message.capture(), eq(false));
        Assert.assertEquals(
                "Message identifier should match.",
                MessageIdentifier.DESKTOP_SITE_GLOBAL_DEFAULT_OPT_OUT,
                message.getValue().get(MessageBannerProperties.MESSAGE_IDENTIFIER));
        Assert.assertEquals(
                "Message title should match.",
                mResources.getString(R.string.rds_global_default_on_message_title),
                message.getValue().get(MessageBannerProperties.TITLE));
        Assert.assertEquals(
                "Message primary button text should match.",
                mResources.getString(R.string.rds_global_default_on_message_button),
                message.getValue().get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));
        Assert.assertEquals(
                "Message icon resource ID should match.",
                R.drawable.ic_desktop_windows,
                message.getValue().get(MessageBannerProperties.ICON_RESOURCE_ID));
    }

    @Test
    public void testMaybeShowDefaultEnableGlobalSettingMessage_DoNotShowIfSettingIsDisabled() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.REQUEST_DESKTOP_SITE_DEFAULT_ON_FEATURE))
                .thenReturn(true);

        // Preference is set when the setting is default-enabled.
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING, true);

        // Simulate disabling of the setting by the user before the message is shown.
        when(mWebsitePreferenceBridgeJniMock.isContentSettingEnabled(
                        mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE))
                .thenReturn(false);

        boolean shown =
                RequestDesktopUtils.maybeShowDefaultEnableGlobalSettingMessage(
                        mProfile, mMessageDispatcher, mActivity);
        Assert.assertFalse(
                "Message should not be shown if the content setting is disabled.", shown);
    }

    @Test
    public void testMaybeShowDefaultEnableGlobalSettingMessage_DoNotShowIfDesktopAndroid() {
        BuildConfig.IS_DESKTOP_ANDROID = true;

        boolean shown =
                RequestDesktopUtils.maybeShowDefaultEnableGlobalSettingMessage(
                        mProfile, mMessageDispatcher, mActivity);
        Assert.assertFalse("Message should not be shown for desktop Android.", shown);
    }

    @Test
    public void testUpgradeTabLevelDesktopSiteSetting() {
        mRdsDefaultValue = ContentSettingValues.BLOCK;
        @TabUserAgent int tabUserAgent = TabUserAgent.DESKTOP;

        RequestDesktopUtils.maybeUpgradeTabLevelDesktopSiteSetting(
                mTab, mProfile, tabUserAgent, mGoogleUrl);

        Assert.assertEquals(
                "Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.ALLOW,
                mContentSettingMap.get(GOOGLE_COM).intValue());
        verify(mTab).setUserAgent(TabUserAgent.DEFAULT);
    }

    @Test
    public void testShouldApplyWindowSetting_IsAutomotive() {
        mShadowPackageManager.setSystemFeature(
                PackageManager.FEATURE_AUTOMOTIVE, /* supported= */ true);
        mWindowSetting = true;
        boolean shouldApplyWindowSetting =
                RequestDesktopUtils.shouldApplyWindowSetting(mProfile, mGoogleUrl, mActivity);
        Assert.assertFalse(
                "Desktop site window setting should not be applied on automotive",
                shouldApplyWindowSetting);
    }

    @Test
    public void testShouldApplyWindowSetting_SettingOff() {
        mWindowSetting = false;
        boolean shouldApplyWindowSetting =
                RequestDesktopUtils.shouldApplyWindowSetting(mProfile, mGoogleUrl, mActivity);
        Assert.assertFalse(
                "Desktop site window setting should not be applied when window setting is off",
                shouldApplyWindowSetting);
    }

    @Test
    public void testShouldApplyWindowSetting_isNotGlobalSetting() {
        mWindowSetting = true;
        ShadowTabUtils.setIsGlobalSetting(false);
        boolean shouldApplyWindowSetting =
                RequestDesktopUtils.shouldApplyWindowSetting(mProfile, mGoogleUrl, mActivity);
        Assert.assertFalse(
                "Desktop site window setting should not be applied when the current RDS setting "
                        + "is domain setting",
                shouldApplyWindowSetting);
    }

    @Test
    public void testShouldApplyWindowSetting_windowAttributesWidthValid() {
        mWindowSetting = true;
        ShadowTabUtils.setIsGlobalSetting(true);
        mLayoutParams.width = 800;
        boolean shouldApplyWindowSetting =
                RequestDesktopUtils.shouldApplyWindowSetting(mProfile, mGoogleUrl, mActivity);
        Assert.assertFalse(
                "Desktop site window setting should not be applied when window width in dp is "
                        + "larger than 600",
                shouldApplyWindowSetting);

        mLayoutParams.width = 400;
        shouldApplyWindowSetting =
                RequestDesktopUtils.shouldApplyWindowSetting(mProfile, mGoogleUrl, mActivity);
        Assert.assertTrue(
                "Desktop site window setting should be applied when window width in dp is "
                        + "smaller than 600",
                shouldApplyWindowSetting);
    }

    @Test
    public void testShouldApplyWindowSetting_windowAttributesWidthInvalid() {
        mWindowSetting = true;
        ShadowTabUtils.setIsGlobalSetting(true);
        mDisplayMetrics.density = 2.0f;
        mDisplayMetrics.widthPixels = 1600;
        boolean shouldApplyWindowSetting =
                RequestDesktopUtils.shouldApplyWindowSetting(mProfile, mGoogleUrl, mActivity);
        Assert.assertFalse(
                "Desktop site window setting should not be applied when window width in dp is "
                        + "larger than 600",
                shouldApplyWindowSetting);

        mDisplayMetrics.widthPixels = 1000;
        shouldApplyWindowSetting =
                RequestDesktopUtils.shouldApplyWindowSetting(mProfile, mGoogleUrl, mActivity);
        Assert.assertTrue(
                "Desktop site window setting should be applied when window width in dp is "
                        + "smaller than 600",
                shouldApplyWindowSetting);
    }

    @Test
    public void testMaybeDefaultEnableWindowSetting_PhoneSizedScreen() {
        mWindowSetting = false;
        mIsDefaultValuePreference = true;
        ShadowDisplayUtil.setCurrentSmallestScreenWidth(400);
        RequestDesktopUtils.maybeDefaultEnableWindowSetting(mActivity, mProfile);
        Assert.assertFalse(
                "Desktop site window setting should not be default enabled when the smallest "
                        + "screen width is less than 600dp",
                mWindowSetting);
    }

    @Test
    public void testMaybeDefaultEnableWindowSetting_DesktopAndroid() {
        mWindowSetting = false;
        mIsDefaultValuePreference = true;
        BuildConfig.IS_DESKTOP_ANDROID = true;
        RequestDesktopUtils.maybeDefaultEnableWindowSetting(mActivity, mProfile);
        Assert.assertFalse(
                "Desktop site window setting should not be default enabled for desktop "
                        + "Android",
                mWindowSetting);
    }

    @Test
    public void testMaybeDefaultEnableWindowSetting_ExternalDisplay() {
        mWindowSetting = false;
        mIsDefaultValuePreference = true;
        when(mDisplay.getDisplayId()).thenReturn(/*non built-in display*/ 2);
        RequestDesktopUtils.maybeDefaultEnableWindowSetting(mActivity, mProfile);
        Assert.assertFalse(
                "Desktop site window setting should not be default enabled when Chrome is opened "
                        + "on external display",
                mWindowSetting);
    }

    @Test
    public void testMaybeDefaultEnableWindowSetting_NotDefaultValuePreference() {
        mWindowSetting = false;
        mIsDefaultValuePreference = false;
        RequestDesktopUtils.maybeDefaultEnableWindowSetting(mActivity, mProfile);
        Assert.assertFalse(
                "Desktop site window setting should not be default enabled when the preference "
                        + "has been previously changed",
                mWindowSetting);
    }

    @Test
    public void testMaybeDefaultEnableWindowSetting_ShouldDefaultEnable() {
        mWindowSetting = false;
        mIsDefaultValuePreference = true;
        RequestDesktopUtils.maybeDefaultEnableWindowSetting(mActivity, mProfile);
        Assert.assertTrue("Desktop site window setting should be default enabled", mWindowSetting);
    }

    private Tab createTab() {
        return mock(Tab.class);
    }

    private void enableFeature(String featureName, boolean enable) {
        enableFeatureWithParams(featureName, null, enable);
    }

    private void enableFeatureWithParams(
            String featureName, Map<String, String> params, boolean enable) {
        mTestValues.addFeatureFlagOverride(featureName, enable);
        if (params != null) {
            for (Entry<String, String> param : params.entrySet()) {
                mTestValues.addFieldTrialParamOverride(
                        featureName, param.getKey(), param.getValue());
            }
        }
        FeatureList.setTestValues(mTestValues);
    }

    private void disableGlobalDefaultsExperimentFeatures() {
        enableFeatureWithParams("RequestDesktopSiteDefaults", null, false);
        enableFeatureWithParams("RequestDesktopSiteDefaultsControl", null, false);
        enableFeatureWithParams("RequestDesktopSiteDefaultsControlCohort1", null, false);
        enableFeatureWithParams("RequestDesktopSiteDefaultsEnabledCohort1", null, false);
        enableFeatureWithParams("RequestDesktopSiteDefaultsControlCohort2", null, false);
        enableFeatureWithParams("RequestDesktopSiteDefaultsEnabledCohort2", null, false);
    }
}
