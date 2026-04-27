// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.customtabs.features.branding.proto.AccountMismatchData.CloseType;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.MockitoHelper;

/** Unit test for {@link MismatchNotificationChecker} */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    SigninFeatures.ENABLE_SEAMLESS_SIGNIN,
    SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT
})
public class MismatchNotificationCheckerUnitTest {
    private static final int INIT_SHOW_COUNT = 2;
    private static final int INIT_USER_ACT_COUNT = 1;

    @Rule public MockitoRule mTestRule = MockitoJUnit.rule();

    @Mock MismatchNotificationChecker.Delegate mDelegate;

    @Test
    public void mimAccountUpdated_fromEmptyData() {
        new MismatchNotificationCheckerTester()
                .newChecker()
                .callMaybeShowUi(/* shown= */ true, null)
                .callCloseUi(CloseType.TIMED_OUT)
                .assertCloseCallbackInvoked(
                        /* showCount= */ 1, /* dismissCount= */ 0, CloseType.TIMED_OUT);
    }

    @Test
    public void mimAccountUpdated_fromExistingData_updateShowCount() {
        new MismatchNotificationCheckerTester()
                .newChecker()
                .callMaybeShowUi(/* shown= */ true, new MismatchNotificationData())
                .callCloseUi(CloseType.TIMED_OUT)
                .assertCloseCallbackInvoked(
                        INIT_SHOW_COUNT + 1, INIT_USER_ACT_COUNT, CloseType.TIMED_OUT);
    }

    @Test
    public void mimAccountUpdated_fromExistingData_updateUserActCount_dismissed() {
        new MismatchNotificationCheckerTester()
                .newChecker()
                .callMaybeShowUi(/* shown= */ true, new MismatchNotificationData())
                .callCloseUi(CloseType.DISMISSED)
                .assertCloseCallbackInvoked(
                        INIT_SHOW_COUNT + 1, INIT_USER_ACT_COUNT + 1, CloseType.DISMISSED);
    }

    @Test
    public void mimAccountUpdated_fromExistingData_updateUserActCount_accepted() {
        new MismatchNotificationCheckerTester()
                .newChecker()
                .callMaybeShowUi(/* shown= */ true, new MismatchNotificationData())
                .callCloseUi(CloseType.ACCEPTED)
                .assertCloseCallbackInvoked(
                        INIT_SHOW_COUNT + 1, INIT_USER_ACT_COUNT + 1, CloseType.ACCEPTED);
    }

    @Test
    public void otherPromptsUisSuppressedWhileShowing() {
        new MismatchNotificationCheckerTester()
                .newChecker()
                .assertOtherPromptsSuppressed(false)
                // Verify the delegate method is invoked and the IPH lock is also activated.
                .callMaybeShowUi(/* shown= */ true, new MismatchNotificationData())
                .assertIphLocked(true)
                .assertOtherPromptsSuppressed(true)
                .callCloseUi(CloseType.DISMISSED)
                // Closing the notification should restore everything.
                .assertIphLocked(false)
                .assertOtherPromptsSuppressed(false);
    }

    @Test
    public void maybeShowWhileSignedOut() {
        new MismatchNotificationCheckerTester()
                .newChecker()
                .signOut()
                .callMaybeShowUi(/* shown= */ true, new MismatchNotificationData())
                .assertIphLocked(true);
    }

    private static class MismatchNotificationCheckerTester {
        // Mocks
        private MismatchNotificationChecker.Delegate mDelegate;
        private MismatchNotificationChecker mChecker;
        private Tracker mTracker;
        private Tracker.DisplayLockHandle mIphDisplayLock;
        private Callback<Integer> mCallback;
        private IdentityManager mIdentityManager;
        private Callback<MismatchNotificationData> mOnClose;
        private SigninAndHistorySyncActivityLauncher mSigninLauncher;

        private final MismatchNotificationData.AppUiData mAppData =
                new MismatchNotificationData.AppUiData();

        public MismatchNotificationCheckerTester newChecker() {
            mDelegate = mock(MismatchNotificationChecker.Delegate.class);

            mTracker = mock(Tracker.class);
            TrackerFactory.setTrackerForTests(mTracker);

            mIphDisplayLock = mock(Tracker.DisplayLockHandle.class);
            when(mTracker.acquireDisplayLock()).thenReturn(mIphDisplayLock);

            mIdentityManager = mock(IdentityManager.class);
            when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(TestAccounts.ACCOUNT1);

            mAppData.showCount = INIT_SHOW_COUNT;
            mAppData.userActCount = INIT_USER_ACT_COUNT;
            mAppData.closeType = CloseType.UNKNOWN.getNumber();

            mOnClose = MockitoHelper.mockCallback();
            mSigninLauncher = mock(SigninAndHistorySyncActivityLauncher.class);

            mChecker =
                    new MismatchNotificationChecker(
                            mock(Activity.class),
                            mock(WindowAndroid.class),
                            mock(ActivityResultTracker.class),
                            mock(DeviceLockActivityLauncher.class),
                            mock(Profile.class),
                            mIdentityManager,
                            mSigninLauncher,
                            () -> mock(BottomSheetController.class),
                            mock(ModalDialogManager.class),
                            mock(SnackbarManager.class),
                            mDelegate);
            return this;
        }

        public MismatchNotificationCheckerTester callMaybeShowUi(
                boolean shown, MismatchNotificationData mimData) {
            when(mDelegate.maybeShow(any(), any(), anyLong(), any(), any())).thenReturn(shown);

            ArgumentCaptor<Callback<Integer>> captor = MockitoHelper.callbackCaptor();
            if (mimData != null) mimData.setAppData(mChecker.getAccountId(), "app-id", mAppData);
            mChecker.maybeShow("app-id", /* lastShowTime= */ 12345, mimData, mOnClose);
            verify(mDelegate).maybeShow(any(), any(), anyLong(), any(), captor.capture());
            mCallback = captor.getValue();
            return this;
        }

        public MismatchNotificationCheckerTester signOut() {
            when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(null);
            return this;
        }

        public MismatchNotificationCheckerTester assertOtherPromptsSuppressed(boolean suppressed) {
            assertEquals(mChecker.shouldSuppressPromptUis(), suppressed);
            return this;
        }

        public MismatchNotificationCheckerTester assertIphLocked(boolean locked) {
            if (locked) {
                verify(mTracker).acquireDisplayLock();
                clearInvocations(mTracker);
            } else {
                verify(mIphDisplayLock).release();
                clearInvocations(mIphDisplayLock);
            }
            return this;
        }

        public MismatchNotificationCheckerTester assertCloseCallbackInvoked(
                int showCount, int dismissCount, CloseType closeType) {
            var captor = ArgumentCaptor.forClass(MismatchNotificationData.class);
            verify(mOnClose).onResult(captor.capture());
            MismatchNotificationData data = captor.getValue();
            var appData = data.getAppData(mChecker.getAccountId(), "app-id");
            assertEquals("ShowCount was not updated", showCount, appData.showCount);
            assertEquals("CloseType was not updated", closeType.getNumber(), appData.closeType);
            assertEquals("UserActCount was not updated", dismissCount, appData.userActCount);
            return this;
        }

        public MismatchNotificationCheckerTester callCloseUi(CloseType closeType) {
            mCallback.onResult(closeType.getNumber());
            return this;
        }
    }
}
