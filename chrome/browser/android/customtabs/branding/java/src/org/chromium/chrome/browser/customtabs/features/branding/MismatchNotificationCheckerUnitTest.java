// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.customtabs.features.branding.proto.AccountMismatchData.CloseType;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;

/** Unit test for {@link MismatchNotificationChecker} */
@RunWith(BaseRobolectricTestRunner.class)
public class MismatchNotificationCheckerUnitTest {
    private static final int INIT_SHOW_COUNT = 2;
    private static final int INIT_USER_ACT_COUNT = 1;

    @Rule public MockitoRule mTestRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

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

    private static class MismatchNotificationCheckerTester {
        // Mocks
        private MismatchNotificationChecker.Delegate mDelegate;
        private MismatchNotificationChecker mChecker;
        private Tracker mTracker;
        private Tracker.DisplayLockHandle mIphDisplayLock;
        private Profile mProfileMock;
        private Callback<Integer> mCallback;
        private IdentityManager mIdentityManager;
        private CoreAccountInfo mCoreAccountInfo;
        private Callback<MismatchNotificationData> mOnClose;

        private MismatchNotificationData.AppUiData mAppData =
                new MismatchNotificationData.AppUiData();

        public MismatchNotificationCheckerTester newChecker() {
            mDelegate = mock(MismatchNotificationChecker.Delegate.class);

            mTracker = mock(Tracker.class);
            TrackerFactory.setTrackerForTests(mTracker);

            mIphDisplayLock = mock(Tracker.DisplayLockHandle.class);
            when(mTracker.acquireDisplayLock()).thenReturn(mIphDisplayLock);

            mCoreAccountInfo = mock(CoreAccountInfo.class);
            when(mCoreAccountInfo.getGaiaId()).thenReturn("nice-gaia-id");
            mIdentityManager = mock(IdentityManager.class);
            when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(mCoreAccountInfo);

            mAppData.showCount = INIT_SHOW_COUNT;
            mAppData.closeType = CloseType.UNKNOWN.getNumber();
            mAppData.userActCount = INIT_USER_ACT_COUNT;

            mOnClose = mock(Callback.class);

            mChecker = new MismatchNotificationChecker(mProfileMock, mIdentityManager, mDelegate);
            return this;
        }

        public MismatchNotificationCheckerTester callMaybeShowUi(
                boolean shown, MismatchNotificationData mimData) {
            when(mDelegate.maybeShow(any(), anyLong(), any(), any())).thenReturn(shown);

            ArgumentCaptor<Callback> captor = ArgumentCaptor.forClass(Callback.class);
            if (mimData != null) mimData.setAppData(mChecker.getAccountId(), "app-id", mAppData);
            mChecker.maybeShow("app-id", /* lastShowTime= */ 12345, mimData, mOnClose);
            verify(mDelegate).maybeShow(any(), anyLong(), any(), captor.capture());
            mCallback = captor.getValue();
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
