// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tabwindow.TabWindowManager.INVALID_WINDOW_ID;

import android.app.Activity;
import android.content.Intent;
import android.content.res.Resources;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.List;

/** Unit tests for {@link MultiInstanceOrchestratorImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 31)
public class MultiInstanceOrchestratorImplUnitTest {
    private static final int SOURCE_WINDOW_ID = 0;
    private static final int DEST_WINDOW_ID = 1;
    private static final int NONEXISTENT_INSTANCE_ID = 14;
    private static final int PARENT_TAB_ID_1 = 1;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private ChromeTabbedActivity mTabbedActivity1;
    @Mock private ChromeTabbedActivity mTabbedActivity2;
    @Mock private MultiInstanceManagerApi31 mMultiInstanceManager1;
    @Mock private MultiInstanceManagerApi31 mMultiInstanceManager2;
    @Mock private TabReparentingDelegate mTabReparentingDelegate;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;

    private MultiInstanceOrchestrator mMultiInstanceOrchestrator;
    private LoadUrlParams mUrlParams;

    @Before
    public void setup() {
        MultiWindowTestUtils.enableMultiInstance();
        MultiInstanceOrchestratorImpl.setTabReparentingDelegateForTesting(mTabReparentingDelegate);
        mMultiInstanceOrchestrator = MultiInstanceOrchestratorImpl.getInstance();
        mMultiInstanceOrchestrator.onInitialize(mTabbedActivity1, mMultiInstanceManager1);
        mMultiInstanceOrchestrator.onInitialize(mTabbedActivity2, mMultiInstanceManager2);
        createActiveInstances(
                /* count= */ 2, SupportedProfileType.MIXED, /* startId= */ SOURCE_WINDOW_ID);

        setupActivityForTab(mTab1, mTabbedActivity1);
        setupActivityForTab(mTab2, mTabbedActivity2);
        when(mTab1.getParentId()).thenReturn(PARENT_TAB_ID_1);

        mUrlParams = new LoadUrlParams(new GURL("about:blank"));

        when(mTabbedActivity1.getPackageName())
                .thenReturn(ContextUtils.getApplicationContext().getPackageName());
        MultiWindowUtils.setActivitySupplierForTesting(() -> mTabbedActivity2);
    }

    @After
    public void teardown() {
        ApplicationStatus.destroyForJUnitTests();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL)
    public void testMoveTabsToNewWindow_validInput_opensAdjacently() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                true);

        // Act.
        mMultiInstanceOrchestrator.moveTabsToNewWindow(
                tabs, /* finalizeCallback= */ null, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabsToNewWindow(
                        tabs,
                        INVALID_WINDOW_ID,
                        /* openAdjacently= */ true,
                        /* finalizeCallback= */ null,
                        NewWindowAppSource.KEYBOARD_SHORTCUT);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL)
    public void testMoveTabsToNewWindow_validInput_opensFullScreen() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                false);

        // Act.
        mMultiInstanceOrchestrator.moveTabsToNewWindow(
                tabs, /* finalizeCallback= */ null, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabsToNewWindow(
                        tabs,
                        INVALID_WINDOW_ID,
                        /* openAdjacently= */ false,
                        /* finalizeCallback= */ null,
                        NewWindowAppSource.KEYBOARD_SHORTCUT);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL)
    public void testMoveTabsToNewWindow_inMultiWindowMode_opensAdjacently() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                false);

        // Act.
        mMultiInstanceOrchestrator.moveTabsToNewWindow(
                tabs, /* finalizeCallback= */ null, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabsToNewWindow(
                        tabs,
                        INVALID_WINDOW_ID,
                        /* openAdjacently= */ false,
                        /* finalizeCallback= */ null,
                        NewWindowAppSource.KEYBOARD_SHORTCUT);
    }

    @Test
    public void testMoveTabsToNewWindow_atInstanceLimit_showsMessageInTabbedActivity() {
        // Setup.
        int maxInstances = 3;
        MultiWindowUtils.setMaxInstancesForTesting(maxInstances);
        MultiWindowUtils.setInstanceCountForTesting(maxInstances);
        List<Tab> tabs = List.of(mTab1, mTab2);
        when(mTabbedActivity1.getResources()).thenReturn(mock(Resources.class));

        // Act.
        mMultiInstanceOrchestrator.moveTabsToNewWindow(
                tabs, /* finalizeCallback= */ null, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify that tab reparenting is not initiated, and a message is shown.
        verify(mTabReparentingDelegate, never())
                .reparentTabsToNewWindow(any(), anyInt(), anyBoolean(), any(), anyInt());
        verify(mMultiInstanceManager1).showInstanceCreationLimitMessage();
    }

    @Test
    public void testMoveTabsToNewWindow_atInstanceLimit_noMessageShownWithNonTabbedActivity() {
        // Setup.
        int maxInstances = 3;
        MultiWindowUtils.setMaxInstancesForTesting(maxInstances);
        MultiWindowUtils.setInstanceCountForTesting(maxInstances);
        List<Tab> tabs = List.of(mTab1);
        setupActivityForTab(mTab1, mActivity);

        // Act.
        mMultiInstanceOrchestrator.moveTabsToNewWindow(
                tabs, /* finalizeCallback= */ null, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify that tab reparenting is not initiated, and a message is shown.
        verify(mTabReparentingDelegate, never())
                .reparentTabsToNewWindow(any(), anyInt(), anyBoolean(), any(), anyInt());
        verify(mMultiInstanceManager1, never()).showInstanceCreationLimitMessage();
    }

    @Test
    public void testMoveTabsToWindowByIdChecked_InvalidParams() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);

        // destWindowId should have persisted instance state.
        assertThrows(
                AssertionError.class,
                () ->
                        mMultiInstanceOrchestrator.moveTabsToWindowByIdChecked(
                                /* destWindowId= */ NONEXISTENT_INSTANCE_ID,
                                tabs,
                                /* destTabIndex= */ 2,
                                /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                                /* bringToFront= */ true));

        // destTabIndex and destGroupTabId should not both be specified when moving tabs to an
        // existing window.
        assertThrows(
                AssertionError.class,
                () ->
                        mMultiInstanceOrchestrator.moveTabsToWindowByIdChecked(
                                DEST_WINDOW_ID,
                                tabs,
                                /* destTabIndex= */ 1,
                                /* destGroupTabId= */ 2,
                                /* bringToFront= */ true));
    }

    @Test
    public void testMoveTabsToWindowByIdChecked_toDestTabIndex() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);
        int destTabIndex = 0;

        // Act.
        mMultiInstanceOrchestrator.moveTabsToWindowByIdChecked(
                DEST_WINDOW_ID,
                tabs,
                destTabIndex,
                /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                /* bringToFront= */ true);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabsToExistingWindow(
                        eq(mTabbedActivity2), eq(tabs), eq(destTabIndex), eq(-1), eq(true));
    }

    @Test
    public void testMoveTabsToWindowByIdChecked_toDestTabGroup() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);
        when(mTab1.getTabGroupId()).thenReturn(null);
        when(mTab2.getTabGroupId()).thenReturn(null);

        // Act.
        mMultiInstanceOrchestrator.moveTabsToWindowByIdChecked(
                DEST_WINDOW_ID,
                tabs,
                /* destTabIndex= */ TabList.INVALID_TAB_INDEX,
                /* destGroupTabId= */ 3,
                /* bringToFront= */ true);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabsToExistingWindow(
                        eq(mTabbedActivity2), eq(tabs), eq(-1), eq(3), eq(true));
    }

    @Test
    public void testMoveTabsToWindowByIdChecked_withDestroyedActivity() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);
        MultiWindowUtils.setActivitySupplierForTesting(() -> null);

        // Act.
        mMultiInstanceOrchestrator.moveTabsToWindowByIdChecked(
                DEST_WINDOW_ID,
                tabs,
                /* destTabIndex= */ 0,
                /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                /* bringToFront= */ true);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabsToNewWindow(
                        eq(tabs),
                        eq(DEST_WINDOW_ID),
                        eq(true),
                        eq(null),
                        eq(NewWindowAppSource.TAB_REPARENTING_TO_INSTANCE_WITH_NO_ACTIVITY));
    }

    @Test
    public void testOpenUrlInOtherWindow_regularTab_showsDialog() {
        doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito*/ false,
                /* preferNew= */ false,
                /* atInstanceLimit= */ false,
                /* eligibleOtherWindowExists= */ true);
    }

    @Test
    public void testOpenUrlInOtherWindow_regularTab_noEligibleWindow_createsNewWindow() {
        doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito*/ false,
                /* preferNew= */ false,
                /* atInstanceLimit= */ false,
                /* eligibleOtherWindowExists= */ false);
    }

    @Test
    public void testOpenUrlInOtherWindow_regularTab_preferNew_createsNewWindow() {
        doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito*/ false,
                /* preferNew= */ true,
                /* atInstanceLimit= */ false,
                /* eligibleOtherWindowExists= */ true);
    }

    @Test
    public void testOpenUrlInOtherWindow_regularTab_preferNew_showsMessageAtInstanceLimit() {
        doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito*/ false,
                /* preferNew= */ true,
                /* atInstanceLimit= */ true,
                /* eligibleOtherWindowExists= */ true);
    }

    @Test
    public void testOpenUrlInOtherWindow_incognitoTab_showsDialog() {
        doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
                /* isSourceIncognito*/ true,
                /* preferNew= */ false,
                /* atInstanceLimit= */ false,
                /* eligibleOtherWindowExists= */ true);
    }

    @Test
    public void testOpenUrlInOtherWindow_incognitoTab_noEligibleWindow_createsNewWindow() {
        doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
                /* isSourceIncognito*/ true,
                /* preferNew= */ false,
                /* atInstanceLimit= */ false,
                /* eligibleOtherWindowExists= */ false);
    }

    @Test
    public void testOpenUrlInOtherWindow_incognitoTab_preferNew_createsNewWindow() {
        doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
                /* isSourceIncognito*/ true,
                /* preferNew= */ true,
                /* atInstanceLimit= */ false,
                /* eligibleOtherWindowExists= */ true);
    }

    @Test
    public void testOpenUrlInOtherWindow_incognitoTab_preferNew_showsMessageAtInstanceLimit() {
        doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
                /* isSourceIncognito*/ true,
                /* preferNew= */ true,
                /* atInstanceLimit= */ true,
                /* eligibleOtherWindowExists= */ true);
    }

    private void doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
            boolean isIncognito,
            boolean preferNew,
            boolean atInstanceLimit,
            boolean eligibleOtherWindowExists) {
        // Setup.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        when(mTab1.isIncognitoBranded()).thenReturn(isIncognito);

        // Setup: Clear existing instance state for mTabbedActivity1 and mTabbedActivity2.
        MultiWindowTestUtils.resetInstanceInfo();

        int numInstances = 2;
        if (atInstanceLimit) {
            MultiWindowUtils.setMaxInstancesForTesting(3);
            numInstances = 3;
        } else if (!eligibleOtherWindowExists) {
            numInstances = 1;
            // Setup: Make mTabbedActivity2 an ineligible profile type window.
            createActiveInstances(
                    /* count= */ 1,
                    isIncognito
                            ? SupportedProfileType.REGULAR
                            : SupportedProfileType.OFF_THE_RECORD,
                    /* startId= */ DEST_WINDOW_ID);
        }

        // This will overwrite instance state for mTabbedActivity1 and mTabbedActivity2.
        createActiveInstances(
                /* count= */ numInstances,
                isIncognito ? SupportedProfileType.OFF_THE_RECORD : SupportedProfileType.REGULAR,
                /* startId= */ SOURCE_WINDOW_ID);

        // Act.
        mMultiInstanceOrchestrator.openUrlInOtherWindow(mTab1, mUrlParams, preferNew);

        // Verify.
        if (preferNew && atInstanceLimit) {
            verify(mMultiInstanceManager1).showInstanceCreationLimitMessage();
        } else if (preferNew || !eligibleOtherWindowExists) {
            var intentCaptor = ArgumentCaptor.forClass(Intent.class);
            verify(mTabbedActivity1).startActivity(intentCaptor.capture());
            assertEquals(
                    "New window source extra is incorrect.",
                    NewWindowAppSource.URL_LAUNCH,
                    intentCaptor
                            .getValue()
                            .getIntExtra(
                                    IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE,
                                    NewWindowAppSource.UNKNOWN));
        } else {
            verify(mMultiInstanceManager1)
                    .showTargetSelectorDialog(
                            any(),
                            eq(
                                    PersistedInstanceType.ACTIVE
                                            | (isIncognito
                                                    ? PersistedInstanceType.OFF_THE_RECORD
                                                    : PersistedInstanceType.REGULAR)),
                            eq(R.string.contextmenu_open_in_other_window));
        }
    }

    private static void createActiveInstances(
            int count, @SupportedProfileType int profileType, int startId) {
        for (int i = startId; i < startId + count; i++) {
            MultiWindowTestUtils.createInstance(
                    i, /* url= */ "https://www.google.com", /* tabCount= */ 1, i);
            ChromeMultiInstancePersistentStore.writeProfileType(i, profileType);
        }
    }

    private void setupActivityForTab(Tab tab, Activity activity) {
        WebContents webContents = mock(WebContents.class);
        WindowAndroid windowAndroid = mock(WindowAndroid.class);
        when(tab.getWebContents()).thenReturn(webContents);
        when(webContents.getTopLevelNativeWindow()).thenReturn(windowAndroid);
        when(windowAndroid.getActivity()).thenReturn(new WeakReference<>(activity));
    }
}
