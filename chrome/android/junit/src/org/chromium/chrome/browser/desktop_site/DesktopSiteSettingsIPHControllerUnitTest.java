// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.desktop_site;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.JUnitProcessor;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.RequestDesktopUtilsUnitTest.ShadowSysUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IPHCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.site_settings.ContentSettingException;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.ShadowUrlUtilities;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

/** Unit tests for {@link DesktopSiteSettingsIPHController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowUrlUtilities.class, ShadowSysUtils.class})
public class DesktopSiteSettingsIPHControllerUnitTest {
    @Rule
    public TestRule mFeaturesProcessor = new JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;
    @Mock
    private WebsitePreferenceBridge mWebsitePreferenceBridge;
    @Mock
    private WindowAndroid mWindowAndroid;
    @Mock
    private ActivityTabProvider mActivityTabProvider;
    @Mock
    private View mToolbarMenuButton;
    @Mock
    private AppMenuHandler mAppMenuHandler;
    @Mock
    private UserEducationHelper mUserEducationHelper;
    @Mock
    private Context mContext;
    @Mock
    private WeakReference<Context> mWeakReferenceContext;
    @Mock
    private Tab mTab;
    @Mock
    private WebContents mWebContents;
    @Mock
    private NavigationController mNavigationController;
    @Mock
    private Tracker mTracker;
    @Mock
    private Profile mProfile;
    @Captor
    private ArgumentCaptor<IPHCommand> mIPHCommandCaptor;

    private DesktopSiteSettingsIPHController mController;
    private GURL mTabUrl;
    private Map<String, String> mTopDesktopSitesDomainMap;
    private boolean mDesktopSiteGloballyEnabled;
    private final TestValues mTestValues = new TestValues();
    private double mDeviceScreenSizeInInches;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJniMock);

        enableFeatureWithParams(ChromeFeatureList.ENABLE_IPH, null);
        enableFeatureWithParams(ChromeFeatureList.ANDROID_SCROLL_OPTIMIZATIONS, null);
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_PER_SITE_IPH, null);

        Resources resources = ApplicationProvider.getApplicationContext().getResources();

        doReturn(mWeakReferenceContext).when(mWindowAndroid).getContext();
        doReturn(mContext).when(mWeakReferenceContext).get();
        doReturn(resources).when(mContext).getResources();
        doReturn(mContext).when(mToolbarMenuButton).getContext();

        TrackerFactory.setTrackerForTests(mTracker);
        when(mTracker.wouldTriggerHelpUI(
                     FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE))
                .thenReturn(true);
        when(mTracker.wouldTriggerHelpUI(
                     FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_SPECIFIC_FEATURE))
                .thenReturn(true);

        mTabUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.getUrl()).thenReturn(mTabUrl);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mNavigationController.getUseDesktopUserAgent()).thenReturn(false);

        when(mWebsitePreferenceBridge.getContentSettingsExceptions(
                     mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE))
                .thenReturn(new ArrayList<>());

        doAnswer(invocation -> mDesktopSiteGloballyEnabled)
                .when(mWebsitePreferenceBridgeJniMock)
                .isContentSettingEnabled(any(), eq(ContentSettingsType.REQUEST_DESKTOP_SITE));

        mTopDesktopSitesDomainMap = new HashMap<>();
        mTopDesktopSitesDomainMap.put(JUnitTestGURLs.EXAMPLE_URL, "example.com");
        mTopDesktopSitesDomainMap.put("https://foo.bar.edu", "bar.edu");
        ShadowUrlUtilities.setTestImpl(new ShadowUrlUtilities.TestImpl() {
            @Override
            public String getDomainAndRegistry(String uri, boolean includePrivateRegistries) {
                return mTopDesktopSitesDomainMap.get(uri);
            }
        });

        mDeviceScreenSizeInInches = 10.0;
        ShadowSysUtils.setMemoryInMB(2048);

        initializeController();
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
        FeatureList.setTestValues(null);
        ShadowSysUtils.setMemoryInMB(0);
        ShadowUrlUtilities.reset();
        mDesktopSiteGloballyEnabled = false;
    }

    @Test
    public void testCreateTabObserver_SpecificIPH() {
        // Re-instantiate the controller to re-register the ActivityTabTabObserver with applicable
        // fieldtrial params set.
        mController.destroy();
        var params = new HashMap<String, String>();
        params.put(DesktopSiteSettingsIPHController.PARAM_IPH_TYPE_SPECIFIC, "true");
        params.put(DesktopSiteSettingsIPHController.PARAM_SITE_LIST,
                String.join(",", mTopDesktopSitesDomainMap.values()));
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_PER_SITE_IPH, params);
        initializeController();

        ActivityTabTabObserver activityTabTabObserver =
                mController.getActiveTabObserverForTesting();
        activityTabTabObserver.onPageLoadFinished(mTab, mTabUrl);
        verify(mUserEducationHelper).requestShowIPH(mIPHCommandCaptor.capture());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testCreateTabObserver_GenericIPH() {
        // Re-instantiate the controller to re-register the ActivityTabTabObserver with applicable
        // fieldtrial params set.
        mController.destroy();
        var params = new HashMap<String, String>();
        params.put(DesktopSiteSettingsIPHController.PARAM_IPH_TYPE_GENERIC, "true");
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_PER_SITE_IPH, params);
        initializeController();

        ActivityTabTabObserver activityTabTabObserver =
                mController.getActiveTabObserverForTesting();
        activityTabTabObserver.onPageLoadFinished(mTab, mTabUrl);
        verify(mUserEducationHelper).requestShowIPH(mIPHCommandCaptor.capture());
    }

    // This tests the fix for the crash reported in crbug.com/1416519.
    @Test
    public void testCreateTabObserver_NullTab() {
        // Re-instantiate the controller to re-register the ActivityTabTabObserver with applicable
        // fieldtrial params set.
        mController.destroy();
        var params = new HashMap<String, String>();
        params.put(DesktopSiteSettingsIPHController.PARAM_IPH_TYPE_SPECIFIC, "true");
        params.put(DesktopSiteSettingsIPHController.PARAM_SITE_LIST,
                String.join(",", mTopDesktopSitesDomainMap.values()));
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_PER_SITE_IPH, params);
        initializeController();

        ActivityTabTabObserver activityTabTabObserver =
                mController.getActiveTabObserverForTesting();
        activityTabTabObserver.onPageLoadFinished(null, mTabUrl);
        verify(mUserEducationHelper, never()).requestShowIPH(mIPHCommandCaptor.capture());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testCreateTabObserver_GenericIPH_NonTabletDevice() {
        // Re-instantiate the controller to re-register the ActivityTabTabObserver with applicable
        // fieldtrial params set.
        mController.destroy();
        var params = new HashMap<String, String>();
        params.put(DesktopSiteSettingsIPHController.PARAM_IPH_TYPE_GENERIC, "true");
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_PER_SITE_IPH, params);
        initializeController();
        Assert.assertNull(
                "ActivityTabTabObserver should be created for the generic IPH when the device is a tablet.",
                mController.getActiveTabObserverForTesting());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testCreateTabObserver_GenericIPH_InvalidDeviceScreenSize() {
        // Re-instantiate the controller to re-register the ActivityTabTabObserver with applicable
        // fieldtrial params set.
        mController.destroy();
        var params = new HashMap<String, String>();
        params.put(DesktopSiteSettingsIPHController.PARAM_IPH_TYPE_GENERIC, "true");
        params.put(DesktopSiteSettingsIPHController.PARAM_GENERIC_IPH_SCREEN_SIZE_THRESHOLD_INCHES,
                "12.0");
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_PER_SITE_IPH, params);
        initializeController();
        Assert.assertNull(
                "ActivityTabTabObserver should be created for the generic IPH when the device size is valid.",
                mController.getActiveTabObserverForTesting());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testCreateTabObserver_GenericIPH_InvalidDeviceMemory() {
        // Re-instantiate the controller to re-register the ActivityTabTabObserver with applicable
        // fieldtrial params set.
        mController.destroy();
        var params = new HashMap<String, String>();
        params.put(DesktopSiteSettingsIPHController.PARAM_IPH_TYPE_GENERIC, "true");
        params.put(DesktopSiteSettingsIPHController.PARAM_GENERIC_IPH_MEMORY_THRESHOLD_MB, "4096");
        enableFeatureWithParams(ChromeFeatureList.REQUEST_DESKTOP_SITE_PER_SITE_IPH, params);
        initializeController();
        Assert.assertNull(
                "ActivityTabTabObserver should be created for the generic IPH when the device memory is valid.",
                mController.getActiveTabObserverForTesting());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testPerSiteIPHPreChecksFailed_TrackerWouldNotTrigger() {
        // Test for specific IPH (Arm 1).
        when(mTracker.wouldTriggerHelpUI(
                     FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_SPECIFIC_FEATURE))
                .thenReturn(false);
        boolean failed = mController.perSiteIPHPreChecksFailed(
                mTab, mTracker, FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_SPECIFIC_FEATURE);
        verify(mTracker).wouldTriggerHelpUI(
                FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_SPECIFIC_FEATURE);
        Assert.assertTrue(
                "Specific site IPH should not trigger when Tracker#wouldTriggerHelpUI returns false.",
                failed);

        // Test for generic IPH (Arm 2).
        when(mTracker.wouldTriggerHelpUI(
                     FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE))
                .thenReturn(false);
        failed = mController.perSiteIPHPreChecksFailed(
                mTab, mTracker, FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE);
        verify(mTracker).wouldTriggerHelpUI(
                FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE);
        Assert.assertTrue(
                "Generic site IPH should not trigger when Tracker#wouldTriggerHelpUI returns false.",
                failed);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testPerSiteIPHPreChecksFailed_IncognitoTab() {
        when(mTab.isIncognito()).thenReturn(true);

        // Test for specific IPH (Arm 1).
        boolean failed = mController.perSiteIPHPreChecksFailed(
                mTab, mTracker, FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_SPECIFIC_FEATURE);
        Assert.assertTrue("Specific site IPH should not be triggered in incognito.", failed);

        // Test for generic IPH (Arm 2).
        failed = mController.perSiteIPHPreChecksFailed(
                mTab, mTracker, FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE);
        Assert.assertTrue("Generic site IPH should not be triggered in incognito.", failed);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testPerSiteIPHPreChecksFailed_ChromePage() {
        mTabUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.CHROME_ABOUT);
        when(mTab.getUrl()).thenReturn(mTabUrl);

        // Test for specific IPH (Arm 1).
        boolean failed = mController.perSiteIPHPreChecksFailed(
                mTab, mTracker, FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_SPECIFIC_FEATURE);
        Assert.assertTrue("Specific site IPH should not be triggered on a chrome:// page.", failed);

        // Test for generic IPH (Arm 2).
        failed = mController.perSiteIPHPreChecksFailed(
                mTab, mTracker, FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE);
        Assert.assertTrue("Generic site IPH should not be triggered on a chrome:// page.", failed);
    }

    @Test
    public void testShowSpecificIPH() {
        mController.setTopDesktopSitesForTesting(new HashSet<>(mTopDesktopSitesDomainMap.values()));
        mController.showSpecificIPH(mTab, mProfile);
        verify(mUserEducationHelper).requestShowIPH(mIPHCommandCaptor.capture());

        IPHCommand command = mIPHCommandCaptor.getValue();
        Assert.assertEquals("IPHCommand feature should match.", command.featureName,
                FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_SPECIFIC_FEATURE);
        Assert.assertEquals("IPHCommand stringId should match.",
                R.string.rds_site_settings_specific_iph_text, command.stringId);

        command.onShowCallback.run();
        verify(mAppMenuHandler).setMenuHighlight(R.id.request_desktop_site_id);

        command.onDismissCallback.run();
        verify(mAppMenuHandler).clearMenuHighlight();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShowGenericIPH_SwitchToDesktop() {
        testShowGenericIPH(true);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShowGenericIPH_SwitchToMobile() {
        testShowGenericIPH(false);
    }

    @Test
    public void testSpecificIPH_NotShown_DesktopUserAgentInUse() {
        // If a website from the top sites list is using the desktop UA, do not show the IPH.
        when(mNavigationController.getUseDesktopUserAgent()).thenReturn(true);
        mController.setTopDesktopSitesForTesting(new HashSet<>(mTopDesktopSitesDomainMap.values()));
        mController.showSpecificIPH(mTab, mProfile);
        verify(mUserEducationHelper, never()).requestShowIPH(mIPHCommandCaptor.capture());
    }

    @Test
    public void testSpecificIPH_NotShown_MobileSiteException() {
        // If a website from the top sites list is using the mobile UA, but the global setting uses
        // the desktop UA, do not show the IPH.
        mDesktopSiteGloballyEnabled = true;
        mController.setTopDesktopSitesForTesting(new HashSet<>(mTopDesktopSitesDomainMap.values()));
        mController.showSpecificIPH(mTab, mProfile);
        verify(mUserEducationHelper, never()).requestShowIPH(mIPHCommandCaptor.capture());
    }

    @Test
    public void testSpecificIPH_NotShown_SiteNotEligible() {
        mTabUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.INITIAL_URL);
        when(mTab.getUrl()).thenReturn(mTabUrl);
        mController.setTopDesktopSitesForTesting(new HashSet<>(mTopDesktopSitesDomainMap.values()));
        mController.showSpecificIPH(mTab, mProfile);
        verify(mUserEducationHelper, never()).requestShowIPH(mIPHCommandCaptor.capture());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testGenericIPH_NotShown_SettingUsed() {
        // The user must have previously used the site-level setting if exceptions are added.
        when(mWebsitePreferenceBridge.getContentSettingsExceptions(
                     mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE))
                .thenReturn(List.of(
                        mock(ContentSettingException.class), mock(ContentSettingException.class)));
        mController.showGenericIPH(mTab, mProfile);
        verify(mUserEducationHelper, never()).requestShowIPH(mIPHCommandCaptor.capture());
    }

    private void testShowGenericIPH(boolean switchToDesktop) {
        when(mNavigationController.getUseDesktopUserAgent()).thenReturn(!switchToDesktop);

        mController.showGenericIPH(mTab, mProfile);
        verify(mUserEducationHelper).requestShowIPH(mIPHCommandCaptor.capture());

        IPHCommand command = mIPHCommandCaptor.getValue();
        Assert.assertEquals("IPHCommand feature should match.", command.featureName,
                FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE);
        Assert.assertEquals("IPHCommand stringId should match.",
                switchToDesktop ? R.string.rds_site_settings_generic_iph_text_desktop
                                : R.string.rds_site_settings_generic_iph_text_mobile,
                command.stringId);
        Assert.assertEquals(
                "IPHCommand stringArgs should match.", mTabUrl.getHost(), command.stringArgs[0]);

        command.onShowCallback.run();
        verify(mAppMenuHandler).setMenuHighlight(R.id.request_desktop_site_id);

        command.onDismissCallback.run();
        verify(mAppMenuHandler).clearMenuHighlight();

        Assert.assertEquals(
                "<Android.RequestDesktopSite.PerSiteIphDismissed.AppMenuOpened> should be recorded when the IPH is dismissed.",
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.RequestDesktopSite.PerSiteIphDismissed.AppMenuOpened"));
    }

    private void enableFeatureWithParams(String featureName, Map<String, String> params) {
        mTestValues.addFeatureFlagOverride(featureName, true);
        if (params != null) {
            for (Entry<String, String> param : params.entrySet()) {
                mTestValues.addFieldTrialParamOverride(
                        featureName, param.getKey(), param.getValue());
            }
        }
        FeatureList.setTestValues(mTestValues);
    }

    private void initializeController() {
        mController = new DesktopSiteSettingsIPHController(mWindowAndroid, mActivityTabProvider,
                mProfile, mToolbarMenuButton, mAppMenuHandler, mUserEducationHelper,
                mWebsitePreferenceBridge, mDeviceScreenSizeInInches);
    }
}
