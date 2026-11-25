// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator.LeaseReason;
import org.chromium.chrome.browser.app.tabmodel.TabbedModeTabModelOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Robolectric unit tests for {@link QuickDeleteDelegateImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class QuickDeleteDelegateImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfileMock;
    @Mock private Profile mIncognitoProfileMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private UserPrefs.Natives mUserPrefsNativesMock;
    @Mock private QuickDeleteBridge.Natives mQuickDeleteBridgeNativesMock;
    @Mock private TabGroupSyncService mTabGroupSyncServiceMock;
    @Mock private SnackbarManager mSnackbarManagerMock;
    @Mock private ArchivedTabModelOrchestrator mOrchestratorMock;
    @Mock private TabbedModeTabModelOrchestrator mActivityOrchestratorMock;
    @Mock private Destroyable mLeaseMock;

    private Activity mActivity;
    private FakeModalDialogManager mModalDialogManager;
    private MockTabModelSelector mTabModelSelector;
    private MockTabModelSelector mArchivedTabModelSelector;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mModalDialogManager = new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);

        when(mProfileMock.getOriginalProfile()).thenReturn(mProfileMock);
        when(mProfileMock.isNativeInitialized()).thenReturn(true);
        when(mProfileMock.isOffTheRecord()).thenReturn(false);
        when(mProfileMock.shutdownStarted()).thenReturn(false);
        when(mIncognitoProfileMock.isOffTheRecord()).thenReturn(true);

        UserPrefsJni.setInstanceForTesting(mUserPrefsNativesMock);
        when(mUserPrefsNativesMock.get(mProfileMock)).thenReturn(mPrefServiceMock);
        when(mPrefServiceMock.getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)).thenReturn(true);
        QuickDeleteBridgeJni.setInstanceForTesting(mQuickDeleteBridgeNativesMock);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncServiceMock);

        mTabModelSelector =
                new MockTabModelSelector(mProfileMock, mIncognitoProfileMock, 0, 0, null);
        mArchivedTabModelSelector =
                new MockTabModelSelector(mProfileMock, mIncognitoProfileMock, 0, 0, null);
        MultiWindowUtils.setInstanceCountForTesting(1);
    }

    @After
    public void tearDown() {
        ArchivedTabModelOrchestrator.unregisterTabModelOrchestrator(mActivityOrchestratorMock);
    }

    @Test
    public void testCreateController_Gating() {
        ArchivedTabModelOrchestrator.setInstanceForTesting(mOrchestratorMock);
        MultiWindowUtils.setInstanceCountForTesting(2);
        createController().destroy();
        verify(mOrchestratorMock, never()).acquireLease(anyInt());

        // When already instantiated, zero archivedTabCount must still acquire a lease.
        MultiWindowUtils.setInstanceCountForTesting(1);
        TabArchiveSettings.getInstance().setArchivedTabCount(0);
        when(mOrchestratorMock.acquireLease(LeaseReason.QUICK_DELETE)).thenReturn(mLeaseMock);
        when(mOrchestratorMock.areTabModelsInitialized()).thenReturn(true);
        when(mOrchestratorMock.getTabModelSelector()).thenReturn(mArchivedTabModelSelector);
        QuickDeleteController controller = createController();
        verify(mOrchestratorMock).acquireLease(LeaseReason.QUICK_DELETE);
        verify(mLeaseMock, never()).destroy();
        controller.destroy();
        verify(mLeaseMock).destroy();

        // mActivityOrchestratorMock.getTabContentManager() returns null so acquireLease() enters
        // the resurrection path without triggering native TabModelJniBridge initialization.
        ArchivedTabModelOrchestrator.setInstanceForTesting(null);
        TabArchiveSettings.getInstance().resetSettingsForTesting();
        assertFalse(TabArchiveSettings.getInstance().hasArchivedTabCountBeenSet());
        ArchivedTabModelOrchestrator.registerActivityOrchestrator(mActivityOrchestratorMock);

        createController().destroy();
        verify(mActivityOrchestratorMock).getTabContentManager();

        clearInvocations(mActivityOrchestratorMock);
        TabArchiveSettings.getInstance().setArchivedTabCount(0);
        createController().destroy();
        verify(mActivityOrchestratorMock, never()).getTabContentManager();

        TabArchiveSettings.getInstance().setArchivedTabCount(1);
        createController().destroy();
        verify(mActivityOrchestratorMock).getTabContentManager();
    }

    @Test
    public void testCreateController_LeaseLifecycle() {
        ArchivedTabModelOrchestrator.setInstanceForTesting(mOrchestratorMock);
        when(mOrchestratorMock.acquireLease(LeaseReason.QUICK_DELETE)).thenReturn(mLeaseMock);

        when(mOrchestratorMock.areTabModelsInitialized()).thenReturn(false);
        QuickDeleteController uninitializedController = createController();
        verify(mLeaseMock).destroy();
        uninitializedController.destroy();

        clearInvocations(mLeaseMock);
        when(mOrchestratorMock.areTabModelsInitialized()).thenReturn(true);
        when(mOrchestratorMock.getTabModelSelector()).thenReturn(null);
        QuickDeleteController nullSelectorController = createController();
        verify(mLeaseMock).destroy();
        nullSelectorController.destroy();

        clearInvocations(mLeaseMock);
        when(mOrchestratorMock.getTabModelSelector()).thenReturn(mArchivedTabModelSelector);
        QuickDeleteController controller = createController();
        verify(mLeaseMock, never()).destroy();
        controller.destroy();
        verify(mLeaseMock).destroy();
    }

    private QuickDeleteController createController() {
        return QuickDeleteDelegateImpl.createController(
                mActivity,
                mProfileMock,
                /* tabSwitcherSupplier= */ () -> null,
                mModalDialogManager,
                mSnackbarManagerMock,
                /* layoutManager= */ null,
                mTabModelSelector);
    }
}
