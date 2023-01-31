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
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.os.Build;

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
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.RequestDesktopUtilsUnitTest.ShadowSysUtils;
import org.chromium.chrome.browser.tab.RequestDesktopUtilsUnitTest.ShadowUmaSessionStats;
import org.chromium.chrome.browser.tab.TabUtilsUnitTest.ShadowCriticalPersistedTabData;
import org.chromium.chrome.browser.tab.TabUtilsUnitTest.ShadowProfile;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings.SiteLayout;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettingsConstants;
import org.chromium.components.browser_ui.site_settings.SiteSettingsFeatureList;
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
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.ShadowGURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

/**
 * Unit tests for {@link RequestDesktopUtils}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowGURL.class, ShadowSysUtils.class, ShadowCriticalPersistedTabData.class,
                ShadowProfile.class, ShadowUmaSessionStats.class})
public class RequestDesktopUtilsUnitTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    /**
     * Shadows {@link SysUtils} class for testing.
     */
    @Implements(SysUtils.class)
    public static class ShadowSysUtils {
        private static boolean sLowEndDevice;
        private static int sMemoryInMB;

        public static void setLowEndDevice(boolean lowEndDevice) {
            sLowEndDevice = lowEndDevice;
        }

        public static void setMemoryInMB(int memoryInMB) {
            sMemoryInMB = memoryInMB;
        }

        @Implementation
        public static boolean isLowEndDevice() {
            return sLowEndDevice;
        }

        @Implementation
        public static int amountOfPhysicalMemoryKB() {
            return sMemoryInMB * ConversionUtils.KILOBYTES_PER_MEGABYTE;
        }
    }

    @Implements(UmaSessionStats.class)
    static class ShadowUmaSessionStats {
        private static boolean sMetricsServiceAvailable;

        public static void setMetricsServiceAvailable(boolean metricsServiceAvailable) {
            sMetricsServiceAvailable = metricsServiceAvailable;
        }

        public static void reset() {
            sMetricsServiceAvailable = false;
            sGlobalDefaultsExperimentTrialName = null;
            sGlobalDefaultsExperimentGroupName = null;
        }

        @Implementation
        public static boolean isMetricsServiceAvailable() {
            return sMetricsServiceAvailable;
        }

        @Implementation
        public static void registerSyntheticFieldTrial(String trialName, String groupName) {
            sGlobalDefaultsExperimentTrialName = trialName;
            sGlobalDefaultsExperimentGroupName = groupName;
        }
    }

    @Mock
    private WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;
    @Mock
    private MessageDispatcher mMessageDispatcher;
    @Mock
    private Activity mActivity;
    @Mock
    private Profile mProfile;
    @Mock
    private ModalDialogManager mModalDialogManager;
    @Mock
    private Tracker mTracker;
    @Mock
    private Tab mTab;
    @Mock
    private CriticalPersistedTabData mCriticalPersistedTabData;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private TabModel mRegularTabModel;
    @Mock
    private TabModel mIncognitoTabModel;
    @Mock
    private ObservableSupplier<Tab> mCurrentTabSupplier;

    private @ContentSettingValues int mRdsDefaultValue;
    private SharedPreferencesManager mSharedPreferencesManager;

    private final Map<String, Integer> mContentSettingMap = new HashMap<>();
    private final GURL mGoogleUrl = new GURL(JUnitTestGURLs.GOOGLE_URL);
    private final GURL mMapsUrl = new GURL(JUnitTestGURLs.MAPS_URL);

    private Resources mResources;

    private final TestValues mTestValues = new TestValues();

    private static final String ANY_SUBDOMAIN_PATTERN = "[*.]";
    private static final String GOOGLE_COM = "[*.]google.com/";
    private static String sGlobalDefaultsExperimentTrialName;
    private static String sGlobalDefaultsExperimentGroupName;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJniMock);
        ShadowCriticalPersistedTabData.setCriticalPersistedTabData(mCriticalPersistedTabData);
        ShadowProfile.setProfile(mProfile);
        ShadowUmaSessionStats.setMetricsServiceAvailable(true);

        doAnswer(invocation -> mRdsDefaultValue)
                .when(mWebsitePreferenceBridgeJniMock)
                .getDefaultContentSetting(any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE));

        doAnswer(invocation -> {
            mRdsDefaultValue = invocation.getArgument(2) ? ContentSettingValues.ALLOW
                                                         : ContentSettingValues.BLOCK;
            return null;
        })
                .when(mWebsitePreferenceBridgeJniMock)
                .setContentSettingEnabled(
                        any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE), anyBoolean());
        doAnswer(invocation -> mRdsDefaultValue == ContentSettingValues.ALLOW)
                .when(mWebsitePreferenceBridgeJniMock)
                .isContentSettingEnabled(any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE));

        doAnswer(invocation -> {
            mContentSettingMap.put(invocation.getArgument(2), invocation.getArgument(4));
            return null;
        })
                .when(mWebsitePreferenceBridgeJniMock)
                .setContentSettingCustomScope(any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE),
                        anyString(), anyString(), anyInt());
        doAnswer(invocation -> toDomainWildcardPattern(invocation.getArgument(0)))
                .when(mWebsitePreferenceBridgeJniMock)
                .toDomainWildcardPattern(anyString());

        mSharedPreferencesManager = SharedPreferencesManager.getInstance();
        mSharedPreferencesManager.disableKeyCheckerForTesting();

        mResources = ApplicationProvider.getApplicationContext().getResources();
        mResources.getConfiguration().smallestScreenWidthDp = 600;
        when(mActivity.getResources()).thenReturn(mResources);

        when(mTabModelSelector.getModels())
                .thenReturn(Arrays.asList(mRegularTabModel, mIncognitoTabModel));
        when(mTabModelSelector.getCurrentModel()).thenReturn(mRegularTabModel);
        when(mRegularTabModel.getProfile()).thenReturn(mProfile);

        TrackerFactory.setTrackerForTests(mTracker);
        disableGlobalDefaultsExperimentFeatures();

        ShadowSysUtils.setMemoryInMB(2048);
    }

    @After
    public void tearDown() {
        FeatureList.setTestValues(null);
        ShadowSysUtils.setLowEndDevice(false);
        ShadowCriticalPersistedTabData.reset();
        ShadowProfile.reset();
        ShadowUmaSessionStats.reset();
        mSharedPreferencesManager.removeKey(
                ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING);
        mSharedPreferencesManager.removeKey(
                SingleCategorySettingsConstants
                        .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY);
        mSharedPreferencesManager.removeKey(
                ChromePreferenceKeys.DESKTOP_SITE_EXCEPTIONS_DOWNGRADE_TAB_SETTING_SET);
        mSharedPreferencesManager.removeKey(
                ChromePreferenceKeys.DESKTOP_SITE_EXCEPTIONS_DOWNGRADE_GLOBAL_SETTING_ENABLED);
        TrackerFactory.setTrackerForTests(null);
    }

    @Test
    public void testSetRequestDesktopSiteContentSettingsForUrl_DefaultBlock_SiteBlock() {
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSettingValues.BLOCK;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSettingValues.BLOCK);
        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, true);
        Assert.assertEquals("Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.ALLOW, mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals("Request Desktop Site subdomain level setting should be removed.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, false);
        Assert.assertEquals("Request Desktop Site domain level setting should be removed.",
                ContentSettingValues.DEFAULT, mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void testSetRequestDesktopSiteContentSettingsForUrl_DefaultBlock_SiteAllow() {
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSettingValues.BLOCK;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSettingValues.ALLOW);
        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, false);
        Assert.assertEquals("Request Desktop Site domain level setting should be removed.",
                ContentSettingValues.DEFAULT, mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals("Request Desktop Site subdomain level setting should be removed.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, true);
        Assert.assertEquals("Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.ALLOW, mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void testSetRequestDesktopSiteContentSettingsForUrl_DefaultAllow_SiteAllow() {
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSettingValues.ALLOW;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSettingValues.ALLOW);
        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, false);
        Assert.assertEquals("Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.BLOCK, mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals("Request Desktop Site subdomain level setting should be removed.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, true);
        Assert.assertEquals("Request Desktop Site domain level setting should be removed.",
                ContentSettingValues.DEFAULT, mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void testSetRequestDesktopSiteContentSettingsForUrl_DefaultAllow_SiteBlock() {
        // Regular profile type.
        when(mProfile.isOffTheRecord()).thenReturn(false);
        mRdsDefaultValue = ContentSettingValues.ALLOW;
        // Pre-existing subdomain setting.
        mContentSettingMap.put(mGoogleUrl.getHost(), ContentSettingValues.BLOCK);
        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, true);
        Assert.assertEquals("Request Desktop Site domain level setting should be removed.",
                ContentSettingValues.DEFAULT, mContentSettingMap.get(GOOGLE_COM).intValue());
        Assert.assertEquals("Request Desktop Site subdomain level setting should be removed.",
                ContentSettingValues.DEFAULT,
                mContentSettingMap.get(mGoogleUrl.getHost()).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, false);
        Assert.assertEquals("Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.BLOCK, mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void testSetRequestDesktopSiteContentSettingsForUrl_DefaultBlock_Incognito() {
        // Incognito profile type.
        when(mProfile.isOffTheRecord()).thenReturn(true);
        when(mProfile.isPrimaryOTRProfile()).thenReturn(true);
        mRdsDefaultValue = ContentSettingValues.BLOCK;

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, true);
        Assert.assertEquals("Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.ALLOW, mContentSettingMap.get(GOOGLE_COM).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, false);
        Assert.assertEquals("Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.BLOCK, mContentSettingMap.get(GOOGLE_COM).intValue());
    }

    @Test
    public void testSetRequestDesktopSiteContentSettingsForUrl_DefaultAllow_Incognito() {
        // Incognito profile type.
        when(mProfile.isOffTheRecord()).thenReturn(true);
        when(mProfile.isPrimaryOTRProfile()).thenReturn(true);
        mRdsDefaultValue = ContentSettingValues.ALLOW;

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mGoogleUrl, false);
        Assert.assertEquals("Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.BLOCK, mContentSettingMap.get(GOOGLE_COM).intValue());

        RequestDesktopUtils.setRequestDesktopSiteContentSettingsForUrl(mProfile, mMapsUrl, true);
        Assert.assertEquals("Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.ALLOW, mContentSettingMap.get(GOOGLE_COM).intValue());
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
    public void testShouldDefaultEnableGlobalSetting_DisableOnLowEndDevice() {
        Map<String, String> params = new HashMap<>();
        params.put(RequestDesktopUtils.PARAM_GLOBAL_SETTING_DEFAULT_ON_ON_LOW_END_DEVICES, "false");
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, params, true);
        ShadowSysUtils.setLowEndDevice(true);
        boolean shouldDefaultEnable = RequestDesktopUtils.shouldDefaultEnableGlobalSetting(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                mActivity);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled on low memory devices.",
                shouldDefaultEnable);
    }

    @Test
    public void testShouldDefaultEnableGlobalSetting_MemoryThreshold() {
        Map<String, String> params = new HashMap<>();
        params.put(RequestDesktopUtils.PARAM_GLOBAL_SETTING_DEFAULT_ON_MEMORY_LIMIT, "4000");
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, params, true);
        boolean shouldDefaultEnable = RequestDesktopUtils.shouldDefaultEnableGlobalSetting(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                mActivity);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled on devices below the memory threshold.",
                shouldDefaultEnable);
    }

    @Test
    public void testShouldDefaultEnableGlobalSetting_CustomScreenSizeThreshold() {
        Map<String, String> params = new HashMap<>();
        params.put(
                RequestDesktopUtils.PARAM_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                "10.0");
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, params, true);
        boolean shouldDefaultEnable =
                RequestDesktopUtils.shouldDefaultEnableGlobalSetting(11.0, mActivity);
        Assert.assertTrue("Desktop site global setting should be default-enabled on 10\"+ devices.",
                shouldDefaultEnable);
    }

    @Test
    public void testShouldDefaultEnableGlobalSetting_UserPreviouslyUpdatedSetting() {
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, null, true);
        // This SharedPreference key will ideally be updated when the user explicitly requests for
        // an update to the desktop site global setting.
        mSharedPreferencesManager.writeBoolean(
                SingleCategorySettingsConstants
                        .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY,
                true);
        boolean shouldDefaultEnable = RequestDesktopUtils.shouldDefaultEnableGlobalSetting(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                mActivity);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled if it has been previously updated by the user.",
                shouldDefaultEnable);
    }

    @Test
    public void testShouldDefaultEnableGlobalSetting_ExperimentControlGroup() {
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, null, false);
        enableFeatureWithParams(
                ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS_CONTROL, null, true);
        boolean shouldDefaultEnable = RequestDesktopUtils.shouldDefaultEnableGlobalSetting(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                mActivity);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled in the control experiment group.",
                shouldDefaultEnable);
        Assert.assertTrue(
                "SharedPreference DEFAULT_ENABLE_DESKTOP_SITE_GLOBAL_SETTING_COHORT should be true.",
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.DEFAULT_ENABLE_DESKTOP_SITE_GLOBAL_SETTING_COHORT)
                        && mSharedPreferencesManager.readBoolean(
                                ChromePreferenceKeys
                                        .DEFAULT_ENABLE_DESKTOP_SITE_GLOBAL_SETTING_COHORT,
                                false));
    }

    @Test
    public void testMaybeRegisterSyntheticFieldTrials_DefaultOnEnabled12Inches() {
        RequestDesktopUtils.maybeRegisterSyntheticFieldTrials(false, 12.0, 0, false);
        Assert.assertEquals("Trial name is incorrect.", "RequestDesktopSiteDefaultsSynthetic",
                sGlobalDefaultsExperimentTrialName);
        Assert.assertEquals("Group name is incorrect.", "DefaultOn_12_0_Enabled",
                sGlobalDefaultsExperimentGroupName);
    }

    @Test
    public void testMaybeRegisterSyntheticFieldTrials_DefaultOnEnabled12Inches_WithCohortId() {
        RequestDesktopUtils.maybeRegisterSyntheticFieldTrials(false, 12.0, 2, false);
        Assert.assertEquals("Trial name is incorrect.", "RequestDesktopSiteDefaultsCohort2",
                sGlobalDefaultsExperimentTrialName);
        Assert.assertEquals(
                "Group name is incorrect.", "DefaultOn_12_0_2", sGlobalDefaultsExperimentGroupName);
    }

    @Test
    public void testMaybeRegisterSyntheticFieldTrials_DefaultOnControl12Inches() {
        RequestDesktopUtils.maybeRegisterSyntheticFieldTrials(true, 12.0, 0, false);
        Assert.assertEquals("Trial name is incorrect.",
                "RequestDesktopSiteDefaultsControlSynthetic", sGlobalDefaultsExperimentTrialName);
        Assert.assertEquals("Group name is incorrect.", "DefaultOn_12_0_Control",
                sGlobalDefaultsExperimentGroupName);
    }

    @Test
    public void testMaybeRegisterSyntheticFieldTrials_DefaultOnControl12Inches_WithCohortId() {
        RequestDesktopUtils.maybeRegisterSyntheticFieldTrials(true, 12.0, 2, false);
        Assert.assertEquals("Trial name is incorrect.", "RequestDesktopSiteDefaultsCohort2",
                sGlobalDefaultsExperimentTrialName);
        Assert.assertEquals(
                "Group name is incorrect.", "DefaultOn_12_0_2", sGlobalDefaultsExperimentGroupName);
    }

    @Test
    public void testMaybeRegisterSyntheticFieldTrials_OptInEnabled10Inches() {
        RequestDesktopUtils.maybeRegisterSyntheticFieldTrials(false, 10.0, 0, true);
        Assert.assertEquals("Trial name is incorrect.", "RequestDesktopSiteOptInSynthetic",
                sGlobalDefaultsExperimentTrialName);
        Assert.assertEquals("Group name is incorrect.", "OptIn_10_0_Enabled",
                sGlobalDefaultsExperimentGroupName);
    }

    @Test
    public void testMaybeRegisterSyntheticFieldTrials_DoNothingWhenExperimentIsActive() {
        enableFeatureWithParams("RequestDesktopSiteDefaultsSynthetic", null, true);
        RequestDesktopUtils.maybeRegisterSyntheticFieldTrials(false, 12.0, 0, false);
        Assert.assertTrue("Synthetic trial should not be registered.",
                sGlobalDefaultsExperimentTrialName == null
                        && sGlobalDefaultsExperimentGroupName == null);
    }

    @Test
    public void testMaybeRegisterSyntheticFieldTrials_ExperimentIsActive_WithCohortId() {
        enableFeatureWithParams("RequestDesktopSiteDefaultsEnabledCohort2", null, true);
        RequestDesktopUtils.maybeRegisterSyntheticFieldTrials(false, 12.0, 2, false);
        Assert.assertEquals("Trial name is incorrect.", "RequestDesktopSiteDefaultsCohort2",
                sGlobalDefaultsExperimentTrialName);
        Assert.assertEquals(
                "Group name is incorrect.", "DefaultOn_12_0_2", sGlobalDefaultsExperimentGroupName);
    }

    @Test
    public void testMaybeDefaultEnableGlobalSetting() {
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, null, true);
        boolean didDefaultEnable = RequestDesktopUtils.maybeDefaultEnableGlobalSetting(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                mProfile, mActivity);
        Assert.assertTrue(
                "Desktop site global setting should be default-enabled on big screen devices.",
                didDefaultEnable);
        Assert.assertEquals("Desktop site content setting should be set correctly.",
                ContentSettingValues.ALLOW, mRdsDefaultValue);
        Assert.assertTrue(
                "SharedPreference DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING should be true.",
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING)
                        && mSharedPreferencesManager.readBoolean(
                                ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING,
                                false));
        Assert.assertTrue(
                "SharedPreference DEFAULT_ENABLE_DESKTOP_SITE_GLOBAL_SETTING_COHORT should be true.",
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.DEFAULT_ENABLE_DESKTOP_SITE_GLOBAL_SETTING_COHORT)
                        && mSharedPreferencesManager.readBoolean(
                                ChromePreferenceKeys
                                        .DEFAULT_ENABLE_DESKTOP_SITE_GLOBAL_SETTING_COHORT,
                                false));

        // Verify that the desktop site global setting will be default-enabled at most once.
        boolean shouldDefaultEnable = RequestDesktopUtils.shouldDefaultEnableGlobalSetting(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                mActivity);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled more than once.",
                shouldDefaultEnable);
    }

    @Test
    public void testMaybeDefaultEnableGlobalSetting_DoNotEnableOnOptInEnabled() {
        Map<String, String> params = new HashMap<>();
        params.put(RequestDesktopUtils.PARAM_GLOBAL_SETTING_OPT_IN_ENABLED, "true");
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, params, true);

        boolean didDefaultEnable = RequestDesktopUtils.maybeDefaultEnableGlobalSetting(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                mProfile, mActivity);
        Assert.assertFalse(
                "Desktop site global setting should not be default-enabled when opt-in is enabled.",
                didDefaultEnable);
    }

    @Test
    public void testMaybeShowDefaultEnableGlobalSettingMessage() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.REQUEST_DESKTOP_SITE_DEFAULT_ON_FEATURE))
                .thenReturn(true);
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, null, true);

        // Default-enable the global setting before the message is shown.
        RequestDesktopUtils.maybeDefaultEnableGlobalSetting(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                mProfile, mActivity);

        when(mWebsitePreferenceBridgeJniMock.isContentSettingEnabled(
                     mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE))
                .thenReturn(true);
        RequestDesktopUtils.maybeShowDefaultEnableGlobalSettingMessage(
                mProfile, mMessageDispatcher, mActivity);

        ArgumentCaptor<PropertyModel> message = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMessageDispatcher).enqueueWindowScopedMessage(message.capture(), eq(false));
        Assert.assertEquals("Message identifier should match.",
                MessageIdentifier.DESKTOP_SITE_GLOBAL_DEFAULT_OPT_OUT,
                message.getValue().get(MessageBannerProperties.MESSAGE_IDENTIFIER));
        Assert.assertEquals("Message title should match.",
                mResources.getString(R.string.rds_global_default_on_message_title),
                message.getValue().get(MessageBannerProperties.TITLE));
        Assert.assertEquals("Message primary button text should match.",
                mResources.getString(R.string.rds_global_default_on_message_button),
                message.getValue().get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));
        Assert.assertEquals("Message icon resource ID should match.", R.drawable.ic_desktop_windows,
                message.getValue().get(MessageBannerProperties.ICON_RESOURCE_ID));
    }

    @Test
    public void testMaybeShowDefaultEnableGlobalSettingMessage_DoNotShowIfSettingIsDisabled() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.REQUEST_DESKTOP_SITE_DEFAULT_ON_FEATURE))
                .thenReturn(true);
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, null, true);

        // Preference is set when the setting is default-enabled.
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING, true);

        // Simulate disabling of the setting by the user before the message is shown.
        when(mWebsitePreferenceBridgeJniMock.isContentSettingEnabled(
                     mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE))
                .thenReturn(false);

        boolean shown = RequestDesktopUtils.maybeShowDefaultEnableGlobalSettingMessage(
                mProfile, mMessageDispatcher, mActivity);
        Assert.assertFalse(
                "Message should not be shown if the content setting is disabled.", shown);
    }

    @Test
    public void testMaybeDisableGlobalSetting() {
        // Default-enable the global setting.
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, null, true);
        RequestDesktopUtils.maybeDefaultEnableGlobalSetting(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                mProfile, mActivity);

        // Disable REQUEST_DESKTOP_SITE_DEFAULTS and initiate downgrade.
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, null, false);
        enableFeatureWithParams(
                ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS_DOWNGRADE, null, true);
        boolean didDisable = RequestDesktopUtils.maybeDisableGlobalSetting(mProfile);

        Assert.assertTrue(
                "Desktop site global setting should be disabled on downgrade.", didDisable);
        Assert.assertEquals("Desktop site content setting should be set correctly.",
                ContentSettingValues.BLOCK, mRdsDefaultValue);
        Assert.assertFalse(
                "SharedPreference DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING should be removed.",
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING));
        Assert.assertFalse(
                "SharedPreference DEFAULT_ENABLE_DESKTOP_SITE_GLOBAL_SETTING_COHORT should be removed.",
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.DEFAULT_ENABLE_DESKTOP_SITE_GLOBAL_SETTING_COHORT));
    }

    @Test
    public void testMaybeDisableGlobalSetting_UserUpdatedSetting() {
        // Default-enable the global setting.
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, null, true);
        RequestDesktopUtils.maybeDefaultEnableGlobalSetting(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                mProfile, mActivity);

        // This SharedPreference key will ideally be updated when the user explicitly requests for
        // an update to the desktop site global setting. Simulate a scenario where the user turns
        // the setting off after it is default-enabled and turns it on again.
        mSharedPreferencesManager.writeBoolean(
                SingleCategorySettingsConstants
                        .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY,
                false);
        mSharedPreferencesManager.writeBoolean(
                SingleCategorySettingsConstants
                        .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY,
                true);

        // Disable REQUEST_DESKTOP_SITE_DEFAULTS and initiate downgrade.
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, null, false);
        enableFeatureWithParams(
                ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS_DOWNGRADE, null, true);
        boolean didDisable = RequestDesktopUtils.maybeDisableGlobalSetting(mProfile);

        Assert.assertFalse(
                "Desktop site global setting should not be disabled on downgrade if the user updated the setting.",
                didDisable);
        Assert.assertEquals("Desktop site content setting should be set correctly.",
                ContentSettingValues.ALLOW, mRdsDefaultValue);
        Assert.assertFalse(
                "SharedPreference DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING should be removed.",
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING));
        Assert.assertFalse(
                "SharedPreference DEFAULT_ENABLE_DESKTOP_SITE_GLOBAL_SETTING_COHORT should be removed.",
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.DEFAULT_ENABLE_DESKTOP_SITE_GLOBAL_SETTING_COHORT));
    }

    @Test
    public void testMaybeDisableGlobalSetting_FinchParamChanged_Memory() {
        // Default-enable the global setting.
        Map<String, String> params = new HashMap<>();
        params.put(RequestDesktopUtils.PARAM_GLOBAL_SETTING_DEFAULT_ON_MEMORY_LIMIT, "1000");
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, params, true);
        RequestDesktopUtils.maybeDefaultEnableGlobalSetting(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                mProfile, mActivity);

        // Update finch param and initiate downgrade.
        params.put(RequestDesktopUtils.PARAM_GLOBAL_SETTING_DEFAULT_ON_MEMORY_LIMIT, "4000");
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, params, true);
        RequestDesktopUtils.maybeDefaultEnableGlobalSetting(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                mProfile, mActivity);
        enableFeatureWithParams(
                ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS_DOWNGRADE, null, true);
        boolean didDisable = RequestDesktopUtils.maybeDisableGlobalSetting(mProfile);

        Assert.assertTrue(
                "Desktop site global setting should be disabled on downgrade.", didDisable);
        Assert.assertEquals("Desktop site content setting should be set correctly.",
                ContentSettingValues.BLOCK, mRdsDefaultValue);
        Assert.assertFalse(
                "SharedPreference DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING should be removed.",
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING));
        Assert.assertFalse(
                "SharedPreference DEFAULT_ENABLE_DESKTOP_SITE_GLOBAL_SETTING_COHORT should be removed.",
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.DEFAULT_ENABLE_DESKTOP_SITE_GLOBAL_SETTING_COHORT));
    }

    @Test
    public void testMaybeDisableGlobalSetting_FinchParamChanged_CPUArch() {
        // Default-enable the global setting.
        Map<String, String> params = new HashMap<>();
        params.put(RequestDesktopUtils.PARAM_GLOBAL_SETTING_DEFAULT_ON_ON_X86_DEVICES, "true");
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, params, true);
        String[] originalAbis = Build.SUPPORTED_ABIS;
        try {
            ReflectionHelpers.setStaticField(
                    Build.class, "SUPPORTED_ABIS", new String[] {"x86", "armeabi-v7a"});
            RequestDesktopUtils.maybeDefaultEnableGlobalSetting(
                    RequestDesktopUtils
                            .DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                    mProfile, mActivity);

            // Update finch param and initiate downgrade.
            params.put(RequestDesktopUtils.PARAM_GLOBAL_SETTING_DEFAULT_ON_ON_X86_DEVICES, "false");
            enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, params, true);
            RequestDesktopUtils.maybeDefaultEnableGlobalSetting(
                    RequestDesktopUtils
                            .DEFAULT_GLOBAL_SETTING_DEFAULT_ON_DISPLAY_SIZE_THRESHOLD_INCHES,
                    mProfile, mActivity);
            enableFeatureWithParams(
                    ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS_DOWNGRADE, null, true);
            boolean didDisable = RequestDesktopUtils.maybeDisableGlobalSetting(mProfile);

            Assert.assertTrue(
                    "Desktop site global setting should be disabled on downgrade.", didDisable);
            Assert.assertEquals("Desktop site content setting should be set correctly.",
                    ContentSettingValues.BLOCK, mRdsDefaultValue);
            Assert.assertFalse(
                    "SharedPreference DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING should be removed.",
                    mSharedPreferencesManager.contains(
                            ChromePreferenceKeys.DEFAULT_ENABLED_DESKTOP_SITE_GLOBAL_SETTING));
            Assert.assertFalse(
                    "SharedPreference DEFAULT_ENABLE_DESKTOP_SITE_GLOBAL_SETTING_COHORT should be removed.",
                    mSharedPreferencesManager.contains(
                            ChromePreferenceKeys
                                    .DEFAULT_ENABLE_DESKTOP_SITE_GLOBAL_SETTING_COHORT));
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "SUPPORTED_ABIS", originalAbis);
        }
    }

    @Test
    public void testShouldShowGlobalSettingOptInMessage_ExperimentControlGroup() {
        when(mTracker.wouldTriggerHelpUI(FeatureConstants.REQUEST_DESKTOP_SITE_OPT_IN_FEATURE))
                .thenReturn(true);
        Map<String, String> params = new HashMap<>();
        params.put(RequestDesktopUtils.PARAM_GLOBAL_SETTING_OPT_IN_ENABLED, "true");
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, null, false);
        enableFeatureWithParams(
                ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS_CONTROL, params, true);

        boolean shouldShowOptIn = RequestDesktopUtils.shouldShowGlobalSettingOptInMessage(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_OPT_IN_DISPLAY_SIZE_MIN_THRESHOLD_INCHES,
                mProfile, mActivity);
        Assert.assertFalse(
                "Opt-in message for desktop site global setting should not be shown in the control experiment group.",
                shouldShowOptIn);
        Assert.assertTrue(
                "SharedPreference DESKTOP_SITE_GLOBAL_SETTING_OPT_IN_MESSAGE_COHORT should be true.",
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.DESKTOP_SITE_GLOBAL_SETTING_OPT_IN_MESSAGE_COHORT)
                        && mSharedPreferencesManager.readBoolean(
                                ChromePreferenceKeys
                                        .DESKTOP_SITE_GLOBAL_SETTING_OPT_IN_MESSAGE_COHORT,
                                false));
    }

    @Test
    public void testMaybeShowGlobalSettingOptInMessage() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.REQUEST_DESKTOP_SITE_OPT_IN_FEATURE))
                .thenReturn(true);
        when(mTracker.wouldTriggerHelpUI(FeatureConstants.REQUEST_DESKTOP_SITE_OPT_IN_FEATURE))
                .thenReturn(true);
        Map<String, String> params = new HashMap<>();
        params.put(RequestDesktopUtils.PARAM_GLOBAL_SETTING_OPT_IN_ENABLED, "true");
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, params, true);

        boolean shown = RequestDesktopUtils.maybeShowGlobalSettingOptInMessage(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_OPT_IN_DISPLAY_SIZE_MIN_THRESHOLD_INCHES,
                mProfile, mMessageDispatcher, mActivity, mCurrentTabSupplier);
        Assert.assertTrue("Desktop site global setting opt-in message should be shown.", shown);

        ArgumentCaptor<PropertyModel> message = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMessageDispatcher).enqueueWindowScopedMessage(message.capture(), eq(false));
        Assert.assertEquals("Message identifier should match.",
                MessageIdentifier.DESKTOP_SITE_GLOBAL_OPT_IN,
                message.getValue().get(MessageBannerProperties.MESSAGE_IDENTIFIER));
        Assert.assertEquals("Message title should match.",
                mResources.getString(R.string.rds_global_opt_in_message_title),
                message.getValue().get(MessageBannerProperties.TITLE));
        Assert.assertEquals("Message primary button text should match.",
                mResources.getString(R.string.yes),
                message.getValue().get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));
        Assert.assertEquals("Message icon resource ID should match.", R.drawable.ic_desktop_windows,
                message.getValue().get(MessageBannerProperties.ICON_RESOURCE_ID));
        Assert.assertTrue(
                "SharedPreference DESKTOP_SITE_GLOBAL_SETTING_OPT_IN_MESSAGE_COHORT should be true.",
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.DESKTOP_SITE_GLOBAL_SETTING_OPT_IN_MESSAGE_COHORT,
                        false));
    }

    @Test
    public void testMaybeShowGlobalSettingOptInMessage_DoNotShowIfSettingIsEnabled() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.REQUEST_DESKTOP_SITE_OPT_IN_FEATURE))
                .thenReturn(true);
        when(mTracker.wouldTriggerHelpUI(FeatureConstants.REQUEST_DESKTOP_SITE_OPT_IN_FEATURE))
                .thenReturn(true);
        Map<String, String> params = new HashMap<>();
        params.put(RequestDesktopUtils.PARAM_GLOBAL_SETTING_OPT_IN_ENABLED, "true");
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, params, true);

        when(mWebsitePreferenceBridgeJniMock.isContentSettingEnabled(
                     mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE))
                .thenReturn(true);

        boolean shown = RequestDesktopUtils.maybeShowGlobalSettingOptInMessage(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_OPT_IN_DISPLAY_SIZE_MIN_THRESHOLD_INCHES,
                mProfile, mMessageDispatcher, mActivity, mCurrentTabSupplier);
        Assert.assertFalse(
                "Desktop site global setting opt-in message should not be shown when the setting is already enabled.",
                shown);
    }

    @Test
    public void testMaybeShowGlobalSettingOptInMessage_MemoryThreshold() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.REQUEST_DESKTOP_SITE_OPT_IN_FEATURE))
                .thenReturn(true);
        when(mTracker.wouldTriggerHelpUI(FeatureConstants.REQUEST_DESKTOP_SITE_OPT_IN_FEATURE))
                .thenReturn(true);
        Map<String, String> params = new HashMap<>();
        params.put(RequestDesktopUtils.PARAM_GLOBAL_SETTING_OPT_IN_ENABLED, "true");
        params.put(RequestDesktopUtils.PARAM_GLOBAL_SETTING_OPT_IN_MEMORY_LIMIT, "4000");
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, params, true);

        boolean shown = RequestDesktopUtils.maybeShowGlobalSettingOptInMessage(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_OPT_IN_DISPLAY_SIZE_MIN_THRESHOLD_INCHES,
                mProfile, mMessageDispatcher, mActivity, mCurrentTabSupplier);
        Assert.assertFalse(
                "Desktop site global setting opt-in message should not be shown on devices below the memory threshold.",
                shown);
    }

    @Test
    public void testUpdateDesktopSiteGlobalSettingOnUserRequest_DesktopSite() {
        RequestDesktopUtils.updateDesktopSiteGlobalSettingOnUserRequest(mProfile, true);
        verifyUpdateDesktopSiteGlobalSettingOnUserRequest(true);
    }

    @Test
    public void testUpdateDesktopSiteGlobalSettingOnUserRequest_MobileSite() {
        RequestDesktopUtils.updateDesktopSiteGlobalSettingOnUserRequest(mProfile, false);
        verifyUpdateDesktopSiteGlobalSettingOnUserRequest(false);
    }

    @Test
    public void testMaybeShowUserEducationPromptForAppMenuSelection() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.REQUEST_DESKTOP_SITE_APP_MENU_FEATURE))
                .thenReturn(true);
        boolean shown = RequestDesktopUtils.maybeShowUserEducationPromptForAppMenuSelection(
                mProfile, mActivity, mModalDialogManager);
        Assert.assertTrue("User education prompt should be shown.", shown);
        ArgumentCaptor<PropertyModel> dialog = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(dialog.capture(), eq(ModalDialogType.APP), eq(true));
        Assert.assertEquals("Dialog title should match.",
                mResources.getString(R.string.rds_app_menu_user_education_dialog_title),
                dialog.getValue().get(ModalDialogProperties.TITLE));
        Assert.assertEquals("Dialog message should match.",
                mResources.getString(R.string.rds_app_menu_user_education_dialog_message),
                dialog.getValue().get(ModalDialogProperties.MESSAGE_PARAGRAPH_1));
        Assert.assertEquals("Dialog button text should match.",
                mResources.getString(R.string.got_it),
                dialog.getValue().get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        Assert.assertTrue("Dialog should be dismissed on touch outside.",
                dialog.getValue().get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));

        // Verify that the button click dismisses the dialog.
        dialog.getValue()
                .get(ModalDialogProperties.CONTROLLER)
                .onClick(dialog.getValue(), ButtonType.POSITIVE);
        verify(mModalDialogManager)
                .dismissDialog(
                        eq(dialog.getValue()), eq(DialogDismissalCause.POSITIVE_BUTTON_CLICKED));

        // Verify that dialog dismissal dismisses the feature in the tracker.
        dialog.getValue()
                .get(ModalDialogProperties.CONTROLLER)
                .onDismiss(dialog.getValue(), DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        verify(mTracker).dismissed(FeatureConstants.REQUEST_DESKTOP_SITE_APP_MENU_FEATURE);
    }

    @Test
    public void testUpgradeTabLevelDesktopSiteSetting() {
        mRdsDefaultValue = ContentSettingValues.BLOCK;
        @TabUserAgent
        int tabUserAgent = TabUserAgent.DESKTOP;

        enableFeature(ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS, true);

        RequestDesktopUtils.maybeUpgradeTabLevelDesktopSiteSetting(
                mTab, mProfile, tabUserAgent, mGoogleUrl);

        Assert.assertEquals("Request Desktop Site domain level setting is not set correctly.",
                ContentSettingValues.ALLOW, mContentSettingMap.get(GOOGLE_COM).intValue());
        verify(mCriticalPersistedTabData).setUserAgent(TabUserAgent.DEFAULT);
    }

    @Test
    public void testDowngradeSiteExceptions_NotMoreThanOnce() {
        // Test downgrade path.
        testSiteExceptionsDowngradePath();

        // Attempt to run downgrade again.
        RequestDesktopUtils.maybeDowngradeSiteSettings(mTabModelSelector);
        Assert.assertTrue("SharedPreferences string set should not be populated.",
                mSharedPreferencesManager
                        .readStringSet(ChromePreferenceKeys
                                               .DESKTOP_SITE_EXCEPTIONS_DOWNGRADE_TAB_SETTING_SET)
                        .isEmpty());
    }

    @Test
    public void testDowngradeSiteExceptions_ClearSharedPrefs() {
        testSiteExceptionsDowngradePath();

        // Downgrade SharedPreferences keys should be removed when exceptions are re-enabled.
        enableFeature(ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS, true);
        RequestDesktopUtils.maybeDowngradeSiteSettings(mTabModelSelector);
        Assert.assertTrue("SharedPreferences keys should be removed.",
                !mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.DESKTOP_SITE_EXCEPTIONS_DOWNGRADE_TAB_SETTING_SET)
                        && !mSharedPreferencesManager.contains(
                                ChromePreferenceKeys
                                        .DESKTOP_SITE_EXCEPTIONS_DOWNGRADE_GLOBAL_SETTING_ENABLED));
    }

    @Test
    public void testDowngradeSiteExceptions_GlobalSettingChanged() {
        mRdsDefaultValue = ContentSettingValues.BLOCK;
        enableFeature(ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS, false);
        enableFeature(SiteSettingsFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS_DOWNGRADE, true);

        int tabCount = 2;
        List<Tab> regularTabs = createTabsForDesktopSiteExceptionsDowngradeTest(
                mRegularTabModel, -1, tabCount, true, false);

        when(mRegularTabModel.getCount()).thenReturn(tabCount);
        RequestDesktopUtils.maybeDowngradeSiteSettings(mTabModelSelector);

        // Simulate loading of tab 0 first, to downgrade.
        RequestDesktopUtils.maybeRestoreUserAgentOnSiteSettingsDowngrade(regularTabs.get(0));

        // Simulate an update to the global setting before loading tab 1 for downgrade.
        mRdsDefaultValue = ContentSettingValues.ALLOW;
        RequestDesktopUtils.maybeRestoreUserAgentOnSiteSettingsDowngrade(regularTabs.get(1));

        // DESKTOP user agent should be applied to tab 0, tab 1's TabUserAgent should not be
        // updated.
        verify(mCriticalPersistedTabData).setUserAgent(TabUserAgent.DESKTOP);
        verify(mCriticalPersistedTabData, never()).setUserAgent(TabUserAgent.MOBILE);
        verify(mCriticalPersistedTabData, never()).setUserAgent(TabUserAgent.DEFAULT);
    }

    // Tests the fix for crash crbug.com/1381841. When the global setting opt-in message is clicked,
    // the current activity tab should be reloaded to use the desktop UA. This tab might not
    // necessarily be the tab on which the message was shown, which could be destroyed by the time
    // the message is clicked on.
    @Test
    public void testGlobalSettingOptInMessageClickedOnDifferentTab() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.REQUEST_DESKTOP_SITE_OPT_IN_FEATURE))
                .thenReturn(true);
        when(mTracker.wouldTriggerHelpUI(FeatureConstants.REQUEST_DESKTOP_SITE_OPT_IN_FEATURE))
                .thenReturn(true);
        Map<String, String> params = new HashMap<>();
        params.put(RequestDesktopUtils.PARAM_GLOBAL_SETTING_OPT_IN_ENABLED, "true");
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_DEFAULTS, params, true);

        // Simulate showing the message on `shownTab`.
        Tab shownTab = mock(Tab.class);
        when(mCurrentTabSupplier.get()).thenReturn(shownTab);
        RequestDesktopUtils.maybeShowGlobalSettingOptInMessage(
                RequestDesktopUtils.DEFAULT_GLOBAL_SETTING_OPT_IN_DISPLAY_SIZE_MIN_THRESHOLD_INCHES,
                mProfile, mMessageDispatcher, mActivity, mCurrentTabSupplier);

        // Simulate clicking on the message on `clickedTab`, when also the `shownTab` has been
        // destroyed.
        Tab clickedTab = mock(Tab.class);
        when(mCurrentTabSupplier.get()).thenReturn(clickedTab);
        when(clickedTab.isDestroyed()).thenReturn(false);
        shownTab.destroy();
        RequestDesktopUtils.onGlobalSettingOptInMessageClicked(mProfile, mCurrentTabSupplier);
        verify(shownTab, never()).isDestroyed();
        verify(shownTab, never()).loadIfNeeded(anyInt());
        verify(clickedTab).loadIfNeeded(anyInt());
    }

    private void testSiteExceptionsDowngradePath() {
        mRdsDefaultValue = ContentSettingValues.BLOCK;
        enableFeature(ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS, false);
        enableFeature(SiteSettingsFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS_DOWNGRADE, true);

        int regularTabCount = 2;
        List<Tab> regularTabs = createTabsForDesktopSiteExceptionsDowngradeTest(
                mRegularTabModel, -1, regularTabCount, true, false);

        int incognitoTabCount = 1;
        List<Tab> incognitoTabs = createTabsForDesktopSiteExceptionsDowngradeTest(
                mIncognitoTabModel, regularTabCount - 1, incognitoTabCount, true);

        when(mRegularTabModel.getCount()).thenReturn(regularTabCount);
        when(mIncognitoTabModel.getCount()).thenReturn(incognitoTabCount);
        RequestDesktopUtils.maybeDowngradeSiteSettings(mTabModelSelector);
        Assert.assertTrue("SharedPreferences key for global setting should be present.",
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys
                                .DESKTOP_SITE_EXCEPTIONS_DOWNGRADE_GLOBAL_SETTING_ENABLED));
        Assert.assertFalse("SharedPreferences for global setting value should be set correctly.",
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys
                                .DESKTOP_SITE_EXCEPTIONS_DOWNGRADE_GLOBAL_SETTING_ENABLED,
                        false));
        Assert.assertEquals(
                "SharedPreferences string set size should match the total tab count across all tab models.",
                3,
                mSharedPreferencesManager
                        .readStringSet(ChromePreferenceKeys
                                               .DESKTOP_SITE_EXCEPTIONS_DOWNGRADE_TAB_SETTING_SET)
                        .size());

        RequestDesktopUtils.maybeRestoreUserAgentOnSiteSettingsDowngrade(regularTabs.get(0));
        RequestDesktopUtils.maybeRestoreUserAgentOnSiteSettingsDowngrade(incognitoTabs.get(0));
        RequestDesktopUtils.maybeRestoreUserAgentOnSiteSettingsDowngrade(regularTabs.get(1));

        verify(mCriticalPersistedTabData, times(2)).setUserAgent(TabUserAgent.DESKTOP);
        verify(mCriticalPersistedTabData, never()).setUserAgent(TabUserAgent.MOBILE);
        verify(mCriticalPersistedTabData, never()).setUserAgent(TabUserAgent.DEFAULT);

        Assert.assertTrue("SharedPreferences key should exist.",
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.DESKTOP_SITE_EXCEPTIONS_DOWNGRADE_TAB_SETTING_SET));
        Assert.assertTrue(
                "SharedPreferences string set should be empty after all tabs' tab level settings are processed.",
                mSharedPreferencesManager
                        .readStringSet(ChromePreferenceKeys
                                               .DESKTOP_SITE_EXCEPTIONS_DOWNGRADE_TAB_SETTING_SET)
                        .isEmpty());
    }

    private List<Tab> createTabsForDesktopSiteExceptionsDowngradeTest(
            TabModel tabModel, int lastUsedTabId, int tabCount, Boolean... lastUsedUserAgents) {
        assert tabCount == lastUsedUserAgents.length;
        List<Tab> tabs = new ArrayList<>();
        int tabIndex = 0;
        for (int i = lastUsedTabId + 1; i <= lastUsedTabId + tabCount; i++) {
            Tab tab = mock(Tab.class);
            WebContents webContents = mock(WebContents.class);
            NavigationController navigationController = mock(NavigationController.class);
            when(tab.getId()).thenReturn(i);
            when(tabModel.getTabAt(tabIndex)).thenReturn(tab);
            when(tab.getWebContents()).thenReturn(webContents);
            when(webContents.getNavigationController()).thenReturn(navigationController);
            int finalTabIndex = tabIndex;
            doAnswer(invocation -> lastUsedUserAgents[finalTabIndex])
                    .when(navigationController)
                    .getUseDesktopUserAgent();
            tabIndex++;
            tabs.add(tab);
        }
        return tabs;
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

    private void verifyUpdateDesktopSiteGlobalSettingOnUserRequest(boolean requestDesktopSite) {
        Assert.assertEquals("Desktop site content setting should be set correctly.",
                requestDesktopSite ? ContentSettingValues.ALLOW : ContentSettingValues.BLOCK,
                mRdsDefaultValue);
        Assert.assertEquals(
                "SharedPreference USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY should be set correctly.",
                requestDesktopSite,
                mSharedPreferencesManager.readBoolean(
                        SingleCategorySettingsConstants
                                .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY,
                        !requestDesktopSite));
        Assert.assertEquals("Histogram Android.RequestDesktopSite.Changed should be updated.", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.RequestDesktopSite.Changed",
                        requestDesktopSite ? SiteLayout.DESKTOP : SiteLayout.MOBILE));
    }

    private void disableGlobalDefaultsExperimentFeatures() {
        enableFeatureWithParams("RequestDesktopSiteDefaults", null, false);
        enableFeatureWithParams("RequestDesktopSiteDefaultsControl", null, false);
        enableFeatureWithParams("RequestDesktopSiteDefaultsControlSynthetic", null, false);
        enableFeatureWithParams("RequestDesktopSiteDefaultsSynthetic", null, false);
        enableFeatureWithParams("RequestDesktopSiteOptInControlSynthetic", null, false);
        enableFeatureWithParams("RequestDesktopSiteOptInSynthetic", null, false);
        enableFeatureWithParams("RequestDesktopSiteDefaultsControlCohort2", null, false);
        enableFeatureWithParams("RequestDesktopSiteDefaultsEnabledCohort2", null, false);
    }
}
