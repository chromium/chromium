// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tabwindow.TabWindowManager.INVALID_WINDOW_ID;

import android.app.Activity;
import android.app.ActivityManager.AppTask;
import android.app.ApplicationExitInfo;
import android.content.Intent;
import android.os.Bundle;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.Token;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTabsTask;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadataExtractor;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.MockitoHelper;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.List;

/** Unit tests for {@link MultiInstanceOrchestratorImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 31)
@EnableFeatures({
    ChromeFeatureList.SESSION_RESTORE_AFTER_CRASH,
    ChromeFeatureList.INCOGNITO_AS_WINDOW_FULL_SCREEN
})
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
    @Mock private TabbedCrashRecoveryDelegate mTabbedCrashRecoveryDelegate;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private TabModel mTabModel;
    @Mock private TabModelSelector mTabModelSelector1;

    @Spy private MultiWindowUtils mMultiWindowUtils;

    private MultiInstanceOrchestrator mMultiInstanceOrchestrator;
    private MonotonicObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private TabGroupMetadata mTabGroupMetadata;
    private LoadUrlParams mUrlParams;

    @Before
    public void setup() {
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);
        MultiWindowTestUtils.enableMultiInstance();
        MultiWindowUtils.setInstanceForTesting(mMultiWindowUtils);
        MultiInstanceOrchestratorImpl.setTabReparentingDelegateForTesting(mTabReparentingDelegate);
        TabbedCrashRecoveryDelegate.setInstanceForTesting(mTabbedCrashRecoveryDelegate);
        mModalDialogManagerSupplier = ObservableSuppliers.createMonotonic();
        mMultiInstanceOrchestrator = MultiInstanceOrchestratorImpl.getInstance();
        mMultiInstanceOrchestrator.onInitialize(mTabbedActivity1, mMultiInstanceManager1);
        mMultiInstanceOrchestrator.onInitialize(mTabbedActivity2, mMultiInstanceManager2);
        when(mMultiInstanceManager1.getCurrentInstanceId()).thenReturn(SOURCE_WINDOW_ID);
        when(mMultiInstanceManager2.getCurrentInstanceId()).thenReturn(DEST_WINDOW_ID);
        createActiveInstances(
                /* count= */ 2, SupportedProfileType.MIXED, /* startId= */ SOURCE_WINDOW_ID);

        setupActivityForTab(mTab1, mTabbedActivity1);
        setupActivityForTab(mTab2, mTabbedActivity2);
        when(mTab1.getParentId()).thenReturn(PARENT_TAB_ID_1);

        mUrlParams = new LoadUrlParams(JUnitTestGURLs.EXAMPLE_URL);
        setupTabGroupMetadata(/* isIncognito= */ false);

        var packageName = ContextUtils.getApplicationContext().getPackageName();
        when(mActivity.getPackageName()).thenReturn(packageName);

        when(mTabbedActivity1.getPackageName()).thenReturn(packageName);
        when(mTabbedActivity1.getResources())
                .thenReturn(ContextUtils.getApplicationContext().getResources());
        when(mTabbedActivity1.getTabModelSelector()).thenReturn(mTabModelSelector1);
        when(mTabbedActivity1.getModalDialogManagerSupplier())
                .thenReturn(mModalDialogManagerSupplier);
        when(mTabModelSelector1.getTotalTabCount()).thenReturn(5);

        MultiWindowUtils.setActivityByWindowIdForTesting(SOURCE_WINDOW_ID, mTabbedActivity1);
        MultiWindowUtils.setActivityByWindowIdForTesting(DEST_WINDOW_ID, mTabbedActivity2);
    }

    @After
    public void teardown() {
        ApplicationStatus.destroyForJUnitTests();
        MultiWindowTestUtils.resetInstanceInfo();
    }

    @Test
    public void testCreateNewWindow_unsupportedSourceActivity_noOp_preApi31() {
        // Setup.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        doReturn(true).when(mActivity).isInMultiWindowMode();

        // Act.
        boolean result =
                mMultiInstanceOrchestrator.createNewWindow(
                        mActivity,
                        /* isIncognito= */ false,
                        /* additionalIntentExtras= */ null,
                        /* startActivityOptions= */ null,
                        NewWindowAppSource.UNKNOWN);

        // Verify.
        assertFalse(result);
        verify(mActivity, never()).startActivity(any());
    }

    @Test
    public void testCreateNewWindow_withIntentExtrasBundle_updatesBasicIntent() {
        // Setup.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        Bundle extrasBundle = new Bundle();
        extrasBundle.putInt("my_extra", 1);

        // Act.
        boolean result =
                mMultiInstanceOrchestrator.createNewWindow(
                        mActivity,
                        /* isIncognito= */ false,
                        extrasBundle,
                        /* startActivityOptions= */ null,
                        NewWindowAppSource.BROWSER_WINDOW_CREATOR);

        // Verify.
        assertTrue(result);
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mActivity).startActivity(intentCaptor.capture(), eq(null));
        assertEquals(
                "Intent consumer update failed.",
                1,
                intentCaptor.getValue().getIntExtra("my_extra", 0));
    }

    @Test
    public void testCreateNewWindow_startsActivityWithBundle() {
        // Setup.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        Bundle startActivityBundle = mock(Bundle.class);

        // Act.
        boolean result =
                mMultiInstanceOrchestrator.createNewWindow(
                        mActivity,
                        /* isIncognito= */ false,
                        /* additionalIntentExtras= */ null,
                        startActivityBundle,
                        NewWindowAppSource.BROWSER_WINDOW_CREATOR);

        // Verify.
        assertTrue(result);
        verify(mActivity).startActivity(any(), eq(startActivityBundle));
    }

    @Test
    public void testCreateNewWindow_startActivityThrows() {
        // Setup.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        doThrow(new RuntimeException()).when(mActivity).startActivity(any(), any());

        // Act.
        boolean result =
                mMultiInstanceOrchestrator.createNewWindow(
                        mActivity,
                        /* isIncognito= */ false,
                        /* additionalIntentExtras= */ null,
                        /* startActivityOptions= */ null,
                        NewWindowAppSource.BROWSER_WINDOW_CREATOR);

        // Verify.
        assertFalse(result);
    }

    @Test
    public void testCreateNewWindowFromWebContents_instanceLimit_showsMessage() {
        // Setup.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        MultiWindowUtils.setMaxInstancesForTesting(2);
        Profile profile = mock(Profile.class);
        WebContents webContents = mock(WebContents.class);

        // Act.
        boolean result =
                mMultiInstanceOrchestrator.createNewWindowFromWebContents(
                        mTabbedActivity1,
                        profile,
                        webContents,
                        /* additionalIntentExtras= */ null,
                        /* startActivityOptions= */ null,
                        NewWindowAppSource.BROWSER_WINDOW_CREATOR);

        // Verify.
        assertFalse(result);
        verify(mMultiInstanceManager1).showInstanceCreationLimitMessage();
    }

    @Test
    public void testCreateNewWindowFromWebContents() {
        // Setup.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        MultiWindowUtils.setMaxInstancesForTesting(5);
        Profile profile = mock(Profile.class);
        WebContents webContents = mock(WebContents.class);
        when(mTabReparentingDelegate.createNewWindowFromWebContents(
                        any(), any(), any(), any(), any(), anyInt()))
                .thenReturn(true);

        // Act.
        boolean result =
                mMultiInstanceOrchestrator.createNewWindowFromWebContents(
                        mTabbedActivity1,
                        profile,
                        webContents,
                        /* additionalIntentExtras= */ null,
                        /* startActivityOptions= */ null,
                        NewWindowAppSource.BROWSER_WINDOW_CREATOR);

        // Verify.
        assertTrue(result);
        verify(mMultiInstanceManager1, never()).showInstanceCreationLimitMessage();
        verify(mTabReparentingDelegate)
                .createNewWindowFromWebContents(
                        eq(mTabbedActivity1),
                        eq(profile),
                        eq(webContents),
                        eq(null),
                        eq(null),
                        eq(NewWindowAppSource.BROWSER_WINDOW_CREATOR));
    }

    @Test
    public void testCreateNewWindowFromWebContents_withExtrasAndOptions() {
        // Setup.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        MultiWindowUtils.setMaxInstancesForTesting(5);
        Profile profile = mock(Profile.class);
        WebContents webContents = mock(WebContents.class);
        when(mTabReparentingDelegate.createNewWindowFromWebContents(
                        any(), any(), any(), any(), any(), anyInt()))
                .thenReturn(true);

        Bundle extras = new Bundle();
        extras.putInt("extra", 1);
        Bundle options = mock(Bundle.class);

        // Act.
        boolean result =
                mMultiInstanceOrchestrator.createNewWindowFromWebContents(
                        mTabbedActivity1,
                        profile,
                        webContents,
                        extras,
                        options,
                        NewWindowAppSource.BROWSER_WINDOW_CREATOR);

        // Verify.
        assertTrue(result);
        verify(mMultiInstanceManager1, never()).showInstanceCreationLimitMessage();
        verify(mTabReparentingDelegate)
                .createNewWindowFromWebContents(
                        eq(mTabbedActivity1),
                        eq(profile),
                        eq(webContents),
                        eq(extras),
                        eq(options),
                        eq(NewWindowAppSource.BROWSER_WINDOW_CREATOR));
    }

    @Test
    public void testMoveTabsToNewWindow_validInput() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);

        // Act.
        mMultiInstanceOrchestrator.moveTabsToNewWindow(
                mTabbedActivity1,
                tabs,
                /* finalizeCallback= */ null,
                NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabsToNewWindow(
                        mTabbedActivity1,
                        tabs,
                        INVALID_WINDOW_ID,
                        /* openAdjacently= */ true,
                        /* finalizeCallback= */ null,
                        NewWindowAppSource.KEYBOARD_SHORTCUT);
    }

    @Test
    public void testMoveTabsToNewWindow_sourceWindowEmpty_opensFullScreen() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);
        when(mTabModelSelector1.getTotalTabCount()).thenReturn(2);

        // Act.
        mMultiInstanceOrchestrator.moveTabsToNewWindow(
                mTabbedActivity1,
                tabs,
                /* finalizeCallback= */ null,
                NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabsToNewWindow(
                        mTabbedActivity1,
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

        // Act.
        mMultiInstanceOrchestrator.moveTabsToNewWindow(
                mTabbedActivity1,
                tabs,
                /* finalizeCallback= */ null,
                NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify that tab reparenting is not initiated, and a message is shown.
        verify(mTabReparentingDelegate, never())
                .reparentTabsToNewWindow(any(), any(), anyInt(), anyBoolean(), any(), anyInt());
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
                mActivity,
                tabs,
                /* finalizeCallback= */ null,
                NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify that tab reparenting is not initiated, and a message is shown.
        verify(mTabReparentingDelegate, never())
                .reparentTabsToNewWindow(any(), any(), anyInt(), anyBoolean(), any(), anyInt());
        verify(mMultiInstanceManager1, never()).showInstanceCreationLimitMessage();
    }

    @Test
    public void testMoveTabsToWindowByIdChecked_invalidParams() {
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
        MultiWindowUtils.setActivityByWindowIdForTesting(DEST_WINDOW_ID, /* activity= */ null);

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
                        eq(mTabbedActivity1),
                        eq(tabs),
                        eq(DEST_WINDOW_ID),
                        eq(true),
                        eq(null),
                        eq(NewWindowAppSource.TAB_REPARENTING_TO_INSTANCE_WITH_NO_ACTIVITY));
    }

    @Test
    public void
            testMoveTabsToWindowByIdChecked_withDestroyedActivity_sourceWindowEmpty_opensFullScreen() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);
        MultiWindowUtils.setActivityByWindowIdForTesting(DEST_WINDOW_ID, /* activity= */ null);
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                true);
        when(mTabModelSelector1.getTotalTabCount()).thenReturn(2);

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
                        eq(mTabbedActivity1),
                        eq(tabs),
                        eq(DEST_WINDOW_ID),
                        eq(false),
                        eq(null),
                        eq(NewWindowAppSource.TAB_REPARENTING_TO_INSTANCE_WITH_NO_ACTIVITY));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testMoveTabsToOtherWindow_showsDialog() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);

        // Act.
        mMultiInstanceOrchestrator.moveTabsToOtherWindow(tabs, NewWindowAppSource.MENU);

        // Verify.
        ArgumentCaptor<Callback<InstanceInfo>> callbackCaptor = MockitoHelper.callbackCaptor();
        verify(mMultiInstanceManager1)
                .showTargetSelectorDialog(
                        callbackCaptor.capture(),
                        eq(PersistedInstanceType.ACTIVE),
                        eq(R.string.menu_move_tab_to_other_window));
        callbackCaptor
                .getValue()
                .onResult(getTestInstanceInfo(DEST_WINDOW_ID, /* isIncognito= */ false));
        verify(mTabReparentingDelegate)
                .reparentTabsToExistingWindow(
                        eq(mTabbedActivity2), eq(tabs), eq(-1), eq(-1), eq(true));
        verify(mMultiInstanceManager1).closeChromeWindowIfEmpty(SOURCE_WINDOW_ID);
    }

    @Test
    public void testMoveTabsToOtherWindow_regularTabs_noEligibleWindow_createsNewWindow() {
        doTestMoveTabsToOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito= */ false, /* eligibleOtherWindowExists= */ false);
    }

    @Test
    public void testMoveTabsToOtherWindow_regularTabs_showsDialog() {
        doTestMoveTabsToOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito= */ false, /* eligibleOtherWindowExists= */ true);
    }

    @Test
    public void testMoveTabsToOtherWindow_incognitoTabs_noEligibleWindow_createsNewWindow() {
        doTestMoveTabsToOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito= */ true, /* eligibleOtherWindowExists= */ false);
    }

    @Test
    public void testMoveTabsToOtherWindow_incognitoTabs_showsDialog() {
        doTestMoveTabsToOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito= */ true, /* eligibleOtherWindowExists= */ true);
    }

    @Test
    public void testMoveTabsToOtherWindow_preApi31() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
        var reparentingTabsTask = mock(ReparentingTabsTask.class);
        ReparentingTabsTask.setReparentingTabsTaskForTesting(reparentingTabsTask);
        when(reparentingTabsTask.begin(any(), any(), any(), any())).thenReturn(true);

        mMultiInstanceOrchestrator.moveTabsToOtherWindow(
                List.of(mTab1, mTab2), NewWindowAppSource.MENU);

        verify(reparentingTabsTask).begin(eq(mTabbedActivity1), any(), eq(null), eq(null));
    }

    @Test
    public void testMoveTabGroupToNewWindow_validInput() {
        // Act.
        mMultiInstanceOrchestrator.moveTabGroupToNewWindow(
                mTabGroupMetadata, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabGroupToNewWindow(
                        mTabGroupMetadata,
                        INVALID_WINDOW_ID,
                        /* openAdjacently= */ true,
                        NewWindowAppSource.KEYBOARD_SHORTCUT);
    }

    @Test
    public void testMoveTabGroupToNewWindow_sourceWindowEmpty_opensFullScreen() {
        // Setup.
        when(mTabModelSelector1.getTotalTabCount()).thenReturn(2);

        // Act.
        mMultiInstanceOrchestrator.moveTabGroupToNewWindow(
                mTabGroupMetadata, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabGroupToNewWindow(
                        mTabGroupMetadata,
                        INVALID_WINDOW_ID,
                        /* openAdjacently= */ false,
                        NewWindowAppSource.KEYBOARD_SHORTCUT);
    }

    @Test
    public void testMoveTabGroupToNewWindow_atInstanceLimit_showsMessageInTabbedActivity() {
        // Setup.
        int maxInstances = 3;
        MultiWindowUtils.setMaxInstancesForTesting(maxInstances);
        MultiWindowUtils.setInstanceCountForTesting(maxInstances);

        // Act.
        mMultiInstanceOrchestrator.moveTabGroupToNewWindow(
                mTabGroupMetadata, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify.
        verify(mTabReparentingDelegate, never())
                .reparentTabGroupToNewWindow(any(), anyInt(), anyBoolean(), anyInt());
        verify(mMultiInstanceManager1).showInstanceCreationLimitMessage();
    }

    @Test
    public void testMoveTabGroupToNewWindow_atInstanceLimit_noMessageShownWithNonTabbedActivity() {
        // Setup.
        int maxInstances = 3;
        MultiWindowUtils.setMaxInstancesForTesting(maxInstances);
        MultiWindowUtils.setInstanceCountForTesting(maxInstances);
        MultiWindowUtils.setActivityByWindowIdForTesting(SOURCE_WINDOW_ID, mActivity);

        // Act.
        mMultiInstanceOrchestrator.moveTabGroupToNewWindow(
                mTabGroupMetadata, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify.
        verify(mTabReparentingDelegate, never())
                .reparentTabGroupToNewWindow(any(), anyInt(), anyBoolean(), anyInt());
        verify(mMultiInstanceManager1, never()).showInstanceCreationLimitMessage();
    }

    @Test
    public void testMoveTabGroupToWindowByIdChecked_toActiveDestWindow() {
        // Act.
        int destTabIndex = 0;
        mMultiInstanceOrchestrator.moveTabGroupToWindowByIdChecked(
                DEST_WINDOW_ID, mTabGroupMetadata, destTabIndex, /* bringToFront= */ true);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabGroupToExistingWindow(
                        eq(mTabbedActivity2), eq(mTabGroupMetadata), eq(destTabIndex), eq(true));
    }

    @Test
    public void testMoveTabGroupToWindowByIdChecked_withDestroyedActivity() {
        // Setup.
        MultiWindowUtils.setActivityByWindowIdForTesting(DEST_WINDOW_ID, /* activity= */ null);

        // Act.
        mMultiInstanceOrchestrator.moveTabGroupToWindowByIdChecked(
                DEST_WINDOW_ID, mTabGroupMetadata, /* destTabIndex= */ 0, /* bringToFront= */ true);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabGroupToNewWindow(
                        mTabGroupMetadata,
                        DEST_WINDOW_ID,
                        /* openAdjacently= */ true,
                        NewWindowAppSource.TAB_REPARENTING_TO_INSTANCE_WITH_NO_ACTIVITY);
    }

    @Test
    public void
            testMoveTabGroupToWindowByIdChecked_withDestroyedActivity_sourceWindowEmpty_opensFullScreen() {
        // Setup.
        MultiWindowUtils.setActivityByWindowIdForTesting(DEST_WINDOW_ID, /* activity= */ null);
        when(mTabModelSelector1.getTotalTabCount()).thenReturn(2);

        // Act.
        mMultiInstanceOrchestrator.moveTabGroupToWindowByIdChecked(
                DEST_WINDOW_ID, mTabGroupMetadata, /* destTabIndex= */ 0, /* bringToFront= */ true);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabGroupToNewWindow(
                        mTabGroupMetadata,
                        DEST_WINDOW_ID,
                        /* openAdjacently= */ false,
                        NewWindowAppSource.TAB_REPARENTING_TO_INSTANCE_WITH_NO_ACTIVITY);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testMoveTabGroupToOtherWindow_showsDialog() {
        // Act.
        mMultiInstanceOrchestrator.moveTabGroupToOtherWindow(
                mTabGroupMetadata, NewWindowAppSource.MENU);

        // Verify.
        ArgumentCaptor<Callback<InstanceInfo>> callbackCaptor = MockitoHelper.callbackCaptor();
        verify(mMultiInstanceManager1)
                .showTargetSelectorDialog(
                        callbackCaptor.capture(),
                        eq(PersistedInstanceType.ACTIVE),
                        eq(R.string.menu_move_group_to_other_window));
        callbackCaptor
                .getValue()
                .onResult(getTestInstanceInfo(DEST_WINDOW_ID, /* isIncognito= */ false));
        verify(mTabReparentingDelegate)
                .reparentTabGroupToExistingWindow(
                        eq(mTabbedActivity2), eq(mTabGroupMetadata), eq(-1), eq(true));
        verify(mMultiInstanceManager1).closeChromeWindowIfEmpty(SOURCE_WINDOW_ID);
    }

    @Test
    public void testMoveTabGroupToOtherWindow_regularTabs_noEligibleWindow_createsNewWindow() {
        doTestMoveTabGroupToOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito= */ false, /* eligibleOtherWindowExists= */ false);
    }

    @Test
    public void testMoveTabGroupToOtherWindow_regularTabs_showsDialog() {
        doTestMoveTabGroupToOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito= */ false, /* eligibleOtherWindowExists= */ true);
    }

    @Test
    public void testMoveTabGroupToOtherWindow_incognitoTabs_noEligibleWindow_createsNewWindow() {
        doTestMoveTabGroupToOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito= */ true, /* eligibleOtherWindowExists= */ false);
    }

    @Test
    public void testMoveTabGroupToOtherWindow_incognitoTabs_showsDialog() {
        doTestMoveTabGroupToOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito= */ true, /* eligibleOtherWindowExists= */ true);
    }

    @Test
    public void testOpenUrlInOtherWindow_regularTab_showsDialog() {
        doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito= */ false,
                /* preferNew= */ false,
                /* atInstanceLimit= */ false,
                /* numOtherEligibleWindows= */ 2);
    }

    @Test
    public void testOpenUrlInOtherWindow_regularTab_opensOtherWindow() {
        doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito= */ false,
                /* preferNew= */ false,
                /* atInstanceLimit= */ false,
                /* numOtherEligibleWindows= */ 1);
    }

    @Test
    public void testOpenUrlInOtherWindow_regularTab_noEligibleWindow_createsNewWindow() {
        doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito= */ false,
                /* preferNew= */ false,
                /* atInstanceLimit= */ false,
                /* numOtherEligibleWindows= */ 0);
    }

    @Test
    public void testOpenUrlInOtherWindow_regularTab_preferNew_createsNewWindow() {
        doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito= */ false,
                /* preferNew= */ true,
                /* atInstanceLimit= */ false,
                /* numOtherEligibleWindows= */ 1);
    }

    @Test
    public void testOpenUrlInOtherWindow_regularTab_preferNew_showsMessageAtInstanceLimit() {
        doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito= */ false,
                /* preferNew= */ true,
                /* atInstanceLimit= */ true,
                /* numOtherEligibleWindows= */ 1);
    }

    @Test
    public void testOpenUrlInOtherWindow_incognitoTab_opensInOtherIncognitoWindow() {
        doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito= */ true,
                /* preferNew= */ false,
                /* atInstanceLimit= */ false,
                /* numOtherEligibleWindows= */ 1);
    }

    @Test
    public void testOpenUrlInOtherWindow_incognitoTab_noEligibleWindow_createsNewWindow() {
        doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito= */ true,
                /* preferNew= */ false,
                /* atInstanceLimit= */ false,
                /* numOtherEligibleWindows= */ 0);
    }

    @Test
    public void testOpenUrlInOtherWindow_incognitoTab_preferNew_createsNewWindow() {
        doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito= */ true,
                /* preferNew= */ true,
                /* atInstanceLimit= */ false,
                /* numOtherEligibleWindows= */ 1);
    }

    @Test
    public void testOpenUrlInOtherWindow_incognitoTab_preferNew_showsMessageAtInstanceLimit() {
        doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
                /* isIncognito= */ true,
                /* preferNew= */ true,
                /* atInstanceLimit= */ true,
                /* numOtherEligibleWindows= */ 1);
    }

    @Test
    public void
            testOpenUrlInOtherWindow_regularTab_sourceNotTabbedActivity_opensLastAccessedWindow() {
        // Setup.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        // Setup: Simulate mTabbedActivity1 to be the last accessed window.
        MultiWindowUtils.setLastAccessedWindowIdForTesting(SOURCE_WINDOW_ID);

        // Act.
        mMultiInstanceOrchestrator.openUrlInOtherWindow(
                mActivity,
                mUrlParams,
                PARENT_TAB_ID_1,
                /* preferNew= */ false,
                /* isIncognito= */ false);

        // Verify.
        var intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mTabbedActivity1).onNewIntent(intentCaptor.capture());
        assertEquals(
                "Uri data is incorrect.",
                mUrlParams.getUrl(),
                intentCaptor.getValue().getData().toString());
    }

    @Test
    public void testOpenUrlInOtherWindow_regularTab_destActivityDestroyed_createsNewTask() {
        // Setup.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        configureInstancesForOtherWindowTests(
                /* isIncognito= */ false,
                /* atInstanceLimit= */ false,
                /* numOtherEligibleWindows= */ 1);
        // Setup: Simulate the last accessed window to have a destroyed activity.
        MultiWindowUtils.setLastAccessedWindowIdForTesting(DEST_WINDOW_ID);
        MultiWindowUtils.setActivityByWindowIdForTesting(DEST_WINDOW_ID, /* activity= */ null);
        var appTask = mock(AppTask.class);
        AndroidTaskUtils.setAppTaskForTesting(appTask);

        // Act.
        mMultiInstanceOrchestrator.openUrlInOtherWindow(
                mTabbedActivity1,
                mUrlParams,
                PARENT_TAB_ID_1,
                /* preferNew= */ false,
                /* isIncognito= */ false);

        // Verify.
        verify(appTask).finishAndRemoveTask();
        var intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mTabbedActivity1).startActivity(intentCaptor.capture());
        verifyNewWindowIntentForUrlLaunch(intentCaptor.getValue(), /* isIncognitoWindow= */ false);
        assertEquals(
                "Window id intent extra is not set.",
                DEST_WINDOW_ID,
                intentCaptor
                        .getValue()
                        .getIntExtra(IntentHandler.EXTRA_WINDOW_ID, INVALID_WINDOW_ID));
    }

    @Test
    public void testOpenUrlInOtherWindow_preApi31() {
        // Setup.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);

        // Act.
        mMultiInstanceOrchestrator.openUrlInOtherWindow(
                mTabbedActivity1,
                mUrlParams,
                PARENT_TAB_ID_1,
                /* preferNew= */ false,
                /* isIncognito= */ false);

        // Verify.
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
    }

    @Test
    public void testOpenUrlInIncognitoWindow_withinInstanceLimit_opensInOtherIncognitoWindow() {
        doTestOpenUrlInIncognitoWindow(
                /* atInstanceLimit= */ false, /* otherIncognitoWindowExists= */ true);
    }

    @Test
    public void testOpenUrlInIncognitoWindow_atInstanceLimit_opensInOtherIncognitoWindow() {
        doTestOpenUrlInIncognitoWindow(
                /* atInstanceLimit= */ true, /* otherIncognitoWindowExists= */ true);
    }

    @Test
    public void testOpenUrlInIncognitoWindow_noOtherIncognitoWindow_createsNewWindow() {
        doTestOpenUrlInIncognitoWindow(
                /* atInstanceLimit= */ false, /* otherIncognitoWindowExists= */ false);
    }

    @Test
    public void testOpenUrlInIncognitoWindow_noOtherIncognitoWindow_showsMessageAtInstanceLimit() {
        doTestOpenUrlInIncognitoWindow(
                /* atInstanceLimit= */ true, /* otherIncognitoWindowExists= */ false);
    }

    @Test
    @Config(sdk = 30)
    public void testOnForegroundBrowserProcessInitialized_noCrash_noOp() {
        // List of reasons that should NOT trigger crash recovery.
        int[] nonCrashReasons = {
            ApplicationExitInfo.REASON_USER_REQUESTED,
            ApplicationExitInfo.REASON_EXIT_SELF,
            ApplicationExitInfo.REASON_USER_STOPPED,
            -1 // API failure
        };

        for (int reason : nonCrashReasons) {
            mMultiInstanceOrchestrator.onForegroundBrowserProcessInitialized(reason);
            assertFalse(
                    "Reason " + reason + " should not trigger crash recovery.",
                    ChromeMultiInstancePersistentStore.readIsCrashRecoveryPending());
        }

        verify(mTabbedCrashRecoveryDelegate, never()).initiateCrashRecovery(any(), any(), any());
    }

    @Test
    @Config(sdk = 30)
    public void testOnForegroundBrowserProcessInitialized_crash_immediateRecovery() {
        MultiWindowTestUtils.createInstance(SOURCE_WINDOW_ID, "www.example.com", 1, 1);
        ChromeMultiInstancePersistentStore.writeIsRecoverable(SOURCE_WINDOW_ID, true);

        int[] crashReasons = {
            ApplicationExitInfo.REASON_CRASH,
            ApplicationExitInfo.REASON_CRASH_NATIVE,
            ApplicationExitInfo.REASON_ANR
        };

        for (int reason : crashReasons) {
            // Register one activity.
            ((MultiInstanceOrchestratorImpl) mMultiInstanceOrchestrator)
                    .clearAssignmentsForTesting();
            mMultiInstanceOrchestrator.onInitialize(mTabbedActivity1, mMultiInstanceManager1);

            // Act: Initialize with a crash exit reason.
            mMultiInstanceOrchestrator.onForegroundBrowserProcessInitialized(reason);

            // Verify: Immediate crash recovery initiated.
            verify(mTabbedCrashRecoveryDelegate)
                    .initiateCrashRecovery(
                            eq(mModalDialogManagerSupplier), eq(mTabbedActivity1), any());
            assertFalse(ChromeMultiInstancePersistentStore.readIsCrashRecoveryPending());

            // Reset mock for the next iteration.
            Mockito.reset(mTabbedCrashRecoveryDelegate);
        }
    }

    @Test
    @Config(sdk = 30)
    public void testOnForegroundBrowserProcessInitialized_crash_deferredRecovery() {
        // Setup: One crashed window.
        MultiWindowTestUtils.createInstance(SOURCE_WINDOW_ID, "www.example.com", 1, 1);
        ChromeMultiInstancePersistentStore.writeIsRecoverable(SOURCE_WINDOW_ID, true);

        int[] crashReasons = {
            ApplicationExitInfo.REASON_CRASH,
            ApplicationExitInfo.REASON_CRASH_NATIVE,
            ApplicationExitInfo.REASON_ANR
        };

        for (int reason : crashReasons) {
            // Clear any registered activities.
            ((MultiInstanceOrchestratorImpl) mMultiInstanceOrchestrator)
                    .clearAssignmentsForTesting();

            // Act: Initialize with a crash exit reason.
            mMultiInstanceOrchestrator.onForegroundBrowserProcessInitialized(reason);

            // Verify: Deferred crash recovery.
            verify(mTabbedCrashRecoveryDelegate, never())
                    .initiateCrashRecovery(any(), any(), any());
            assertTrue(
                    "Reason " + reason + " should trigger deferred crash recovery.",
                    ChromeMultiInstancePersistentStore.readIsCrashRecoveryPending());
        }
    }

    @Test
    @Config(sdk = 30)
    public void testOnInitialize_pendingRecovery() {
        // Setup: One crashed window.
        MultiWindowTestUtils.createInstance(SOURCE_WINDOW_ID, "www.example.com", 1, 1);
        ChromeMultiInstancePersistentStore.writeIsRecoverable(SOURCE_WINDOW_ID, true);

        // Clear any registered activities and trigger deferred recovery.
        ((MultiInstanceOrchestratorImpl) mMultiInstanceOrchestrator).clearAssignmentsForTesting();
        mMultiInstanceOrchestrator.onForegroundBrowserProcessInitialized(
                ApplicationExitInfo.REASON_CRASH);
        assertTrue(ChromeMultiInstancePersistentStore.readIsCrashRecoveryPending());

        // Act: Initialize a new ChromeTabbedActivity.
        mMultiInstanceOrchestrator.onInitialize(mTabbedActivity1, mMultiInstanceManager1);

        // Verify: Crash recovery initiated during onInitialize.
        verify(mTabbedCrashRecoveryDelegate)
                .initiateCrashRecovery(
                        eq(mModalDialogManagerSupplier), eq(mTabbedActivity1), any());
        assertFalse(ChromeMultiInstancePersistentStore.readIsCrashRecoveryPending());
    }

    @Test
    @Config(sdk = 30)
    public void
            testOnForegroundBrowserProcessInitialized_crash_newActivityNoPreviousActiveTabbedActivity() {
        // Setup: No crashed tabbed windows exist initially on disk.
        MultiWindowTestUtils.resetInstanceInfo();

        // Clear any registered activities.
        ((MultiInstanceOrchestratorImpl) mMultiInstanceOrchestrator).clearAssignmentsForTesting();

        // Register the new activity. This triggers onInitialize(), which captures the empty list.
        mMultiInstanceOrchestrator.onInitialize(mTabbedActivity1, mMultiInstanceManager1);

        // The new activity now writes writeIsRecoverable(0, true) during its normal initialization.
        ChromeMultiInstancePersistentStore.writeIsRecoverable(SOURCE_WINDOW_ID, true);

        // Act: Foreground browser process is initialized with a crash exit reason.
        mMultiInstanceOrchestrator.onForegroundBrowserProcessInitialized(
                ApplicationExitInfo.REASON_CRASH);

        // Verify: No crash recovery is initiated because no ChromeTabbedActivity was active before
        // the crash.
        verify(mTabbedCrashRecoveryDelegate, never()).initiateCrashRecovery(any(), any(), any());
        assertFalse(ChromeMultiInstancePersistentStore.readIsCrashRecoveryPending());
    }

    private void doTestOpenUrlInOtherWindowWithIncognitoWindowingEnabled(
            boolean isIncognito,
            boolean preferNew,
            boolean atInstanceLimit,
            int numOtherEligibleWindows) {
        // Setup.
        boolean eligibleOtherWindowExists = numOtherEligibleWindows != 0;
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        configureInstancesForOtherWindowTests(
                isIncognito, atInstanceLimit, numOtherEligibleWindows);

        // Act.
        mMultiInstanceOrchestrator.openUrlInOtherWindow(
                mTabbedActivity1, mUrlParams, PARENT_TAB_ID_1, preferNew, isIncognito);

        // Verify.
        if (preferNew && atInstanceLimit) {
            verify(mMultiInstanceManager1).showInstanceCreationLimitMessage();
        } else if (preferNew || !eligibleOtherWindowExists) {
            var intentCaptor = ArgumentCaptor.forClass(Intent.class);
            verify(mTabbedActivity1).startActivity(intentCaptor.capture());
            verifyNewWindowIntentForUrlLaunch(intentCaptor.getValue(), isIncognito);
        } else if (isIncognito || numOtherEligibleWindows == 1) {
            var intentCaptor = ArgumentCaptor.forClass(Intent.class);
            verify(mTabbedActivity2).onNewIntent(intentCaptor.capture());
            assertEquals(
                    "Uri data is incorrect.",
                    mUrlParams.getUrl(),
                    intentCaptor.getValue().getData().toString());
        } else {
            ArgumentCaptor<Callback<InstanceInfo>> callbackCaptor = MockitoHelper.callbackCaptor();
            verify(mMultiInstanceManager1)
                    .showTargetSelectorDialog(
                            callbackCaptor.capture(),
                            eq(PersistedInstanceType.ACTIVE | PersistedInstanceType.REGULAR),
                            eq(R.string.contextmenu_open_in_other_window));
            callbackCaptor
                    .getValue()
                    .onResult(getTestInstanceInfo(DEST_WINDOW_ID, /* isIncognito= */ false));
            var intentCaptor = ArgumentCaptor.forClass(Intent.class);
            verify(mTabbedActivity2).onNewIntent(intentCaptor.capture());
            assertEquals(
                    "Uri data is incorrect.",
                    mUrlParams.getUrl(),
                    intentCaptor.getValue().getData().toString());
        }
    }

    private void doTestOpenUrlInIncognitoWindow(
            boolean atInstanceLimit, boolean otherIncognitoWindowExists) {
        // Setup.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);

        // Create mTabbedActivity1 as a regular window and mTabbedActivity2 based on
        // otherIncognitoWindowExists.
        createActiveInstances(
                /* count= */ 1, SupportedProfileType.REGULAR, /* startId= */ SOURCE_WINDOW_ID);
        var otherWindowType =
                otherIncognitoWindowExists
                        ? SupportedProfileType.OFF_THE_RECORD
                        : SupportedProfileType.REGULAR;
        createActiveInstances(/* count= */ 1, otherWindowType, /* startId= */ DEST_WINDOW_ID);

        if (atInstanceLimit) MultiWindowUtils.setMaxInstancesForTesting(2);

        // Act.
        mMultiInstanceOrchestrator.openUrlInOtherWindow(
                mTabbedActivity1,
                mUrlParams,
                PARENT_TAB_ID_1,
                /* preferNew= */ false,
                /* isIncognito= */ true);

        // Verify.
        if (!otherIncognitoWindowExists && atInstanceLimit) {
            verify(mMultiInstanceManager1).showInstanceCreationLimitMessage();
        } else if (!otherIncognitoWindowExists) {
            var intentCaptor = ArgumentCaptor.forClass(Intent.class);
            verify(mTabbedActivity1).startActivity(intentCaptor.capture());
            verifyNewWindowIntentForUrlLaunch(
                    intentCaptor.getValue(), /* isIncognitoWindow= */ true);
        } else {
            var intentCaptor = ArgumentCaptor.forClass(Intent.class);
            verify(mTabbedActivity2).onNewIntent(intentCaptor.capture());
            assertEquals(
                    "Uri data is incorrect.",
                    mUrlParams.getUrl(),
                    intentCaptor.getValue().getData().toString());
        }
    }

    private void doTestMoveTabsToOtherWindowWithIncognitoWindowingEnabled(
            boolean isIncognito, boolean eligibleOtherWindowExists) {
        // Setup.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        List<Tab> tabs = List.of(mTab1, mTab2);
        when(mTab1.isIncognitoBranded()).thenReturn(isIncognito);
        configureInstancesForOtherWindowTests(
                isIncognito, /* atInstanceLimit= */ false, eligibleOtherWindowExists ? 1 : 0);

        // Act.
        mMultiInstanceOrchestrator.moveTabsToOtherWindow(tabs, NewWindowAppSource.MENU);

        // Verify.
        if (!eligibleOtherWindowExists) {
            verify(mTabReparentingDelegate)
                    .reparentTabsToNewWindow(
                            mTabbedActivity1,
                            tabs,
                            INVALID_WINDOW_ID,
                            /* openAdjacently= */ true,
                            /* finalizeCallback= */ null,
                            NewWindowAppSource.MENU);
        } else {
            ArgumentCaptor<Callback<InstanceInfo>> callbackCaptor = MockitoHelper.callbackCaptor();
            verify(mMultiInstanceManager1)
                    .showTargetSelectorDialog(
                            callbackCaptor.capture(),
                            eq(
                                    PersistedInstanceType.ACTIVE
                                            | (isIncognito
                                                    ? PersistedInstanceType.OFF_THE_RECORD
                                                    : PersistedInstanceType.REGULAR)),
                            eq(R.string.menu_move_tab_to_other_window));
            callbackCaptor.getValue().onResult(getTestInstanceInfo(DEST_WINDOW_ID, isIncognito));
            verify(mTabReparentingDelegate)
                    .reparentTabsToExistingWindow(
                            eq(mTabbedActivity2), eq(tabs), eq(-1), eq(-1), eq(true));
            verify(mMultiInstanceManager1).closeChromeWindowIfEmpty(SOURCE_WINDOW_ID);
        }
    }

    private void doTestMoveTabGroupToOtherWindowWithIncognitoWindowingEnabled(
            boolean isIncognito, boolean eligibleOtherWindowExists) {
        // Setup.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        setupTabGroupMetadata(isIncognito);
        configureInstancesForOtherWindowTests(
                isIncognito, /* atInstanceLimit= */ false, eligibleOtherWindowExists ? 1 : 0);

        // Act.
        mMultiInstanceOrchestrator.moveTabGroupToOtherWindow(
                mTabGroupMetadata, NewWindowAppSource.MENU);

        // Verify.
        if (!eligibleOtherWindowExists) {
            verify(mTabReparentingDelegate)
                    .reparentTabGroupToNewWindow(
                            mTabGroupMetadata,
                            INVALID_WINDOW_ID,
                            /* openAdjacently= */ true,
                            NewWindowAppSource.MENU);
        } else {
            ArgumentCaptor<Callback<InstanceInfo>> callbackCaptor = MockitoHelper.callbackCaptor();
            verify(mMultiInstanceManager1)
                    .showTargetSelectorDialog(
                            callbackCaptor.capture(),
                            eq(
                                    PersistedInstanceType.ACTIVE
                                            | (isIncognito
                                                    ? PersistedInstanceType.OFF_THE_RECORD
                                                    : PersistedInstanceType.REGULAR)),
                            eq(R.string.menu_move_group_to_other_window));
            callbackCaptor.getValue().onResult(getTestInstanceInfo(DEST_WINDOW_ID, isIncognito));
            verify(mTabReparentingDelegate)
                    .reparentTabGroupToExistingWindow(
                            eq(mTabbedActivity2), eq(mTabGroupMetadata), eq(-1), eq(true));
            verify(mMultiInstanceManager1).closeChromeWindowIfEmpty(SOURCE_WINDOW_ID);
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
        when(tab.getContext()).thenReturn(activity);
        when(webContents.getTopLevelNativeWindow()).thenReturn(windowAndroid);
        when(windowAndroid.getActivity()).thenReturn(new WeakReference<>(activity));
    }

    private void configureInstancesForOtherWindowTests(
            boolean isIncognito, boolean atInstanceLimit, int numOtherEligibleWindows) {
        when(mTabbedActivity1.isIncognitoWindow()).thenReturn(isIncognito);

        int numInstances = numOtherEligibleWindows + 1;
        if (atInstanceLimit) {
            MultiWindowUtils.setMaxInstancesForTesting(numInstances);
        } else if (numOtherEligibleWindows == 0) {
            numInstances = 1;
            // Make mTabbedActivity2 an ineligible profile type window.
            createActiveInstances(
                    /* count= */ 1,
                    isIncognito
                            ? SupportedProfileType.REGULAR
                            : SupportedProfileType.OFF_THE_RECORD,
                    /* startId= */ DEST_WINDOW_ID);
            when(mTabbedActivity2.isIncognitoWindow()).thenReturn(!isIncognito);
        }

        // This will overwrite instance state for mTabbedActivity1 and mTabbedActivity2.
        createActiveInstances(
                /* count= */ numInstances,
                isIncognito ? SupportedProfileType.OFF_THE_RECORD : SupportedProfileType.REGULAR,
                /* startId= */ SOURCE_WINDOW_ID);
    }

    private InstanceInfo getTestInstanceInfo(int windowId, boolean isIncognito) {
        return new InstanceInfo(
                windowId,
                /* taskId= */ windowId,
                InstanceInfo.Type.CURRENT,
                /* url= */ "www.example.com",
                /* title= */ "Example",
                /* customTitle= */ null,
                isIncognito ? 0 : 2,
                isIncognito ? 2 : 0,
                isIncognito,
                /* lastAccessedTime= */ 0,
                /* closureTime= */ 0);
    }

    private void setupTabGroupMetadata(boolean isIncognito) {
        when(mTab1.isIncognitoBranded()).thenReturn(isIncognito);
        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab1.getTabGroupId()).thenReturn(Token.createRandom());
        when(mTab2.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        mTabGroupMetadata =
                TabGroupMetadataExtractor.extractTabGroupMetadata(
                        mTabModel,
                        List.of(mTab1, mTab2),
                        SOURCE_WINDOW_ID,
                        PARENT_TAB_ID_1,
                        /* isGroupShared= */ false);
    }

    private void verifyNewWindowIntentForUrlLaunch(Intent intent, boolean isIncognitoWindow) {
        assertEquals(
                "New window source extra is incorrect.",
                NewWindowAppSource.URL_LAUNCH,
                intent.getIntExtra(
                        IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, NewWindowAppSource.UNKNOWN));
        assertNotNull("Uri data is missing.", intent.getData());
        assertEquals("Uri data is incorrect.", mUrlParams.getUrl(), intent.getData().toString());
        assertEquals(
                "Target window profile type is incorrect.",
                isIncognitoWindow,
                intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_WINDOW, false));
        assertTrue(
                "FLAG_ACTIVITY_NEW_TASK is not set.",
                (intent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0);
        assertTrue(
                "FLAG_ACTIVITY_MULTIPLE_TASK is not set.",
                (intent.getFlags() & Intent.FLAG_ACTIVITY_MULTIPLE_TASK) != 0);
    }
}
