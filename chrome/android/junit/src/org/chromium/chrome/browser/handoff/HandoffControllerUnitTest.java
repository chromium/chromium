// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.handoff;

import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.app.ChromeActivity.HANDOFF_SDK_VERSION;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.os.UserManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.url.GURL;

import java.lang.reflect.Method;
import java.util.Collections;

/** Unit tests for {@link HandoffController}. */
@RunWith(BaseRobolectricTestRunner.class)
// TODO(crbug.com/503422619): Update to 37 once its available, and remove the delegate in
//  HandoffController.java.
@Config(manifest = Config.NONE, sdk = 35)
public class HandoffControllerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private UserManager mUserManager;
    @Mock private Tab mTab;
    @Mock private HandoffController.Delegate mDelegate;

    private ActivityTabProvider mActivityTabProvider;
    private HandoffController mController;
    private Bundle mUserRestrictions;
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        mUserActionTester = new UserActionTester();
        // Force SDK_INT to HANDOFF_SDK_VERSION so the controller logic executes.
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", HANDOFF_SDK_VERSION);

        mActivityTabProvider = new ActivityTabProvider();
        mUserRestrictions = new Bundle();

        when(mActivity.getSystemService(Context.USER_SERVICE)).thenReturn(mUserManager);
        when(mUserManager.getUserRestrictions()).thenReturn(mUserRestrictions);
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(false);
        when(mTabModelSelector.getModels()).thenReturn(Collections.emptyList());
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTab.isIncognitoBranded()).thenReturn(false);
        when(mTab.getUrl()).thenReturn(new GURL("https://example.com"));

        mActivityTabProvider.setForTesting(mTab);
    }

    private void initializeController() {
        mController =
                new HandoffController(
                        mActivity, mTabModelSelector, mActivityTabProvider, mDelegate);
        ShadowLooper.idleMainLooper();
    }

    @Test
    public void testUpdateHandoffState_Initialization_Enabled() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Android.Handoff.Enabled.TabSwitch", true);
        initializeController();

        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(true));
        watcher.assertExpected();
    }

    @Test
    public void testUpdateHandoffState_Initialization_Incognito_Disabled() {
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(true);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.Handoff.Enabled.TabSwitch")
                        .build();
        initializeController();

        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(false));
        watcher.assertExpected();
    }

    @Test
    public void testUpdateHandoffState_Initialization_Policy_Disabled() {
        mUserRestrictions.putBoolean("disallow_handoff", true);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.Handoff.Enabled.TabSwitch")
                        .build();
        initializeController();

        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(false));
        watcher.assertExpected();
    }

    @Test
    public void testOnObservingDifferentTab_Enabled() {
        initializeController();
        when(mDelegate.isHandoffEnabled(mActivity)).thenReturn(false);
        clearInvocations(mDelegate);

        Tab tabB = mock(Tab.class);
        when(tabB.isIncognitoBranded()).thenReturn(false);
        when(tabB.getUrl()).thenReturn(new GURL("https://google.com"));

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Android.Handoff.Enabled.TabSwitch", true);

        mActivityTabProvider.setForTesting(tabB);
        ShadowLooper.idleMainLooper();

        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(true));
        watcher.assertExpected();
    }

    @Test
    public void testOnHandoffActivityDataRequested_Success() throws Exception {
        initializeController();
        Object mockRequest = mock(Class.forName("android.app.HandoffActivityDataRequestInfo"));
        callOnHandoffActivityDataRequested(mockRequest);

        verify(mDelegate).buildHandoffActivityData(eq(mActivity), eq("https://example.com/"));
        assertTrue(mUserActionTester.getActions().contains("HandoffDataRequested"));
    }

    @Test
    public void testOnHandoffActivityDataRequested_IncognitoTab_ReturnsNull() throws Exception {
        initializeController();
        when(mTab.isOffTheRecord()).thenReturn(true);

        var data = callOnHandoffActivityDataRequested(null);

        assertNull(data);
    }

    @Test
    public void testOnHandoffActivityDataRequested_NoTab_ReturnsNull() throws Exception {
        initializeController();
        mActivityTabProvider.setForTesting(null);

        var data = callOnHandoffActivityDataRequested(null);

        assertNull(data);
    }

    @Test
    public void testOnChange_TriggersUpdate() {
        initializeController();
        when(mDelegate.isHandoffEnabled(mActivity)).thenReturn(true);

        // Change state to incognito
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(true);

        // Trigger observer
        mController.onChange();

        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(false));
        assertTrue(mUserActionTester.getActions().contains("HandoffDisabled"));
    }

    @Test
    public void testOnUrlUpdated_MultipleTimes_SameUrl_TriggersOnlyOneUpdate() {
        initializeController();
        when(mDelegate.isHandoffEnabled(mActivity)).thenReturn(false);
        clearInvocations(mDelegate);

        GURL newUrl = new GURL("https://new.url");
        when(mTab.getUrl()).thenReturn(newUrl);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Handoff.Enabled.UrlNavigation", true);
        mController.getActiveTabObserverForTesting().onUrlUpdated(mTab);
        watcher.assertExpected();

        mController.getActiveTabObserverForTesting().onUrlUpdated(mTab);

        // Now, multiple onUrlUpdated calls with same URL should only trigger one update.
        verify(mDelegate, times(1)).setHandoffEnabled(eq(mActivity), eq(true));
    }

    @Test
    public void testOnUrlUpdated_AfterTabSwitch_TriggersOnlyOneUpdate() {
        initializeController();
        when(mDelegate.isHandoffEnabled(mActivity)).thenReturn(true);
        clearInvocations(mDelegate);

        // 2. Switch to Tab B (URL: google.com)
        Tab tabB = mock(Tab.class);
        when(tabB.getUrl()).thenReturn(new GURL("https://google.com"));

        mActivityTabProvider.setForTesting(tabB);
        ShadowLooper.idleMainLooper();

        // Verify toggle - Force a state reset by toggling to false before re-enabling Handoff.
        // Handoff is already enabled, so it should reset.
        InOrder inOrder = inOrder(mDelegate);
        inOrder.verify(mDelegate).setHandoffEnabled(eq(mActivity), eq(false));
        inOrder.verify(mDelegate).setHandoffEnabled(eq(mActivity), eq(true));
        clearInvocations(mDelegate);

        // 3. onUrlUpdated fires for Tab B.
        // It should be deduplicated because onObservingDifferentTab already set mTabLastUrlSeen to
        // google.com.
        mController.getActiveTabObserverForTesting().onUrlUpdated(tabB);
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void testOnUrlUpdated_AfterIncognitoToggle_NotTriggered() {
        initializeController();
        when(mDelegate.isHandoffEnabled(mActivity)).thenReturn(true);
        clearInvocations(mDelegate);

        // 2. Toggle incognito (disables handoff)
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(true);
        mController.onChange();
        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(false));
        clearInvocations(mDelegate);

        // 3. Toggle back (enables handoff)
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(false);
        when(mDelegate.isHandoffEnabled(mActivity)).thenReturn(false);
        mController.onChange();
        verify(mDelegate, atLeastOnce()).setHandoffEnabled(eq(mActivity), eq(true));
        clearInvocations(mDelegate);

        // 4. onUrlUpdated fires with same URL. Should be deduplicated.
        mController.getActiveTabObserverForTesting().onUrlUpdated(mTab);
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void testOnUrlUpdated_InIncognito_NotTriggered() {
        initializeController();
        // 1. Setup an incognito tab
        Tab incognitoTab = mock(Tab.class);
        when(incognitoTab.isIncognitoBranded()).thenReturn(true);
        when(incognitoTab.getUrl()).thenReturn(new GURL("https://incognito.com"));

        // 2. Switch to it (this will call onObservingDifferentTab and disable handoff)
        mActivityTabProvider.setForTesting(incognitoTab);
        ShadowLooper.idleMainLooper();
        clearInvocations(mDelegate);

        // 3. Trigger URL update on the incognito tab
        mController.getActiveTabObserverForTesting().onUrlUpdated(incognitoTab);

        // 4. Verify that updateHandoffState was NEVER called
        verifyNoMoreInteractions(mDelegate);
    }

    private Object callOnHandoffActivityDataRequested(Object requestInfo) throws Exception {
        Method method =
                HandoffController.class.getDeclaredMethod(
                        "onHandoffActivityDataRequested",
                        Class.forName("android.app.HandoffActivityDataRequestInfo"));
        return method.invoke(mController, requestInfo);
    }
}
