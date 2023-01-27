// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.desktop_site;

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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.JUnitProcessor;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IPHCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.site_settings.ContentSettingException;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingsType;
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
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

/** Unit tests for {@link DesktopSiteSettingsIPHController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DesktopSiteSettingsIPHControllerUnitTest {
    @Rule
    public TestRule mFeaturesProcessor = new JUnitProcessor();

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
    private final TestValues mTestValues = new TestValues();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

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

        mTabUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.getUrl()).thenReturn(mTabUrl);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mNavigationController.getUseDesktopUserAgent()).thenReturn(false);

        when(mWebsitePreferenceBridge.getContentSettingsExceptions(
                     mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE))
                .thenReturn(new ArrayList<>());

        initializeController();
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
        FeatureList.setTestValues(null);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testRegisterTabObserverForPerSiteIPH_Generic() {
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
    @Config(qualifiers = "sw320dp")
    public void testShowGenericIPH_NonTabletDevice() {
        mController.showGenericIPH(mTab, mProfile);
        verify(mUserEducationHelper, never()).requestShowIPH(mIPHCommandCaptor.capture());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShowGenericIPH_TrackerWouldNotTrigger() {
        when(mTracker.wouldTriggerHelpUI(
                     FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE))
                .thenReturn(false);
        mController.showGenericIPH(mTab, mProfile);
        verify(mTracker).wouldTriggerHelpUI(
                FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE);
        verify(mUserEducationHelper, never()).requestShowIPH(mIPHCommandCaptor.capture());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShowGenericIPH_NotShown_IncognitoTab() {
        when(mTab.isIncognito()).thenReturn(true);
        mController.showGenericIPH(mTab, mProfile);
        verify(mUserEducationHelper, never()).requestShowIPH(mIPHCommandCaptor.capture());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testGenericIPH_NotShown_ChromePage() {
        mTabUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.CHROME_ABOUT);
        when(mTab.getUrl()).thenReturn(mTabUrl);
        mController.showGenericIPH(mTab, mProfile);
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
                mWebsitePreferenceBridge);
    }
}
