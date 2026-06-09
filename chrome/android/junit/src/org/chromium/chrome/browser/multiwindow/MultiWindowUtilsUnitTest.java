// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.multiwindow.MultiWindowUtils.HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW;
import static org.chromium.chrome.browser.multiwindow.MultiWindowUtils.HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW;
import static org.chromium.chrome.browser.multiwindow.MultiWindowUtils.HISTOGRAM_PERSISTENT_STATE_ID_VERIFICATION;
import static org.chromium.chrome.browser.multiwindow.MultiWindowUtils.INVALID_TASK_ID;
import static org.chromium.chrome.browser.multiwindow.MultiWindowUtils.PERSISTENT_STATE_ID;
import static org.chromium.chrome.browser.tabwindow.TabWindowManager.INVALID_WINDOW_ID;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.PersistableBundle;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.SysUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.InstanceAllocationType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils.PersistentStateIdVerification;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link MultiWindowUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = 31)
public class MultiWindowUtilsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public OverrideContextWrapperTestRule mOverrideContextWrapperTestRule =
            new OverrideContextWrapperTestRule();

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    private static final int INSTANCE_ID_0 = 0;
    private static final int INSTANCE_ID_1 = 1;
    private static final int INSTANCE_ID_2 = 2;
    private static final int TASK_ID_5 = 5;
    private static final int TASK_ID_6 = 6;
    private static final int TASK_ID_7 = 7;
    private static final String URL_1 = "url1";
    private static final String URL_2 = "url2";
    private static final String URL_3 = "url3";
    private static final GURL NTP_GURL = new GURL(getOriginalNativeNtpUrl());
    private static final GURL TEST_GURL = new GURL("https://youtube.com/");

    private MultiWindowUtils mUtils;
    private boolean mIsInMultiDisplayMode;

    @Mock TabModelSelector mTabModelSelector;
    @Mock TabModel mNormalTabModel;
    @Mock TabModel mIncognitoTabModel;
    @Mock HomepageManager mHomepageManager;
    @Mock DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock AppHeaderState mAppHeaderState;
    @Mock Tab mTab1;
    @Mock Tab mTab2;
    @Mock Tab mTab3;
    @Mock TabWindowManager mTabWindowManager;

    private SettableMonotonicObservableSupplier<TabModel> mTabModelSupplier;

    @Before
    public void setUp() {
        MultiInstancePersistentStore.resetForTesting();
        mTabModelSupplier = ObservableSuppliers.createMonotonic(mNormalTabModel);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);

        mUtils =
                new MultiWindowUtils() {
                    @Override
                    public boolean isInMultiDisplayMode(Activity activity) {
                        return mIsInMultiDisplayMode;
                    }
                };

        when(mHomepageManager.isHomepageEnabled()).thenReturn(true);
        when(mHomepageManager.getHomepageGurl(/* isIncognito= */ false)).thenReturn(NTP_GURL);
        HomepageManager.setInstanceForTesting(mHomepageManager);

        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(mAppHeaderState);
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(false);
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mTabModelSupplier);

        SysUtils.setAmountOfPhysicalMemoryKbForTesting(
                7000 * ConversionUtils.KILOBYTES_PER_MEGABYTE);
    }

    private ChromeTabbedActivity addRunningTabbedActivity(int windowId) {
        return (ChromeTabbedActivity) addActivity(windowId, /* tabbedActivity= */ true);
    }

    private Activity addActivity(int windowId, boolean tabbedActivity) {
        Activity activity =
                tabbedActivity ? mock(ChromeTabbedActivity.class) : mock(Activity.class);
        when(mTabWindowManager.getIdForWindow(activity)).thenReturn(windowId);
        when(activity.getTaskId()).thenReturn(windowId);
        if (tabbedActivity) {
            var cta = (ChromeTabbedActivity) activity;
            when(cta.getWindowId()).thenReturn(windowId);
            when(cta.getSupportedProfileType()).thenReturn(SupportedProfileType.MIXED);
        }
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.RESUMED);
        return activity;
    }

    private ChromeTabbedActivity createMockActivity() {
        ChromeTabbedActivity mActivity = mock(ChromeTabbedActivity.class);
        var packageName = ContextUtils.getApplicationContext().getPackageName();
        when(mActivity.getPackageName()).thenReturn(packageName);
        return mActivity;
    }

    @Test
    public void testCreateNewWindowIntent_incognito_addsIncognitoIntentExtra() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        Activity mActivity = createMockActivity();
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        mActivity,
                        /* isIncognito= */ true,
                        NewWindowAppSource.BROWSER_WINDOW_CREATOR);

        assertNotNull(intent);
        assertTrue(
                intent.getBooleanExtra(
                        IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_WINDOW, /* defaultValue= */ false));
    }

    @Test
    public void testCreateNewWindowIntent_notIncognito_skipsIncognitoIntentExtra() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        Activity mActivity = createMockActivity();
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        mActivity,
                        /* isIncognito= */ false,
                        NewWindowAppSource.BROWSER_WINDOW_CREATOR);
        assertNotNull(intent);
        assertFalse(
                intent.getBooleanExtra(
                        IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_WINDOW, /* defaultValue= */ true));
    }

    @Test
    @Config(sdk = 32)
    public void testCreateNewWindowIntent_nonMultiWindowMode_opensAdjacently() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        Activity mActivity = createMockActivity();
        when(mActivity.isInMultiWindowMode()).thenReturn(false);

        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        mActivity,
                        /* isIncognito= */ false,
                        NewWindowAppSource.BROWSER_WINDOW_CREATOR);

        assertNotNull(intent);
        assertTrue((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    public void testCreateNewWindowIntent_multiWindowMode_opensAdjacently() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        Activity mActivity = createMockActivity();
        when(mActivity.isInMultiWindowMode()).thenReturn(true);

        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        mActivity,
                        /* isIncognito= */ false,
                        NewWindowAppSource.BROWSER_WINDOW_CREATOR);

        assertNotNull(intent);
        assertTrue((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    public void testCreateNewWindowIntent_incognito_throwsException_preApi31() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        Activity mActivity = createMockActivity();
        assertThrows(
                AssertionError.class,
                () ->
                        MultiWindowUtils.createNewWindowIntent(
                                mActivity, /* isIncognito= */ true, NewWindowAppSource.MENU));
    }

    @Test
    public void testCreateNewWindowIntent_unsupportedWindowingMode_throwsException_preApi31() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        Activity mActivity = createMockActivity();
        when(mActivity.isInMultiWindowMode()).thenReturn(false);
        mIsInMultiDisplayMode = false;

        assertThrows(
                AssertionError.class,
                () ->
                        MultiWindowUtils.createNewWindowIntent(
                                mActivity, /* isIncognito= */ false, NewWindowAppSource.MENU));
    }

    @Test
    public void testCreateNewWindowIntent_multiWindowMode_launchesAdjacently_preApi31() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        Activity mActivity = createMockActivity();

        // Multi-window mode.
        when(mActivity.isInMultiWindowMode()).thenReturn(true);

        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        mActivity, /* isIncognito= */ false, NewWindowAppSource.MENU);

        assertNotNull(intent);
        assertTrue((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    public void testGetExtraPreferNewFromIntent_IntentExtraValue() {
        // EXTRA_PREFER_NEW is present and true.
        Intent intent = new Intent();
        intent.putExtra(IntentHandler.EXTRA_PREFER_NEW, true);
        assertTrue(
                "Should be true when EXTRA_PREFER_NEW is true.",
                MultiWindowUtils.getExtraPreferNewFromIntent(intent));

        // EXTRA_PREFER_NEW is present and false.
        intent.putExtra(IntentHandler.EXTRA_PREFER_NEW, false);
        assertFalse(
                "Should be false when EXTRA_PREFER_NEW is false.",
                MultiWindowUtils.getExtraPreferNewFromIntent(intent));
    }

    @Test
    @Config(sdk = 35)
    public void testGetExtraPreferNewFromIntent_DefaultValue_BelowThresholdSDK() {
        // EXTRA_PREFER_NEW is not present, conditions for preferNew are met but SDK is too low.
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        assertFalse(
                "Should be false when SDK is not high enough.",
                MultiWindowUtils.getExtraPreferNewFromIntent(intent));
    }

    @Test
    @Config(sdk = 37)
    @DisabledTest(message = "crbug.com/440643534: Enable when SDK support is available.")
    public void testGetExtraPreferNewFromIntent_UpdatedDefaultValue() {
        // EXTRA_PREFER_NEW is not present, conditions for preferNew are met.
        Intent intent = new Intent();
        intent = new Intent(Intent.ACTION_MAIN);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        assertTrue(
                "Should be true when conditions are met.",
                MultiWindowUtils.getExtraPreferNewFromIntent(intent));

        // Test with different conditions not being met.
        // Wrong action.
        intent = new Intent(Intent.ACTION_VIEW);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        assertFalse(
                "Should be false for wrong action.",
                MultiWindowUtils.getExtraPreferNewFromIntent(intent));

        // No NEW_TASK flag.
        intent = new Intent(Intent.ACTION_MAIN);
        intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        assertFalse(
                "Should be false without NEW_TASK.",
                MultiWindowUtils.getExtraPreferNewFromIntent(intent));

        // No MULTIPLE_TASK flag.
        intent = new Intent(Intent.ACTION_MAIN);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        assertFalse(
                "Should be false without MULTIPLE_TASK.",
                MultiWindowUtils.getExtraPreferNewFromIntent(intent));
    }

    @Test
    public void testCanEnterMultiWindowMode_isAutomotive_returnsFalse() {
        mOverrideContextWrapperTestRule.setIsAutomotive(true);
        assertFalse(MultiWindowUtils.canEnterMultiWindowMode());
    }

    @Test
    @Config(sdk = VERSION_CODES.R)
    public void testCanEnterMultiWindowMode_withCustomOemSupport_returnsTrue() {
        mOverrideContextWrapperTestRule.setIsAutomotive(false);
        ReflectionHelpers.setStaticField(Build.class, "MANUFACTURER", "samsung");
        assertTrue(MultiWindowUtils.canEnterMultiWindowMode());
    }

    @Test
    @Config(sdk = VERSION_CODES.S)
    public void testCanEnterMultiWindowMode_withoutAutoSplitSupport_returnsFalse() {
        mOverrideContextWrapperTestRule.setIsAutomotive(false);
        assertFalse(MultiWindowUtils.canEnterMultiWindowMode());
    }

    @Test
    @Config(sdk = VERSION_CODES.S_V2)
    public void testCanEnterMultiWindowMode_withAutoSplitSupport_returnsTrue() {
        mOverrideContextWrapperTestRule.setIsAutomotive(false);
        assertTrue(MultiWindowUtils.canEnterMultiWindowMode());
    }

    @Test
    public void testIsLinkNavigationToNewWindowSupported_invalidParams() {
        // No support on automotive devices.
        mOverrideContextWrapperTestRule.setIsAutomotive(true);
        assertFalse(MultiWindowUtils.isLinkNavigationToNewWindowSupported());

        // No support when multi-instance Api31 feature is disabled.
        mOverrideContextWrapperTestRule.setIsAutomotive(false);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        assertFalse(MultiWindowUtils.isLinkNavigationToNewWindowSupported());
    }

    @Test
    public void testIsLinkNavigationToNewWindowSupported_withinInstanceLimit() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        MultiWindowUtils.setMaxInstancesForTesting(3);
        // Create 1 active regular window, and 1 active incognito window.
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 1,
                /* numInactive= */ 0,
                SupportedProfileType.REGULAR,
                /* startId= */ INSTANCE_ID_0);
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 1,
                /* numInactive= */ 0,
                SupportedProfileType.OFF_THE_RECORD,
                /* startId= */ INSTANCE_ID_1);

        assertTrue(MultiWindowUtils.isLinkNavigationToNewWindowSupported());
    }

    @Test
    public void testIsLinkNavigationToNewWindowSupported_atInstanceLimit() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        MultiWindowUtils.setMaxInstancesForTesting(2);
        // Create 1 active regular window, and 1 active incognito window.
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 1,
                /* numInactive= */ 0,
                SupportedProfileType.REGULAR,
                /* startId= */ INSTANCE_ID_0);
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 1,
                /* numInactive= */ 0,
                SupportedProfileType.OFF_THE_RECORD,
                /* startId= */ INSTANCE_ID_1);

        assertFalse(MultiWindowUtils.isLinkNavigationToNewWindowSupported());
    }

    @Test
    public void testIsLinkNavigationToIncognitoWindowSupported_invalidParams() {
        // No support on automotive devices.
        mOverrideContextWrapperTestRule.setIsAutomotive(true);
        assertFalse(MultiWindowUtils.isLinkNavigationToIncognitoWindowSupported());

        // No support when incognito windowing is disabled.
        mOverrideContextWrapperTestRule.setIsAutomotive(false);
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(false);
        assertFalse(MultiWindowUtils.isLinkNavigationToIncognitoWindowSupported());

        // No support when multi-instance Api31 feature is disabled.
        mOverrideContextWrapperTestRule.setIsAutomotive(false);
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        assertFalse(MultiWindowUtils.isLinkNavigationToIncognitoWindowSupported());
    }

    @Test
    public void testIsLinkNavigationToIncognitoWindowSupported_withinInstanceLimit() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        MultiWindowUtils.setMaxInstancesForTesting(3);
        // Create 1 active regular window, 1 inactive regular window, and 1 active incognito window.
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 1,
                /* numInactive= */ 1,
                SupportedProfileType.REGULAR,
                /* startId= */ INSTANCE_ID_0);
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 1,
                /* numInactive= */ 0,
                SupportedProfileType.OFF_THE_RECORD,
                /* startId= */ INSTANCE_ID_2);

        assertTrue(MultiWindowUtils.isLinkNavigationToIncognitoWindowSupported());
    }

    @Test
    public void
            testIsLinkNavigationToIncognitoWindowSupported_atInstanceLimitWithOtherActiveIncognitoWindow() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        MultiWindowUtils.setMaxInstancesForTesting(3);
        // Create 2 active regular windows, and 1 active incognito window.
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 2,
                /* numInactive= */ 0,
                SupportedProfileType.REGULAR,
                /* startId= */ INSTANCE_ID_0);
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 1,
                /* numInactive= */ 0,
                SupportedProfileType.OFF_THE_RECORD,
                /* startId= */ INSTANCE_ID_2);

        assertTrue(MultiWindowUtils.isLinkNavigationToIncognitoWindowSupported());
    }

    @Test
    public void
            testIsLinkNavigationToIncognitoWindowSupported_atInstanceLimitWithNoOtherActiveIncognitoWindow() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        MultiWindowUtils.setMaxInstancesForTesting(2);
        // Create 2 active regular windows, and 1 inactive incognito window.
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 2,
                /* numInactive= */ 0,
                SupportedProfileType.REGULAR,
                /* startId= */ INSTANCE_ID_0);
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 0,
                /* numInactive= */ 1,
                SupportedProfileType.OFF_THE_RECORD,
                /* startId= */ INSTANCE_ID_2);

        assertFalse(MultiWindowUtils.isLinkNavigationToIncognitoWindowSupported());
    }

    @Test
    public void testIsLinkNavigationToOtherWindowSupported_automotiveDevice() {
        Activity activity = addActivity(/* windowId= */ INSTANCE_ID_0, /* tabbedActivity= */ true);
        mOverrideContextWrapperTestRule.setIsAutomotive(true);
        assertFalse(mUtils.isLinkNavigationToOtherWindowSupported(activity));
    }

    @Test
    public void testIsLinkNavigationToOtherWindowSupported_withinInstanceLimit() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        Activity activity = addActivity(/* windowId= */ INSTANCE_ID_0, /* tabbedActivity= */ true);
        MultiWindowUtils.setMaxInstancesForTesting(3);
        // Create 1 active regular window, and 1 active incognito window.
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 1,
                /* numInactive= */ 0,
                SupportedProfileType.REGULAR,
                /* startId= */ INSTANCE_ID_0);
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 1,
                /* numInactive= */ 0,
                SupportedProfileType.OFF_THE_RECORD,
                /* startId= */ INSTANCE_ID_1);

        assertFalse(mUtils.isLinkNavigationToOtherWindowSupported(activity));
    }

    @Test
    public void testIsLinkNavigationToOtherWindowSupported_atInstanceLimit_regularWindow() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        Activity activity = addActivity(/* windowId= */ INSTANCE_ID_0, /* tabbedActivity= */ true);
        MultiWindowUtils.setMaxInstancesForTesting(3);
        // Create 2 active regular windows, and 1 active incognito window.
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 2,
                /* numInactive= */ 0,
                SupportedProfileType.REGULAR,
                /* startId= */ INSTANCE_ID_0);
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 1,
                /* numInactive= */ 0,
                SupportedProfileType.OFF_THE_RECORD,
                /* startId= */ INSTANCE_ID_2);

        assertTrue(mUtils.isLinkNavigationToOtherWindowSupported(activity));
    }

    @Test
    public void
            testIsLinkNavigationToOtherWindowSupported_atInstanceLimit_regularWindow_noOtherWindow() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        Activity activity = addActivity(/* windowId= */ INSTANCE_ID_0, /* tabbedActivity= */ true);
        MultiWindowUtils.setMaxInstancesForTesting(2);
        // Create 1 active regular window, and 1 active incognito window.
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 1,
                /* numInactive= */ 0,
                SupportedProfileType.REGULAR,
                /* startId= */ INSTANCE_ID_0);
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 1,
                /* numInactive= */ 0,
                SupportedProfileType.OFF_THE_RECORD,
                /* startId= */ INSTANCE_ID_1);

        assertFalse(mUtils.isLinkNavigationToOtherWindowSupported(activity));
    }

    @Test
    public void testIsLinkNavigationToOtherWindowSupported_atInstanceLimit_incognitoWindow() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        var activity =
                (ChromeTabbedActivity)
                        addActivity(/* windowId= */ INSTANCE_ID_0, /* tabbedActivity= */ true);
        when(activity.isIncognitoWindow()).thenReturn(true);
        MultiWindowUtils.setMaxInstancesForTesting(3);
        // Create 1 active regular window, and 2 active incognito windows.
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 2,
                /* numInactive= */ 0,
                SupportedProfileType.OFF_THE_RECORD,
                /* startId= */ INSTANCE_ID_0);
        MultiWindowTestUtils.createInstances(
                /* numActive= */ 1,
                /* numInactive= */ 0,
                SupportedProfileType.REGULAR,
                /* startId= */ INSTANCE_ID_2);

        assertFalse(mUtils.isLinkNavigationToOtherWindowSupported(activity));
    }

    @Test
    public void testIsLinkNavigationToOtherWindowSupported_preApi31_invalidParams() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        ChromeTabbedActivity tabbedActivity = mock(ChromeTabbedActivity.class);

        // No support when not in multi-window or multi-display mode.
        when(tabbedActivity.isInMultiWindowMode()).thenReturn(false);
        mIsInMultiDisplayMode = false;
        assertFalse(mUtils.isLinkNavigationToOtherWindowSupported(tabbedActivity));

        // No support when other window activity is null.
        when(tabbedActivity.isInMultiWindowMode()).thenReturn(true);
        assertFalse(mUtils.isLinkNavigationToOtherWindowSupported(mock(Activity.class)));
    }

    @Test
    public void testIsLinkNavigationToOtherWindowSupported_preApi31_inMultiWindowMode() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        ChromeTabbedActivity tabbedActivity = mock(ChromeTabbedActivity.class);
        when(tabbedActivity.isInMultiWindowMode()).thenReturn(true);
        assertTrue(mUtils.isLinkNavigationToOtherWindowSupported(tabbedActivity));
    }

    @Test
    public void testIsLinkNavigationToOtherWindowSupported_preApi31_inMultiDisplayMode() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        ChromeTabbedActivity tabbedActivity = mock(ChromeTabbedActivity.class);
        when(tabbedActivity.isInMultiWindowMode()).thenReturn(false);
        mIsInMultiDisplayMode = true;
        assertTrue(mUtils.isLinkNavigationToOtherWindowSupported(tabbedActivity));
    }

    @Test
    @Config(sdk = VERSION_CODES.S)
    public void testIsMoveOtherWindowSupported_InstanceSwitcherEnabled_ReturnsTrue() {
        MultiWindowTestUtils.enableMultiInstance();
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);

        // Instance with no tabs (ID_1) still counts as long as it is alive.
        writeInstanceInfo(
                INSTANCE_ID_0,
                URL_1,
                /* tabCount= */ 3,
                /* incognitoTabCount= */ 2,
                TASK_ID_5,
                /* profileType= */ SupportedProfileType.MIXED);
        writeInstanceInfo(
                INSTANCE_ID_1,
                URL_2,
                /* tabCount= */ 0,
                /* incognitoTabCount= */ 0,
                TASK_ID_6,
                /* profileType= */ SupportedProfileType.MIXED);

        // Mock that the tasks for the 2 active instances are running.
        MultiWindowUtils.setAppTaskIdsForTesting(
                new HashSet<>(Arrays.asList(TASK_ID_5, TASK_ID_6)));

        assertTrue(
                "Should return true on Android R+ with multiple tabs.",
                mUtils.isMoveToOtherWindowSupported(null, mTabModelSelector));
    }

    @Test
    @Config(sdk = VERSION_CODES.S)
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testIsMoveOtherWindowSupported_SingleActiveRegularWindow_ReturnsFalse() {
        MultiWindowTestUtils.enableMultiInstance();
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(false);

        writeInstanceInfo(
                INSTANCE_ID_0,
                URL_1,
                /* tabCount= */ 3,
                /* incognitoTabCount= */ 0,
                TASK_ID_5,
                /* profileType= */ SupportedProfileType.REGULAR);

        // Mock that the tasks for the active instance is running.
        MultiWindowUtils.setAppTaskIdsForTesting(new HashSet<>(List.of(TASK_ID_5)));

        assertFalse(
                "Should return false since there is only one regular window.",
                mUtils.isMoveToOtherWindowSupported(null, mTabModelSelector));
    }

    @Test
    @Config(sdk = VERSION_CODES.S)
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testIsMoveOtherWindowSupported_MultipleActiveRegularWindow_ReturnsTrue() {
        MultiWindowTestUtils.enableMultiInstance();
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(false);

        writeInstanceInfo(
                INSTANCE_ID_0,
                URL_1,
                /* tabCount= */ 3,
                /* incognitoTabCount= */ 0,
                TASK_ID_5,
                /* profileType= */ SupportedProfileType.REGULAR);
        writeInstanceInfo(
                INSTANCE_ID_1,
                URL_2,
                /* tabCount= */ 2,
                /* incognitoTabCount= */ 0,
                TASK_ID_6,
                /* profileType= */ SupportedProfileType.REGULAR);

        // Mock that the tasks for the 2 active instances are running.
        MultiWindowUtils.setAppTaskIdsForTesting(
                new HashSet<>(Arrays.asList(TASK_ID_5, TASK_ID_6)));

        assertTrue(
                "Should return true since there are multiple regular windows.",
                mUtils.isMoveToOtherWindowSupported(null, mTabModelSelector));
    }

    @Test
    @Config(sdk = VERSION_CODES.S)
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testIsMoveOtherWindowSupported_SingleActiveIncognitoWindow_ReturnsFalse() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        MultiWindowTestUtils.enableMultiInstance();
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(true);

        writeInstanceInfo(
                INSTANCE_ID_0,
                URL_1,
                /* tabCount= */ 0,
                /* incognitoTabCount= */ 2,
                TASK_ID_5,
                /* profileType= */ SupportedProfileType.OFF_THE_RECORD);

        // Mock that the task for the active instance is running.
        MultiWindowUtils.setAppTaskIdsForTesting(new HashSet<>(List.of(TASK_ID_5)));

        assertFalse(
                "Should return false since there is only one incognito window.",
                mUtils.isMoveToOtherWindowSupported(null, mTabModelSelector));
    }

    @Test
    @Config(sdk = VERSION_CODES.S)
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testIsMoveOtherWindowSupported_MultipleActiveIncognitoWindow_ReturnsTrue() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        MultiWindowTestUtils.enableMultiInstance();
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(true);

        writeInstanceInfo(
                INSTANCE_ID_0,
                URL_1,
                /* tabCount= */ 0,
                /* incognitoTabCount= */ 2,
                TASK_ID_5,
                /* profileType= */ SupportedProfileType.OFF_THE_RECORD);
        writeInstanceInfo(
                INSTANCE_ID_1,
                URL_2,
                /* tabCount= */ 0,
                /* incognitoTabCount= */ 2,
                TASK_ID_6,
                /* profileType= */ SupportedProfileType.OFF_THE_RECORD);

        // Mock that the tasks for the 2 active instances are running.
        MultiWindowUtils.setAppTaskIdsForTesting(
                new HashSet<>(Arrays.asList(TASK_ID_5, TASK_ID_6)));

        assertTrue(
                "Should return true since there are multiple incognito windows.",
                mUtils.isMoveToOtherWindowSupported(null, mTabModelSelector));
    }

    @Test
    @Config(sdk = BaseRobolectricTestRunner.MIN_SDK)
    public void
            testIsMoveOtherWindowSupported_InstanceSwitcherDisabledAndInMultiWindowMode_ReturnsTrue() {
        ChromeTabbedActivity tabbedActivity = mock(ChromeTabbedActivity.class);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);
        when(tabbedActivity.isInMultiWindowMode()).thenReturn(true);
        assertTrue(
                "Should return true on Android Q with multiple tabs.",
                mUtils.isMoveToOtherWindowSupported(tabbedActivity, mTabModelSelector));
    }

    @Test
    public void testIsMoveOtherWindowSupported_HasOneTabWithHomePageDisabled_ReturnsTrue() {
        ChromeTabbedActivity tabbedActivity = mock(ChromeTabbedActivity.class);
        when(mHomepageManager.isHomepageEnabled()).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        when(tabbedActivity.isInMultiWindowMode()).thenReturn(true);
        assertTrue(
                "Should return true when called for last tab with homepage disabled.",
                mUtils.isMoveToOtherWindowSupported(tabbedActivity, mTabModelSelector));
    }

    @Test
    public void testIsMoveOtherWindowSupported_HasOneTabWithHomePageEnabledAsNtp_ReturnsTrue() {
        ChromeTabbedActivity tabbedActivity = mock(ChromeTabbedActivity.class);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        when(tabbedActivity.isInMultiWindowMode()).thenReturn(true);
        assertTrue(
                "Should return true when called for last tab with homepage enabled as NTP.",
                mUtils.isMoveToOtherWindowSupported(tabbedActivity, mTabModelSelector));
    }

    @Test
    public void
            testIsMoveOtherWindowSupported_HasOneTabWithHomePageEnabledAsCustomUrl_ReturnsFalse() {
        when(mHomepageManager.getHomepageGurl(/* isIncognito= */ false)).thenReturn(TEST_GURL);
        when(mHomepageManager.isHomepageEnabled()).thenReturn(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        assertFalse(
                "Should return false when called for last tab with homepage set as a custom url.",
                mUtils.isMoveToOtherWindowSupported(null, mTabModelSelector));
    }

    @Test
    public void testIsMoveOtherWindowSupported_OnAutomotive_ReturnsFalse() {
        mOverrideContextWrapperTestRule.setIsAutomotive(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);
        assertFalse(
                "Should return false for automotive.",
                mUtils.isMoveToOtherWindowSupported(null, mTabModelSelector));
    }

    @Test
    public void testHasAtMostOneTabWithHomepageEnabled_OneTab_HasCustomHomepage() {
        when(mHomepageManager.shouldCloseAppWithZeroTabs()).thenReturn(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        assertTrue(
                "Should return true with one tab and custom homepage.",
                mUtils.hasAtMostOneTabWithHomepageEnabled(mTabModelSelector));
    }

    @Test
    public void testHasAtMostOneTabWithHomepageEnabled_OneTab_NoCustomHomepage() {
        when(mHomepageManager.shouldCloseAppWithZeroTabs()).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        assertFalse(
                "Should return false with one tab and no custom homepage.",
                mUtils.hasAtMostOneTabWithHomepageEnabled(mTabModelSelector));
    }

    @Test
    public void testHasAtMostOneTabWithHomepageEnabled_WithMoreThanOneTab_HasCustomHomepage() {
        when(mHomepageManager.shouldCloseAppWithZeroTabs()).thenReturn(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);
        assertFalse(
                "Should return false for multiple tabs.",
                mUtils.hasAtMostOneTabWithHomepageEnabled(mTabModelSelector));
    }

    @Test
    public void testHasAtMostOneTabWithHomepageEnabled_WithMoreThanOneTab_NoCustomHomepage() {
        when(mHomepageManager.shouldCloseAppWithZeroTabs()).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);
        assertFalse(
                "Should return false for multiple tabs.",
                mUtils.hasAtMostOneTabWithHomepageEnabled(mTabModelSelector));
    }

    @Test
    public void
            testHasAtMostOneTabGroupWithHomepageEnabled_OneTabGroupAndNoOtherTabs_HasCustomHomepage() {
        when(mHomepageManager.shouldCloseAppWithZeroTabs()).thenReturn(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(3);
        when(mNormalTabModel.getTabCountForGroup(any())).thenReturn(3);
        when(mNormalTabModel.getTabAt(0)).thenReturn(mTab1);
        assertTrue(
                "Should return true with one tab group and custom homepage.",
                mUtils.hasAtMostOneTabGroupWithHomepageEnabled(mTabModelSelector, mNormalTabModel));
    }

    @Test
    public void
            testHasAtMostOneTabWithHomepageEnabled_OneTabGroupAndNoOtherTabs_NoCustomHomepage() {
        when(mHomepageManager.shouldCloseAppWithZeroTabs()).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(3);
        when(mNormalTabModel.getTabCountForGroup(any())).thenReturn(3);
        when(mNormalTabModel.getTabAt(0)).thenReturn(mTab1);
        assertFalse(
                "Should return true with one tab group and custom homepage.",
                mUtils.hasAtMostOneTabGroupWithHomepageEnabled(mTabModelSelector, mNormalTabModel));
    }

    @Test
    public void testHasAtMostOneTabWithHomepageEnabled_WithMoreThanOneTabGroup_HasCustomHomepage() {
        when(mHomepageManager.shouldCloseAppWithZeroTabs()).thenReturn(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(4);
        when(mNormalTabModel.getTabCountForGroup(any())).thenReturn(3);
        when(mNormalTabModel.getTabAt(0)).thenReturn(mTab1);
        assertFalse(
                "Should return false for multiple tabs.",
                mUtils.hasAtMostOneTabGroupWithHomepageEnabled(mTabModelSelector, mNormalTabModel));
    }

    @Test
    public void testHasAtMostOneTabWithHomepageEnabled_WithMoreThanOneTabGroup_NoCustomHomepage() {
        when(mHomepageManager.shouldCloseAppWithZeroTabs()).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(4);
        when(mNormalTabModel.getTabCountForGroup(any())).thenReturn(3);
        when(mNormalTabModel.getTabAt(0)).thenReturn(mTab1);
        assertFalse(
                "Should return false for multiple tabs.",
                mUtils.hasAtMostOneTabGroupWithHomepageEnabled(mTabModelSelector, mNormalTabModel));
    }

    @Test
    public void testGetRunningTabbedActivityCount() {
        // Create 1 activity that is not a ChromeTabbedActivity and 2 ChromeTabbedActivity's.
        Activity activity1 =
                addActivity(/* windowId= */ INSTANCE_ID_0, /* tabbedActivity= */ false);
        Activity activity2 = addActivity(/* windowId= */ INSTANCE_ID_1, /* tabbedActivity= */ true);
        Activity activity3 = addActivity(/* windowId= */ INSTANCE_ID_2, /* tabbedActivity= */ true);

        // Remove activity2, this will be considered a non-running activity subsequently.
        ApplicationStatus.onStateChangeForTesting(activity2, ActivityState.DESTROYED);

        int runningTabbedActivityCount = MultiWindowUtils.getRunningTabbedActivityCount();
        assertEquals(
                "There should be only 1 running ChromeTabbedActivity.",
                1,
                runningTabbedActivityCount);
    }

    @Test
    public void launchIntentInMaybeClosedWindow_NewWindow() {
        MultiWindowTestUtils.enableMultiInstance();
        Intent intent = new Intent();
        ChromeTabbedActivity activity = mock(ChromeTabbedActivity.class);
        MultiWindowUtils.launchIntentInMaybeClosedWindow(activity, intent, INSTANCE_ID_0);
        verify(activity).startActivity(intent, null);
        assertEquals(
                INSTANCE_ID_0,
                intent.getIntExtra(IntentHandler.EXTRA_WINDOW_ID, INVALID_WINDOW_ID));
    }

    @Test
    public void launchIntentInMaybeClosedWindow_ExistingWindow() {
        MultiWindowTestUtils.enableMultiInstance();
        ChromeTabbedActivity activity1 =
                (ChromeTabbedActivity)
                        addActivity(/* windowId= */ INSTANCE_ID_0, /* tabbedActivity= */ true);

        Intent intent = new Intent();
        ChromeTabbedActivity activity2 = mock(ChromeTabbedActivity.class);
        MultiWindowUtils.launchIntentInMaybeClosedWindow(activity2, intent, INSTANCE_ID_0);
        verify(activity1).onNewIntent(intent);
    }

    @Test
    public void testGetInstanceCount() {
        MultiWindowTestUtils.enableMultiInstance();
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        // Create 2 active instances.
        writeInstanceInfo(
                INSTANCE_ID_0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 2, TASK_ID_5);
        writeInstanceInfo(
                INSTANCE_ID_1, URL_2, /* tabCount= */ 1, /* incognitoTabCount= */ 0, TASK_ID_6);

        // Create 1 inactive instance. This instance is restorable because it has tabs, but it is
        // not active because it does not have a valid task ID.
        writeInstanceInfo(
                INSTANCE_ID_2,
                URL_3,
                /* tabCount= */ 5,
                /* incognitoTabCount= */ 0,
                MultiWindowUtils.INVALID_TASK_ID);

        // Mock that the tasks for the 2 active instances are running.
        MultiWindowUtils.setAppTaskIdsForTesting(
                new HashSet<>(Arrays.asList(TASK_ID_5, TASK_ID_6)));

        assertEquals(
                "getInstanceCount should only count active instances.",
                2,
                MultiWindowUtils.getInstanceCount(
                        MultiInstanceManagerApi31.PersistedInstanceType.ACTIVE));

        assertEquals(
                "getInstanceCount should count all instances.",
                3,
                MultiWindowUtils.getInstanceCount(
                        MultiInstanceManagerApi31.PersistedInstanceType.ANY));

        // Mark the inactive instance for deletion.
        ChromeMultiInstancePersistentStore.writeMarkedForDeletion(INSTANCE_ID_2, true);
        assertEquals(
                "getInstanceCount should exclude instances marked for deletion.",
                2,
                MultiWindowUtils.getInstanceCount(
                        MultiInstanceManagerApi31.PersistedInstanceType.ANY));
    }

    @Test
    public void testGetInstanceCount_WithPreFetchedAppTaskIds() {
        MultiWindowTestUtils.enableMultiInstance();
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        // Create 2 active instances and 1 inactive instance.
        writeInstanceInfo(
                INSTANCE_ID_0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 2, TASK_ID_5);
        writeInstanceInfo(
                INSTANCE_ID_1, URL_2, /* tabCount= */ 1, /* incognitoTabCount= */ 0, TASK_ID_6);
        writeInstanceInfo(
                INSTANCE_ID_2,
                URL_3,
                /* tabCount= */ 5,
                /* incognitoTabCount= */ 0,
                MultiWindowUtils.INVALID_TASK_ID);

        MultiWindowUtils.setAppTaskIdsForTesting(
                new HashSet<>(Arrays.asList(TASK_ID_5, TASK_ID_6)));

        // Pre-fetch appTaskIds.
        Set<Integer> appTaskIds = new HashSet<>(Arrays.asList(TASK_ID_5, TASK_ID_6));

        // Verify the overload with pre-fetched appTaskIds matches the no-arg version.
        assertEquals(
                "getInstanceCount overload should match no-arg version for ACTIVE type.",
                MultiWindowUtils.getInstanceCount(
                        MultiInstanceManagerApi31.PersistedInstanceType.ACTIVE),
                MultiWindowUtils.getInstanceCount(
                        MultiInstanceManagerApi31.PersistedInstanceType.ACTIVE, appTaskIds));

        assertEquals(
                "getInstanceCount overload should match no-arg version for ANY type.",
                MultiWindowUtils.getInstanceCount(
                        MultiInstanceManagerApi31.PersistedInstanceType.ANY),
                MultiWindowUtils.getInstanceCount(
                        MultiInstanceManagerApi31.PersistedInstanceType.ANY, appTaskIds));
    }

    @Test
    public void testGetPersistedInstanceIds_WithPreFetchedAppTaskIds() {
        MultiWindowTestUtils.enableMultiInstance();

        // Create 2 active instances and 1 inactive instance.
        writeInstanceInfo(
                INSTANCE_ID_0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 0, TASK_ID_5);
        writeInstanceInfo(
                INSTANCE_ID_1, URL_2, /* tabCount= */ 1, /* incognitoTabCount= */ 0, TASK_ID_6);
        writeInstanceInfo(
                INSTANCE_ID_2,
                URL_3,
                /* tabCount= */ 5,
                /* incognitoTabCount= */ 0,
                MultiWindowUtils.INVALID_TASK_ID);

        MultiWindowUtils.setAppTaskIdsForTesting(
                new HashSet<>(Arrays.asList(TASK_ID_5, TASK_ID_6)));

        Set<Integer> appTaskIds = new HashSet<>(Arrays.asList(TASK_ID_5, TASK_ID_6));

        // Verify ACTIVE type returns same result with both overloads.
        assertEquals(
                "getPersistedInstanceIds overload should match no-arg version for ACTIVE type.",
                MultiWindowUtils.getPersistedInstanceIds(PersistedInstanceType.ACTIVE),
                MultiWindowUtils.getPersistedInstanceIds(
                        PersistedInstanceType.ACTIVE, appTaskIds));

        // Verify ANY type returns same result with both overloads.
        assertEquals(
                "getPersistedInstanceIds overload should match no-arg version for ANY type.",
                MultiWindowUtils.getPersistedInstanceIds(PersistedInstanceType.ANY),
                MultiWindowUtils.getPersistedInstanceIds(PersistedInstanceType.ANY, appTaskIds));

        // Verify INACTIVE type returns same result with both overloads.
        assertEquals(
                "getPersistedInstanceIds overload should match no-arg version for INACTIVE type.",
                MultiWindowUtils.getPersistedInstanceIds(PersistedInstanceType.INACTIVE),
                MultiWindowUtils.getPersistedInstanceIds(
                        PersistedInstanceType.INACTIVE, appTaskIds));
    }

    @Test
    public void testGetPersistedInstanceIds_AnyTypeSkipsIpc() {
        MultiWindowTestUtils.enableMultiInstance();

        writeInstanceInfo(
                INSTANCE_ID_0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 0, TASK_ID_5);
        writeInstanceInfo(
                INSTANCE_ID_1, URL_2, /* tabCount= */ 1, /* incognitoTabCount= */ 0, TASK_ID_6);

        // Do not set appTaskIds for testing - this means getAllAppTaskIds() would return empty set.
        // If getPersistedInstanceIds(ANY) correctly skips the IPC call, it should still return
        // all persisted instance IDs regardless.
        MultiWindowUtils.setAppTaskIdsForTesting(new HashSet<>());

        Set<Integer> ids = MultiWindowUtils.getPersistedInstanceIds(PersistedInstanceType.ANY);
        assertEquals(
                "ANY type should return all persisted IDs without needing appTaskIds.",
                2,
                ids.size());
        assertTrue("Should contain INSTANCE_ID_0.", ids.contains(INSTANCE_ID_0));
        assertTrue("Should contain INSTANCE_ID_1.", ids.contains(INSTANCE_ID_1));
    }

    @Test
    public void testShouldShowInstanceSwitcherIph_NonDesktop() {
        DeviceInfo.setIsDesktopForTesting(false);
        MultiWindowTestUtils.enableMultiInstance();

        // 0 instances -> should return false
        assertFalse(MultiWindowUtils.shouldShowInstanceSwitcherIph());

        // 1 instance -> should return false
        writeInstanceInfo(
                INSTANCE_ID_0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 2, TASK_ID_5);
        assertFalse(MultiWindowUtils.shouldShowInstanceSwitcherIph());

        // 2 instances -> should return true
        writeInstanceInfo(
                INSTANCE_ID_1, URL_2, /* tabCount= */ 1, /* incognitoTabCount= */ 0, TASK_ID_6);
        assertTrue(MultiWindowUtils.shouldShowInstanceSwitcherIph());
    }

    @Test
    public void testShouldShowInstanceSwitcherIph_Desktop() {
        DeviceInfo.setIsDesktopForTesting(true);
        MultiWindowTestUtils.enableMultiInstance();

        // 1 instance -> should return false
        writeInstanceInfo(
                INSTANCE_ID_0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 2, TASK_ID_5);
        assertFalse(MultiWindowUtils.shouldShowInstanceSwitcherIph());

        // Create up to 10 instances -> should return false
        for (int i = 1; i < 10; i++) {
            writeInstanceInfo(i, "url" + i, /* tabCount= */ 1, /* incognitoTabCount= */ 0, 100 + i);
        }
        assertFalse(MultiWindowUtils.shouldShowInstanceSwitcherIph());

        // 11 instances -> should return true
        writeInstanceInfo(10, "url10", /* tabCount= */ 1, /* incognitoTabCount= */ 0, 110);
        assertTrue(MultiWindowUtils.shouldShowInstanceSwitcherIph());

        // Reset DeviceInfo setting
        DeviceInfo.setIsDesktopForTesting(false);
    }

    @Test
    public void getInstanceCount_ExceedsLimit() {
        MultiWindowTestUtils.enableMultiInstance();
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        int maxInstances = 3;
        MultiWindowUtils.setMaxInstancesForTesting(maxInstances);

        // Simulate persistence of instance state for max instances = 3.
        MultiWindowUtils.setAppTaskIdsForTesting(
                new HashSet<>(List.of(TASK_ID_5, TASK_ID_6, TASK_ID_7)));
        writeInstanceInfo(
                INSTANCE_ID_0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 2, TASK_ID_5);
        writeInstanceInfo(
                INSTANCE_ID_1, URL_2, /* tabCount= */ 0, /* incognitoTabCount= */ 0, TASK_ID_6);
        writeInstanceInfo(
                INSTANCE_ID_2, URL_3, /* tabCount= */ 6, /* incognitoTabCount= */ 2, TASK_ID_7);

        // Simulate downgrade of instance limit.
        MultiWindowUtils.setMaxInstancesForTesting(maxInstances - 1);

        // Verify instance count.
        assertEquals(3, MultiWindowUtils.getInstanceCount(PersistedInstanceType.ANY));
    }

    @Test
    @Config(sdk = 31)
    public void testGetInstanceIdForViewIntent_LessThanMaxInstancesOpen() {
        MultiWindowTestUtils.enableMultiInstance();
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        int maxInstances = MultiWindowUtils.getMaxInstances();
        // Simulate opening of 1 less than the max number of instances. #writeInstanceInfo will
        // update the access time for IDs 0 -> |maxInstances - 2| in increasing order of recency.
        for (int i = 0; i < maxInstances - 1; i++) {
            addRunningTabbedActivity(i);
            writeInstanceInfo(i, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 0, i);
        }

        int instanceId = MultiWindowUtils.getInstanceIdForViewIntent();
        assertEquals(
                "The last accessed instance ID should be returned when an existing instance is"
                        + " preferred.",
                maxInstances - 2,
                instanceId);
    }

    @Test
    @Config(sdk = 31)
    public void testGetInstanceIdForViewIntent_MaxInstancesOpen_MaxRunningActivities() {
        MultiWindowTestUtils.enableMultiInstance();
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        int maxInstances = MultiWindowUtils.getMaxInstances();
        // Simulate opening of max number of instances. #writeInstanceInfo will update the access
        // time for IDs 0 -> |maxInstances - 1| in increasing order of recency.
        for (int i = 0; i < maxInstances; i++) {
            addRunningTabbedActivity(i);
            writeInstanceInfo(i, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 0, i);
        }

        // Simulate last access of instance ID 0.
        writeInstanceInfo(0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 0, 0);

        int instanceId = MultiWindowUtils.getInstanceIdForViewIntent();
        assertEquals("The last accessed instance ID should be returned.", 0, instanceId);
    }

    @Test
    @Config(sdk = 31)
    public void testGetInstanceIdForViewIntent_MaxInstancesOpen_LessThanMaxRunningActivities() {
        MultiWindowTestUtils.enableMultiInstance();
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        int maxInstances = MultiWindowUtils.getMaxInstances();
        // Simulate opening of max number of instances. #writeInstanceInfo will update the access
        // time for IDs 0 -> |maxInstances - 1| in increasing order of recency.
        Activity firstMockActivity = null;
        for (int i = 0; i < maxInstances; i++) {
            Activity mockActivity = addRunningTabbedActivity(i);
            if (i == 0) {
                firstMockActivity = mockActivity;
            }
            writeInstanceInfo(i, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 0, i);
        }

        // Simulate last access of instance ID 0.
        writeInstanceInfo(0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 0, 0);
        // Simulate destruction of the activity represented by instance ID 0.
        ApplicationStatus.onStateChangeForTesting(firstMockActivity, ActivityState.DESTROYED);

        int instanceId = MultiWindowUtils.getInstanceIdForViewIntent();
        assertEquals(
                "The instance ID of a running activity that was last accessed should be returned.",
                maxInstances - 1,
                instanceId);
    }

    @Test
    @Config(sdk = 31)
    public void testGetInstanceIdForLinkIntent_OnlyConsidersActiveInstances() {
        MultiWindowTestUtils.enableMultiInstance();
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        int maxInstances = MultiWindowUtils.getMaxInstances();
        // Simulate opening of 1 less than the max number of instances. #writeInstanceInfo will
        // update the access time for IDs 0 -> |maxInstances - 2| in increasing order of recency.
        for (int i = 0; i < maxInstances - 1; i++) {
            writeInstanceInfo(i, URL_1, /* tabCount= */ 1, /* incognitoTabCount= */ 0, i);
            addRunningTabbedActivity(i);
        }

        // Create inactive instances to exceed max instances.
        writeInstanceInfo(
                maxInstances - 1,
                URL_2,
                /* tabCount= */ 1,
                /* incognitoTabCount= */ 0,
                MultiWindowUtils.INVALID_TASK_ID);
        writeInstanceInfo(
                maxInstances,
                URL_3,
                /* tabCount= */ 1,
                /* incognitoTabCount= */ 0,
                MultiWindowUtils.INVALID_TASK_ID);

        // Total instances is maxInstances + 1. Active instances is maxInstances - 1. Returns
        // INVALID_WINDOW_ID to allow for new window creation.
        int instanceId = MultiWindowUtils.getInstanceIdForLinkIntent(mock(Activity.class));
        assertEquals(
                "Should return INVALID_WINDOW_ID to allow for new window creation.",
                TabWindowManager.INVALID_WINDOW_ID,
                instanceId);
    }

    @Test
    @Config(sdk = 31)
    public void testRecordDesktopWindowCount_OnlyOnColdStart() {
        MultiWindowTestUtils.enableMultiInstance();
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(true);

        // Simulate persistence of 2 instances, running of 1.
        MultiWindowUtils.setAppTaskIdsForTesting(new HashSet<>(List.of(TASK_ID_5, TASK_ID_6)));
        writeInstanceInfo(
                INSTANCE_ID_0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 2, TASK_ID_5);
        writeInstanceInfo(
                INSTANCE_ID_1, URL_2, /* tabCount= */ 0, /* incognitoTabCount= */ 0, TASK_ID_6);
        int runningActivityCount = 1;
        addRunningTabbedActivity(INSTANCE_ID_0);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW, runningActivityCount)
                        .expectIntRecord(HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW, 2)
                        .build();

        // Assume that the histograms are attempted to be recorded on a cold start of the app.
        MultiWindowUtils.maybeRecordDesktopWindowCountHistograms(
                mDesktopWindowStateManager,
                InstanceAllocationType.NEW_INSTANCE_NEW_TASK,
                /* isColdStart= */ true);

        // Assume that the histograms are attempted to be recorded on a subsequent warm start.
        MultiWindowUtils.maybeRecordDesktopWindowCountHistograms(
                mDesktopWindowStateManager,
                InstanceAllocationType.NEW_INSTANCE_NEW_TASK,
                /* isColdStart= */ false);

        // Each histogram should be emitted only once.
        watcher.assertExpected();
    }

    @Test
    @Config(sdk = 31)
    public void testRecordDesktopWindowCount_ColdStartOfInstance() {
        MultiWindowTestUtils.enableMultiInstance();
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(true);

        // Simulate persistence of 2 instances, running of 1.
        MultiWindowUtils.setAppTaskIdsForTesting(new HashSet<>(List.of(TASK_ID_5, TASK_ID_6)));
        writeInstanceInfo(
                INSTANCE_ID_0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 2, TASK_ID_5);
        writeInstanceInfo(
                INSTANCE_ID_1, URL_2, /* tabCount= */ 0, /* incognitoTabCount= */ 0, TASK_ID_6);
        int runningActivityCount = 1;
        addRunningTabbedActivity(INSTANCE_ID_0);

        int[] instanceAllocationTypes =
                new int[] {
                    InstanceAllocationType.DEFAULT,
                    InstanceAllocationType.EXISTING_INSTANCE_MAPPED_TASK,
                    InstanceAllocationType.EXISTING_INSTANCE_UNMAPPED_TASK,
                    InstanceAllocationType.EXISTING_INSTANCE_NEW_TASK,
                    InstanceAllocationType.NEW_INSTANCE_NEW_TASK,
                    InstanceAllocationType.PREFER_NEW_INSTANCE_NEW_TASK
                };

        // Assume that the histograms are attempted to be recorded on a cold start of an instance,
        // for different instance allocation types.
        for (int type : instanceAllocationTypes) {
            var watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(
                                    HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW, runningActivityCount)
                            .expectIntRecord(HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW, 2)
                            .build();
            MultiWindowUtils.maybeRecordDesktopWindowCountHistograms(
                    mDesktopWindowStateManager, type, /* isColdStart= */ true);
            watcher.assertExpected();
        }
    }

    @Test
    @Config(sdk = 31)
    public void testRecordDesktopWindowCount_NotInDesktopWindow() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(HISTOGRAM_NUM_ACTIVITIES_DESKTOP_WINDOW)
                        .expectNoRecords(HISTOGRAM_NUM_INSTANCES_DESKTOP_WINDOW)
                        .build();

        // Assume that the histograms are attempted to be recorded on a cold start of the app, not
        // in a desktop window.
        MultiWindowUtils.maybeRecordDesktopWindowCountHistograms(
                mDesktopWindowStateManager,
                InstanceAllocationType.NEW_INSTANCE_NEW_TASK,
                /* isColdStart= */ true);

        // Histograms should not be emitted.
        watcher.assertExpected();
    }

    @Test
    public void testGetTabCountForRelaunchFromSharedPrefs() {
        int windowId1 = 0;
        int windowId2 = 1;
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(windowId1);
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(windowId2);
        ChromeMultiInstancePersistentStore.writeTabCountForRelaunchSync(
                windowId1, /* tabCount= */ 10);
        ChromeMultiInstancePersistentStore.writeTabCountForRelaunchSync(
                windowId2, /* tabCount= */ 15);
        assertEquals(
                10, MultiWindowUtils.getTabCountForRelaunchFromPersistentStore(windowId1), 0.01);
        assertEquals(
                15, MultiWindowUtils.getTabCountForRelaunchFromPersistentStore(windowId2), 0.01);
    }

    @Test
    public void testRecordTabCountForRelaunchWhenActivityPaused_MultiInstanceApi31Enabled() {
        MultiWindowTestUtils.enableMultiInstance();
        int windowId = 1;
        testRecordTabCountForRelaunchWhenActivityPausedImpl(windowId);
    }

    @Test
    public void testRecordTabCountForRelaunchWhenActivityPaused_MultiInstanceApi31Disabled() {
        testRecordTabCountForRelaunchWhenActivityPausedImpl(/* windowId= */ 0);
    }

    @Test
    public void testGetLastAccessedWindowId() {
        MultiWindowTestUtils.enableMultiInstance();

        final int oldestId = 10;
        final int midId = 20;
        final int newestId = 30;

        writeInstanceInfo(oldestId, URL_1, 3, 0, TASK_ID_5);
        writeInstanceInfo(midId, URL_3, 1, 0, TASK_ID_6);
        writeInstanceInfo(newestId, "", 0, 0, INVALID_TASK_ID);

        Assert.assertEquals(
                "The last accessed window ID should be returned.",
                newestId,
                MultiWindowUtils.getLastAccessedWindowId());
    }

    @Test
    public void testGetLastAccessedWindowIdExcludingSelf() {
        MultiWindowTestUtils.enableMultiInstance();
        writeInstanceInfo(INSTANCE_ID_0, URL_1, 3, 0, TASK_ID_5);
        writeInstanceInfo(INSTANCE_ID_1, URL_2, 2, 0, TASK_ID_6);
        writeInstanceInfo(INSTANCE_ID_2, URL_3, 1, 0, INVALID_TASK_ID);

        Assert.assertEquals(
                "The last accessed active window ID excluding the current id should be returned.",
                INSTANCE_ID_1,
                MultiWindowUtils.getLastAccessedWindowIdExcludingSelf(
                        INSTANCE_ID_0, PersistedInstanceType.ACTIVE));
    }

    @Test
    public void testInstanceCreationLimitMessage() {
        MultiWindowUtils.setMaxInstancesForTesting(3);
        MessageDispatcher messageDispatcher = mock(MessageDispatcher.class);
        Context context = ApplicationProvider.getApplicationContext();
        CallbackHelper primaryActionCallbackHelper = new CallbackHelper();
        int primaryActionClickCount = primaryActionCallbackHelper.getCallCount();
        CallbackHelper dismissCallbackHelper = new CallbackHelper();
        int dismissClickCount = dismissCallbackHelper.getCallCount();

        MultiWindowUtils.showInstanceCreationLimitMessage(
                messageDispatcher,
                context,
                primaryActionCallbackHelper::notifyCalled,
                dismissCallbackHelper::notifyCalled);

        ArgumentCaptor<PropertyModel> message = ArgumentCaptor.forClass(PropertyModel.class);
        verify(messageDispatcher).enqueueWindowScopedMessage(message.capture(), eq(false));

        Resources resources = context.getResources();
        Assert.assertEquals(
                "Message identifier should match.",
                MessageIdentifier.MULTI_INSTANCE_CREATION_LIMIT,
                message.getValue().get(MessageBannerProperties.MESSAGE_IDENTIFIER));
        Assert.assertEquals(
                "Message title should match.",
                resources.getString(R.string.multi_instance_creation_limit_message_title, 3),
                message.getValue().get(MessageBannerProperties.TITLE));
        Assert.assertEquals(
                "Message description should match.",
                resources.getString(R.string.multi_instance_creation_limit_message_description),
                message.getValue().get(MessageBannerProperties.DESCRIPTION));
        Assert.assertEquals(
                "Message primary button text should match.",
                resources.getString(R.string.multi_instance_message_button),
                message.getValue().get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));
        Assert.assertEquals(
                "Message icon resource ID should match.",
                R.drawable.ic_chrome,
                message.getValue().get(MessageBannerProperties.ICON_RESOURCE_ID));

        // Simulate and verify primary button click.
        var unused = message.getValue().get(MessageBannerProperties.ON_PRIMARY_ACTION).get();
        assertEquals(
                "Primary action callback was not called.",
                primaryActionClickCount + 1,
                primaryActionCallbackHelper.getCallCount());

        // Simulate and verify dismiss.
        message.getValue()
                .get(MessageBannerProperties.ON_DISMISSED)
                .onResult(DismissReason.GESTURE);
        assertEquals(
                "Dismiss callback was not called.",
                dismissClickCount + 1,
                dismissCallbackHelper.getCallCount());
    }

    @Test
    public void testMaxInstances_DefaultValues() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);

        // Verify default instance limit for low-memory device, using default memory threshold.
        SysUtils.setAmountOfPhysicalMemoryKbForTesting(
                4000 * ConversionUtils.KILOBYTES_PER_MEGABYTE);
        assertEquals(
                "Instance limit on low-memory device is incorrect.",
                5,
                MultiWindowUtils.getMaxInstances());

        // Verify default instance limit for high-memory device, using default memory threshold.
        SysUtils.setAmountOfPhysicalMemoryKbForTesting(
                7000 * ConversionUtils.KILOBYTES_PER_MEGABYTE);
        assertEquals(
                "Instance limit on high-memory device is incorrect.",
                20,
                MultiWindowUtils.getMaxInstances());
    }

    @Test
    public void testMaxInstances_DesktopDevice() {
        mOverrideContextWrapperTestRule.setIsDesktop(true);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        assertEquals(
                "Instance limit on desktop device is incorrect.",
                1000,
                MultiWindowUtils.getMaxInstances());
    }

    @Test
    public void testVerifyLatestPersistentStateId_InvalidWindowId() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_PERSISTENT_STATE_ID_VERIFICATION,
                        PersistentStateIdVerification.NO_PERSISTENT_STATE_NOR_ID);
        MultiWindowUtils.verifyLatestPersistentStateId(TabWindowManager.INVALID_WINDOW_ID, null);
        watcher.assertExpected();
    }

    @Test
    public void testVerifyLatestPersistentStateId_NoPersistentStateNorId() {
        int windowId = INSTANCE_ID_0;
        // Ensure no id is stored.
        ChromeMultiInstancePersistentStore.deleteInstanceState(windowId);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_PERSISTENT_STATE_ID_VERIFICATION,
                        PersistentStateIdVerification.NO_PERSISTENT_STATE_NOR_ID);
        MultiWindowUtils.verifyLatestPersistentStateId(windowId, null);
        watcher.assertExpected();
    }

    @Test
    public void testVerifyLatestPersistentStateId_MissingPersistentState() {
        int windowId = INSTANCE_ID_0;
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(windowId);
        ChromeMultiInstancePersistentStore.writeLatestPersistentStateId(windowId, 123);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_PERSISTENT_STATE_ID_VERIFICATION,
                        PersistentStateIdVerification.MISSING_PERSISTENT_STATE);
        MultiWindowUtils.verifyLatestPersistentStateId(windowId, null);
        watcher.assertExpected();
    }

    @Test
    public void testVerifyLatestPersistentStateId_MissingPersistentStateId() {
        int windowId = INSTANCE_ID_0;
        ChromeMultiInstancePersistentStore.deleteInstanceState(windowId);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_PERSISTENT_STATE_ID_VERIFICATION,
                        PersistentStateIdVerification.MISSING_PERSISTENT_STATE_ID);
        MultiWindowUtils.verifyLatestPersistentStateId(windowId, new PersistableBundle());
        watcher.assertExpected();
    }

    @Test
    public void testVerifyLatestPersistentStateId_Match() {
        int windowId = INSTANCE_ID_0;
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(windowId);
        PersistableBundle bundle = new PersistableBundle();
        int persistentStateId = bundle.hashCode();
        bundle.putInt(PERSISTENT_STATE_ID, persistentStateId);
        ChromeMultiInstancePersistentStore.writeLatestPersistentStateId(
                windowId, persistentStateId);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_PERSISTENT_STATE_ID_VERIFICATION,
                        PersistentStateIdVerification.PERSISTENT_STATE_MATCH);
        MultiWindowUtils.verifyLatestPersistentStateId(windowId, bundle);
        watcher.assertExpected();
    }

    @Test
    public void testVerifyLatestPersistentStateId_Mismatch() {
        int windowId = INSTANCE_ID_0;
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(windowId);
        PersistableBundle bundle = new PersistableBundle();
        int persistentStateId = bundle.hashCode();
        bundle.putInt(PERSISTENT_STATE_ID, persistentStateId + 1);
        ChromeMultiInstancePersistentStore.writeLatestPersistentStateId(
                windowId, persistentStateId);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_PERSISTENT_STATE_ID_VERIFICATION,
                        PersistentStateIdVerification.PERSISTENT_STATE_MISMATCH);
        MultiWindowUtils.verifyLatestPersistentStateId(windowId, bundle);
        watcher.assertExpected();
    }

    @Test
    public void testGetForegroundWindowActivity() {
        // Create running tabbed activities. A larger instance id is for an instance with a more
        // recent lastAccessedTime.
        ChromeTabbedActivity activity0 = addRunningTabbedActivity(INSTANCE_ID_0);
        writeInstanceInfo(
                INSTANCE_ID_0, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 0, INSTANCE_ID_0);
        ChromeTabbedActivity activity1 = addRunningTabbedActivity(INSTANCE_ID_1);
        writeInstanceInfo(
                INSTANCE_ID_1, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 0, INSTANCE_ID_1);
        ChromeTabbedActivity activity2 = addRunningTabbedActivity(INSTANCE_ID_2);
        writeInstanceInfo(
                INSTANCE_ID_2, URL_1, /* tabCount= */ 3, /* incognitoTabCount= */ 0, INSTANCE_ID_2);

        assertEquals(
                "Expected activity with more recent lastAccessedTime.",
                activity2,
                MultiWindowUtils.getForegroundWindowActivity(activity0));
    }

    private void testRecordTabCountForRelaunchWhenActivityPausedImpl(int windowId) {
        List<TabModel> models = Arrays.asList(mNormalTabModel, mIncognitoTabModel);
        when(mTabModelSelector.getModels()).thenReturn(models);
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        when(mIncognitoTabModel.iterator()).thenAnswer(inv -> Collections.emptyList().iterator());

        ChromeMultiInstancePersistentStore.writeLastAccessedTime(windowId);

        // Test if recordTabCountForRelaunchWhenActivityPaused() returns the correct value for
        // standard tabs.
        when(mNormalTabModel.getCount()).thenReturn(2);
        when(mNormalTabModel.getTabAtChecked(0)).thenReturn(mTab1);
        when(mNormalTabModel.getTabAtChecked(1)).thenReturn(mTab2);
        when(mTab1.isNativePage()).thenReturn(false);
        when(mTab1.getUrl()).thenReturn(TEST_GURL);
        when(mTab2.isNativePage()).thenReturn(false);
        when(mTab2.getUrl()).thenReturn(TEST_GURL);
        when(mNormalTabModel.iterator()).thenAnswer(inv -> List.of(mTab1, mTab2).iterator());
        MultiWindowUtils.recordTabCountForRelaunchWhenActivityPaused(
                mTabModelSelector, windowId, /* isRecreating= */ false);
        Assert.assertEquals(
                /* expected= */ 2,
                ChromeMultiInstancePersistentStore.readTabCountForRelaunch(windowId));

        // Test the case of adding a non-NTP tab to the tab model.
        when(mNormalTabModel.getCount()).thenReturn(3);
        when(mNormalTabModel.iterator()).thenAnswer(inv -> List.of(mTab1, mTab2, mTab3).iterator());
        when(mNormalTabModel.getTabAtChecked(2)).thenReturn(mTab3);
        when(mTab3.isNativePage()).thenReturn(false);
        when(mTab3.getUrl()).thenReturn(TEST_GURL);
        MultiWindowUtils.recordTabCountForRelaunchWhenActivityPaused(
                mTabModelSelector, windowId, /* isRecreating= */ false);
        Assert.assertEquals(
                /* expected= */ 3,
                ChromeMultiInstancePersistentStore.readTabCountForRelaunch(windowId));

        // Test the case of adding a NTP tab to the tab model.
        when(mTab3.isNativePage()).thenReturn(true);
        when(mTab3.getUrl()).thenReturn(NTP_GURL);
        MultiWindowUtils.recordTabCountForRelaunchWhenActivityPaused(
                mTabModelSelector, windowId, /* isRecreating= */ false);
        Assert.assertEquals(
                /* expected= */ 2,
                ChromeMultiInstancePersistentStore.readTabCountForRelaunch(windowId));

        // Test the case of adding a NTP tab to the tab model when the activity is recreating.
        when(mTab3.isNativePage()).thenReturn(true);
        when(mTab3.getUrl()).thenReturn(NTP_GURL);
        MultiWindowUtils.recordTabCountForRelaunchWhenActivityPaused(
                mTabModelSelector, windowId, /* isRecreating= */ true);
        Assert.assertEquals(
                /* expected= */ 3,
                ChromeMultiInstancePersistentStore.readTabCountForRelaunch(windowId));
    }

    private void writeInstanceInfo(
            int instanceId,
            String url,
            int tabCount,
            int incognitoTabCount,
            int taskId,
            @SupportedProfileType int profileType) {
        // Advance the timestamp for each instance to update its last-accessed time, so the instance
        // list is in chronological order.
        mFakeTimeTestRule.advanceMillis(1);

        ChromeMultiInstancePersistentStore.writeLastAccessedTime(instanceId);
        ChromeMultiInstancePersistentStore.writeActiveTabUrl(instanceId, url);
        ChromeMultiInstancePersistentStore.writeTabCount(instanceId, tabCount, incognitoTabCount);
        ChromeMultiInstancePersistentStore.writeTaskId(instanceId, taskId);
        if (taskId != -1) MultiWindowUtils.addAppTaskIdForTesting(taskId);
        ChromeMultiInstancePersistentStore.writeProfileType(instanceId, profileType);
    }

    private void writeInstanceInfo(
            int instanceId, String url, int tabCount, int incognitoTabCount, int taskId) {
        writeInstanceInfo(
                instanceId, url, tabCount, incognitoTabCount, taskId, SupportedProfileType.MIXED);
    }
}
