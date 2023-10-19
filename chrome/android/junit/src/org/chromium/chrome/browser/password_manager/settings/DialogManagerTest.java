// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.annotation.IntDef;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Tests for the {@link DialogManager} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class DialogManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    /**
     * Fake of {@link DialogFragment} allowing to detect calls to {@link DialogFragment#show} and
     * {@link DialogFragment#dismiss}.
     */
    private static class FakeDialogFragment extends DialogFragment {
        @Retention(RetentionPolicy.SOURCE)
        @IntDef({NEW, SHOWING, DISMISSED})
        public @interface DialogState {}

        /** The dialog has not been shown yet. */
        public static final int NEW = 0;

        /** The dialog is showing. */
        public static final int SHOWING = 1;

        /** The dialog has been dismissed. */
        public static final int DISMISSED = 2;

        private @DialogState int mState = NEW;

        public @DialogState int getState() {
            return mState;
        }

        @Override
        public void show(FragmentManager manager, String tag) {
            assert mState == NEW;
            mState = SHOWING;
        }

        @Override
        public void dismiss() {
            assert mState == SHOWING;
            mState = DISMISSED;
        }
    }

    /** Used to detect showing and hiding without actually triggering any UI. */
    private final FakeDialogFragment mDialogFragment = new FakeDialogFragment();

    /** Used to test reporting from DialogManager. */
    private final DialogManager.ActionsConsumer mMockActionsConsumer =
            mock(DialogManager.ActionsConsumer.class);

    /** The object under test. */
    private final DialogManager mDialogManager = new DialogManager(mMockActionsConsumer);

    /**
     * Delayer to replace the timed one in the tested class. This gives exact control over hiding
     * delays.
     */
    private final ManualCallbackDelayer mManualDelayer = new ManualCallbackDelayer();

    @Before
    public void setUp() {
        mDialogManager.replaceCallbackDelayerForTesting(mManualDelayer);
    }

    /** Check that a dialog is hidden eventually but not before a prescribed delay. */
    @Test
    public void testHiddenEventually() {
        mDialogManager.show(mDialogFragment, null);

        Runnable callback = mock(Runnable.class);
        mDialogManager.hide(callback);
        Robolectric.getForegroundThreadScheduler().advanceToLastPostedRunnable();
        verify(callback, never()).run();

        mManualDelayer.runCallbacksSynchronously();
        verify(callback, times(1)).run();
        assertEquals(FakeDialogFragment.DISMISSED, mDialogFragment.getState());
        verify(mMockActionsConsumer, times(1)).consume(DialogManager.HideActions.HIDING_DELAYED);
    }

    /** Check that immediate hiding is notified properly. */
    @Test
    public void testNotification() {
        mDialogManager.show(mDialogFragment, null);

        Runnable callback = mock(Runnable.class);
        mManualDelayer.runCallbacksSynchronously();
        mDialogManager.hide(callback);
        Robolectric.getForegroundThreadScheduler().advanceToLastPostedRunnable();
        verify(callback, times(1)).run();
        assertEquals(FakeDialogFragment.DISMISSED, mDialogFragment.getState());
        verify(mMockActionsConsumer, times(1))
                .consume(DialogManager.HideActions.HIDDEN_IMMEDIATELY);
    }

    /** Check that the callback is called even if the dialog has not been shown at all. */
    @Test
    public void testCallbackCalled() {
        Runnable callback = mock(Runnable.class);
        mDialogManager.hide(callback);
        Robolectric.getForegroundThreadScheduler().advanceToLastPostedRunnable();
        verify(callback, times(1)).run();
        assertEquals(FakeDialogFragment.NEW, mDialogFragment.getState());
        verify(mMockActionsConsumer, times(1)).consume(DialogManager.HideActions.NO_OP);
    }
}
