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
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feature_engagement.Tracker;

/** Unit test for {@link MismatchNotificationChecker} */
@RunWith(BaseRobolectricTestRunner.class)
public class MismatchNotificationCheckerUnitTest {
    @Rule public MockitoRule mTestRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock MismatchNotificationChecker.Delegate mDelegate;

    @Test
    public void otherPromptsUisSuppressedWhileShowing() {
        new MismatchNotificationCheckerTester()
                .newChecker()
                .assertOtherPromptsSuppressed(false)
                // Verify the delegate method is invoked and the IPH lock is also activated.
                .callMaybeShowUi(/* shown= */ true)
                .assertIphLocked(true)
                .assertOtherPromptsSuppressed(true)
                .closeUi(/* closeType= */ 0) // value doesn't matter
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

        private Callback<Integer> mCloseCallback;

        public MismatchNotificationCheckerTester newChecker() {
            mDelegate = mock(MismatchNotificationChecker.Delegate.class);

            mTracker = mock(Tracker.class);
            TrackerFactory.setTrackerForTests(mTracker);

            mIphDisplayLock = mock(Tracker.DisplayLockHandle.class);
            when(mTracker.acquireDisplayLock()).thenReturn(mIphDisplayLock);

            mChecker = new MismatchNotificationChecker(mProfileMock, mDelegate);
            return this;
        }

        public MismatchNotificationCheckerTester callMaybeShowUi(boolean shown) {
            when(mDelegate.maybeShow(anyLong(), any())).thenReturn(shown);

            ArgumentCaptor<Callback> captor = ArgumentCaptor.forClass(Callback.class);
            mChecker.maybeShow(/* lastShowTime= */ 12345); // any value will do
            verify(mDelegate).maybeShow(anyLong(), captor.capture());
            mCloseCallback = captor.getValue();
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

        public MismatchNotificationCheckerTester closeUi(int closeType) {
            mCloseCallback.onResult(closeType);
            return this;
        }
    }
}
