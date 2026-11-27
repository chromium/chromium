// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.jni_zero.JniUniquePtr;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.AccountPreviewDataService;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link RecentTabsManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
@DisableFeatures(SigninFeatures.ENABLE_ACCOUNT_PREVIEW_PREFERRED_ACCOUNT)
public class RecentTabsManagerUnitTest {
    private static final long NATIVE_FAVICON_HELPER = 1L;
    private static final long NATIVE_FOREIGN_SESSION_HELPER = 1L;
    private static final ForeignSession SESSION_1 =
            new ForeignSession("tag1", "Phone", 1000L, new ArrayList<>(), FormFactor.PHONE);
    private static final ForeignSession SESSION_2 =
            new ForeignSession("tag2", "Desktop", 2000L, new ArrayList<>(), FormFactor.DESKTOP);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Tab mTab;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Profile mProfile;
    @Mock private ActivityResultTracker mActivityResultTracker;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Runnable mShowHistoryManager;
    @Mock private RecentlyClosedEntriesManager mRecentlyClosedEntriesManager;
    @Mock private SigninManager mSigninManager;
    @Mock private AccountPreviewDataService mAccountPreviewDataService;
    @Mock private SyncService mSyncService;
    @Mock private SigninAndHistorySyncActivityLauncher mSigninLauncher;
    @Mock private BottomSheetSigninAndHistorySyncCoordinator mSigninCoordinator;
    @Mock private ForeignSessionHelper.Natives mForeignSessionHelperJniMock;
    @Mock private FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock private RecentTabsManager.UpdatedCallback mUpdatedCallback;

    /** Foreign sessions that the native side currently reports. */
    private final List<ForeignSession> mNativeForeignSessions = new ArrayList<>();

    private JniUniquePtr<ForeignSessionHelper.NativeForeignSessionHelper>
            mNativeForeignSessionHelper;
    private Activity mActivity;
    private RecentTabsManager mManager;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        ForeignSessionHelperJni.setInstanceForTesting(mForeignSessionHelperJniMock);
        mNativeForeignSessionHelper = JniUniquePtr.createForTesting(NATIVE_FOREIGN_SESSION_HELPER);
        when(mForeignSessionHelperJniMock.init(mProfile)).thenReturn(mNativeForeignSessionHelper);
        doAnswer(
                        invocation -> {
                            List<ForeignSession> resultSessions = invocation.getArgument(1);
                            resultSessions.addAll(mNativeForeignSessions);
                            return true;
                        })
                .when(mForeignSessionHelperJniMock)
                .getForeignSessions(eq(mNativeForeignSessionHelper), any());

        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJniMock);
        when(mFaviconHelperJniMock.init()).thenReturn(NATIVE_FAVICON_HELPER);

        IdentityServicesProvider.setSigninManagerForTesting(mSigninManager);
        IdentityServicesProvider.setAccountPreviewDataServiceForTesting(mAccountPreviewDataService);
        when(mSigninManager.getIdentityManager())
                .thenReturn(mAccountManagerTestRule.getIdentityManager());
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        SigninAndHistorySyncActivityLauncherImpl.setLauncherForTest(mSigninLauncher);
        when(mSigninLauncher.createBottomSheetSigninCoordinatorAndObserveAddAccountResult(
                        any(), any(), any(), any(), any(), any(), any(), any(), any(), anyInt()))
                .thenReturn(mSigninCoordinator);
    }

    @After
    public void tearDown() {
        destroyManager();
    }

    private RecentTabsManager createManager() {
        return new RecentTabsManager(
                mTab,
                mWindowAndroid,
                mActivity,
                mProfile,
                mActivityResultTracker,
                mBottomSheetController,
                () -> mModalDialogManager,
                mSnackbarManager,
                mShowHistoryManager,
                mRecentlyClosedEntriesManager);
    }

    private void destroyManager() {
        if (mManager != null) {
            mManager.destroy();
            mManager = null;
        }
    }

    @Test
    public void testConstruction() {
        mNativeForeignSessions.add(SESSION_1);

        mManager = createManager();

        assertEquals(mProfile, mManager.getProfile());
        assertEquals(mTab, mManager.activeTab());
        assertEquals(List.of(SESSION_1), mManager.getForeignSessions());
        verify(mRecentlyClosedEntriesManager).updateRecentlyClosedEntries();
        verify(mForeignSessionHelperJniMock).triggerSessionSync(mNativeForeignSessionHelper);
    }

    @Test
    public void testAccountAddedReloadsForeignSessions() {
        mManager = createManager();
        mManager.setUpdatedCallback(mUpdatedCallback);
        assertEquals(List.of(), mManager.getForeignSessions());
        mNativeForeignSessions.add(SESSION_1);

        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        assertEquals(List.of(SESSION_1), mManager.getForeignSessions());
        verify(mUpdatedCallback, atLeastOnce()).onUpdated();
    }

    @Test
    public void testAccountRemovedReloadsForeignSessions() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mNativeForeignSessions.add(SESSION_1);
        mManager = createManager();
        mManager.setUpdatedCallback(mUpdatedCallback);
        assertEquals(List.of(SESSION_1), mManager.getForeignSessions());
        mNativeForeignSessions.clear();

        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        assertEquals(List.of(), mManager.getForeignSessions());
        verify(mUpdatedCallback, atLeastOnce()).onUpdated();
    }

    @Test
    public void testAccountUpdatedReloadsForeignSessions() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mNativeForeignSessions.add(SESSION_1);
        mManager = createManager();
        mManager.setUpdatedCallback(mUpdatedCallback);
        assertEquals(List.of(SESSION_1), mManager.getForeignSessions());
        mNativeForeignSessions.add(SESSION_2);

        AccountInfo updatedAccount =
                new AccountInfo.Builder(TestAccounts.ACCOUNT1).fullName("Updated Name").build();
        mAccountManagerTestRule.updateAccount(updatedAccount);

        assertEquals(List.of(SESSION_1, SESSION_2), mManager.getForeignSessions());
        verify(mUpdatedCallback, atLeastOnce()).onUpdated();
    }

    @Test
    public void testDestroyStopsReloadingForeignSessions() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mNativeForeignSessions.add(SESSION_1);
        mManager = createManager();
        mManager.setUpdatedCallback(mUpdatedCallback);
        assertEquals(List.of(SESSION_1), mManager.getForeignSessions());

        RecentTabsManager manager = mManager;
        destroyManager();

        assertEquals(0, mAccountManagerTestRule.getIdentityManager().getObserverCount());
        verify(mSyncService).removeSyncStateChangedListener(manager);
        verify(mSigninManager).removeSignInStateObserver(manager);
        verify(mSigninCoordinator).destroy();
        verify(mFaviconHelperJniMock).destroy(NATIVE_FAVICON_HELPER);

        mNativeForeignSessions.add(SESSION_2);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        assertEquals(List.of(SESSION_1), manager.getForeignSessions());
        verify(mUpdatedCallback, never()).onUpdated();
    }
}
