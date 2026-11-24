// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Robolectric unit tests for {@link QuickDeleteController}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class QuickDeleteControllerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfileMock;
    @Mock private Profile mIncognitoProfileMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private UserPrefs.Natives mUserPrefsNativesMock;
    @Mock private QuickDeleteBridge.Natives mQuickDeleteBridgeNativesMock;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private Tracker mTrackerMock;
    @Mock private Tracker.DisplayLockHandle mTrackerLockMock;
    @Mock private QuickDeleteDelegate mDelegateMock;
    @Mock private SnackbarManager mSnackbarManagerMock;
    @Mock private LayoutManager mLayoutManagerMock;
    @Mock private TabRemover mTabRemoverMock;
    @Mock private TabGroupSyncService mTabGroupSyncServiceMock;

    @Captor private ArgumentCaptor<Runnable> mOnDeleteFinishedCaptor;
    @Captor private ArgumentCaptor<Runnable> mOnAnimationEndCaptor;

    private Activity mActivity;
    private FakeModalDialogManager mModalDialogManager;
    private MockTabModelSelector mTabModelSelector;
    private MockTabModel mTabModel;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mModalDialogManager = new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);

        when(mProfileMock.isOffTheRecord()).thenReturn(false);
        when(mIncognitoProfileMock.isOffTheRecord()).thenReturn(true);

        UserPrefsJni.setInstanceForTesting(mUserPrefsNativesMock);
        when(mUserPrefsNativesMock.get(mProfileMock)).thenReturn(mPrefServiceMock);
        when(mPrefServiceMock.getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)).thenReturn(true);

        QuickDeleteBridgeJni.setInstanceForTesting(mQuickDeleteBridgeNativesMock);
        IdentityServicesProvider.setIdentityManagerForTesting(mIdentityManagerMock);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncServiceMock);

        TrackerFactory.setTrackerForTests(mTrackerMock);
        when(mTrackerMock.acquireDisplayLock()).thenReturn(mTrackerLockMock);

        mTabModelSelector =
                new MockTabModelSelector(mProfileMock, mIncognitoProfileMock, 0, 0, null);
        mTabModel = (MockTabModel) mTabModelSelector.getModel(/* incognito= */ false);
        mTabModel.setTabRemoverForTesting(mTabRemoverMock);
    }

    @Test
    public void testRegularTabModelSelectorDestroyed_DismissesDialogAndCancelsCallback() {
        QuickDeleteController controller = createController(/* layoutManager= */ null);
        controller.showDialog();
        mModalDialogManager.clickPositiveButton();

        verify(mDelegateMock).performQuickDelete(mOnDeleteFinishedCaptor.capture(), anyInt());

        mTabModelSelector.destroy();
        mOnDeleteFinishedCaptor.getValue().run();

        verify(mPrefServiceMock, never())
                .setBoolean(eq(QuickDeleteController.QUICK_DELETE_EVER_USED_PREF), anyBoolean());
        verify(mTrackerMock, never()).acquireDisplayLock();
        verifyNoInteractions(mSnackbarManagerMock);

        // Verify destroy() is idempotent.
        controller.destroy();
    }

    @Test
    public void testRegularTabModelSelectorDestroyedWhileDialogShown_DismissesDialog() {
        QuickDeleteController controller = createController(/* layoutManager= */ null);
        controller.showDialog();
        assertNotNull(mModalDialogManager.getShownDialogModel());

        mTabModelSelector.destroy();
        assertNull(mModalDialogManager.getShownDialogModel());

        controller.showDialog();
        assertNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    public void
            testRegularTabModelSelectorDestroyedDuringAnimation_CancelsCallbackAndReleasesLock() {
        MockTab tab = mTabModel.addTab(1);
        tab.setLastNavigationCommittedTimestampMillis(System.currentTimeMillis());
        when(mLayoutManagerMock.isLayoutVisible(LayoutType.HUB)).thenReturn(true);
        doAnswer(
                        invocation -> {
                            ((Runnable) invocation.getArgument(0)).run();
                            return null;
                        })
                .when(mDelegateMock)
                .performQuickDelete(any(), anyInt());

        QuickDeleteController controller = createController(mLayoutManagerMock);
        controller.showDialog();
        mModalDialogManager.clickPositiveButton();

        verify(mTrackerMock).acquireDisplayLock();
        verify(mTrackerLockMock, never()).release();
        verify(mDelegateMock).showQuickDeleteAnimation(mOnAnimationEndCaptor.capture(), any());

        mTabModelSelector.destroy();
        verify(mTrackerLockMock).release();

        mOnAnimationEndCaptor.getValue().run();
        verifyNoInteractions(mTabRemoverMock);
        verifyNoInteractions(mSnackbarManagerMock);
    }

    private QuickDeleteController createController(@Nullable LayoutManager layoutManager) {
        return new QuickDeleteController(
                mActivity,
                mDelegateMock,
                mModalDialogManager,
                mSnackbarManagerMock,
                layoutManager,
                mTabModelSelector,
                /* archivedTabModelSelector= */ null);
    }
}
