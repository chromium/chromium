// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchInternalStateController.InternalState;

/** Tests for the {@link ContextualSearchInternalStateController} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class ContextualSearchInternalStateTest {
    private ContextualSearchInternalStateController mInternalStateController;

    private class ContextualSearchInternalStateHandlerStub
            implements ContextualSearchInternalStateHandler {
        @Override
        public void hideContextualSearchUi(@StateChangeReason int reason) {
            mDidHide = true;
        }

        @Override
        public void showContextualSearchResolvingUi() {
            mDidShow = true;
            stubForWorkOnState(InternalState.SHOW_RESOLVING_UI);
        }

        @Override
        public void showContextualSearchLiteralSearchUi() {
            mDidShow = true;
        }

        @Override
        public void tapGestureCommit() {
            stubForWorkOnState(InternalState.TAP_GESTURE_COMMIT);
        }

        @Override
        public void gatherSurroundingText() {
            stubForWorkOnState(InternalState.GATHERING_SURROUNDINGS);
        }

        @Override
        public void decideSuppression() {
            stubForWorkOnState(InternalState.DECIDING_SUPPRESSION);
        }

        @Override
        public void startShowingTapUi() {
            stubForWorkOnState(InternalState.START_SHOWING_TAP_UI);
        }

        @Override
        public void resolveSearchTerm() {
            mInternalStateController.notifyStartingWorkOn(InternalState.RESOLVING);
            if (mInternalStateController.isStillWorkingOn(InternalState.RESOLVING)) {
                mDidResolve = true;
                mInternalStateController.notifyFinishedWorkOn(InternalState.RESOLVING);
            }
        }

        @Override
        public void showingTapSearch() {
            stubForWorkOnState(InternalState.SHOWING_TAP_SEARCH);
        }

        @Override
        public void showingIntelligentLongpress() {
            stubForWorkOnState(InternalState.SHOWING_RESOLVED_LONG_PRESS_SEARCH);
        }

        @Override
        public void completeSearch() {
            stubForWorkOnState(InternalState.SEARCH_COMPLETED);
        }

        @Override
        public void waitForPossibleTapNearPrevious() {
            stubForWorkOnState(InternalState.WAITING_FOR_POSSIBLE_TAP_NEAR_PREVIOUS);
        }

        @Override
        public void waitForPossibleTapOnTapSelection() {
            stubForWorkOnState(InternalState.WAITING_FOR_POSSIBLE_TAP_ON_TAP_SELECTION);
        }

        boolean didResolve() {
            return mDidResolve;
        }

        // Stub for doing work on some state.
        private void stubForWorkOnState(@InternalState int state) {
            mInternalStateController.notifyStartingWorkOn(state);
            // Work completes (possibly async)
            mInternalStateController.notifyFinishedWorkOn(state);
        }
    }

    private ContextualSearchInternalStateHandlerStub mHandlerStub;
    private ContextualSearchPolicy mMockedPolicy;

    private boolean mDidHide;
    private boolean mDidShow;
    private boolean mDidResolve;

    boolean didHide() {
        return mDidHide;
    }

    boolean didShow() {
        return mDidShow;
    }

    boolean didResolve() {
        return mDidResolve;
    }

    private void reset() {
        mDidHide = false;
        mDidShow = false;
        mDidResolve = false;
    }

    @Before
    public void setup() {
        reset();
        mMockedPolicy = mock(ContextualSearchPolicy.class);
        mHandlerStub = new ContextualSearchInternalStateHandlerStub();
        mInternalStateController =
                new ContextualSearchInternalStateController(mMockedPolicy, mHandlerStub);
    }

    private void mocksForTap() {
        when(mMockedPolicy.shouldPreviousGestureResolve()).thenReturn(true);
    }

    private void mocksForNonResolvingTap() {
        when(mMockedPolicy.shouldPreviousGestureResolve()).thenReturn(false);
    }

    private void mocksForLongpress() {
        // None needed.
    }

    @Test
    @Feature({"ContextualSearch"})
    public void testInternalStateNormalTapSequence() {
        mocksForTap();
        mInternalStateController.enter(InternalState.TAP_RECOGNIZED);
        assertTrue("Did not Resolve!", mHandlerStub.didResolve());
    }

    @Test
    @Feature({"ContextualSearch"})
    public void testInternalStateNormalLongpressSequence() {
        mocksForLongpress();
        mInternalStateController.enter(InternalState.LONG_PRESS_RECOGNIZED);
        assertFalse("A Resolve should not be done on Long-press!", mHandlerStub.didResolve());
        assertThat(mInternalStateController.getState(), is(InternalState.SHOWING_LITERAL_SEARCH));
    }

    @Test
    @Feature({"ContextualSearch"})
    public void testInternalStateNonResolvingTapSequence() {
        mocksForNonResolvingTap();
        mInternalStateController.enter(InternalState.TAP_RECOGNIZED);
        assertFalse("Unexpected Resolve!", mHandlerStub.didResolve());
    }

    // Tests for assertions.

    @Test(expected = AssertionError.class)
    @Feature({"ContextualSearch"})
    public void testResetWithNullReasonFails() {
        mInternalStateController.reset(null);
    }

    @Test(expected = AssertionError.class)
    @Feature({"ContextualSearch"})
    public void testEnterTransitionalStateFails() {
        mInternalStateController.enter(InternalState.RESOLVING);
    }

    @Test(expected = AssertionError.class)
    @Feature({"ContextualSearch"})
    public void testFinishedWithoutStartingFails() {
        mHandlerStub =
                new ContextualSearchInternalStateHandlerStub() {
                    @Override
                    public void startShowingTapUi() {
                        // Finish without starting on this arbitrary transitional step.
                        mInternalStateController.notifyFinishedWorkOn(InternalState.RESOLVING);
                    }
                };
        mInternalStateController =
                new ContextualSearchInternalStateController(mMockedPolicy, mHandlerStub);
        mocksForTap();
        mInternalStateController.enter(InternalState.TAP_RECOGNIZED);
        assertTrue("Did not Resolve!", mHandlerStub.didResolve());
    }

    @Test
    @Feature({"ContextualSearch"})
    public void testResetDoesntRetryCurrentState() {
        mInternalStateController.enter(InternalState.IDLE);
        when(mMockedPolicy.shouldRetryCurrentState(InternalState.IDLE)).thenReturn(false);
        mInternalStateController.reset(StateChangeReason.BACK_PRESS);
        verify(mMockedPolicy, times(1)).shouldRetryCurrentState(InternalState.IDLE);
        assertFalse(didHide());
    }

    @Test
    @Feature({"ContextualSearch"})
    public void testResetDoesRetryCurrentStateWhenNeeded() {
        mInternalStateController.enter(InternalState.IDLE);
        when(mMockedPolicy.shouldRetryCurrentState(InternalState.IDLE)).thenReturn(true);
        mInternalStateController.reset(StateChangeReason.BACK_PRESS);
        verify(mMockedPolicy, times(1)).shouldRetryCurrentState(InternalState.IDLE);
        assertTrue(didHide());
    }
}
