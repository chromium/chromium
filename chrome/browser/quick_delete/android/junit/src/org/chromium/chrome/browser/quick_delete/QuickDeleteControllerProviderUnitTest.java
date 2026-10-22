// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

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
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.TestActivity;

import java.util.function.Supplier;

/** Robolectric unit tests for {@link QuickDeleteControllerProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class QuickDeleteControllerProviderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private QuickDeleteController mController;

    private final Supplier<QuickDeleteController> mFactory = () -> mController;
    private ActivityController<TestActivity> mActivityController;
    private Activity mActivity;
    private ActivityWindowAndroid mWindowAndroid;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        mActivity = mActivityController.get();
        mWindowAndroid =
                new ActivityWindowAndroid(
                        mActivity,
                        /* listenToActivityState= */ false,
                        IntentRequestTracker.createFromActivity(mActivity),
                        /* insetObserver= */ null,
                        /* occlusionTrackingAllowed= */ false);
    }

    @After
    public void tearDown() {
        QuickDeleteControllerProvider.detach(mFactory);
        mWindowAndroid.destroy();
    }

    @Test
    public void testAttachAndDetach() {
        assertNull(QuickDeleteControllerProvider.from(/* window= */ null));
        assertNull(QuickDeleteControllerProvider.from(mWindowAndroid));

        QuickDeleteControllerProvider.attach(mWindowAndroid, mFactory);
        assertEquals(mFactory, QuickDeleteControllerProvider.from(mWindowAndroid));

        QuickDeleteControllerProvider.detach(mFactory);
        assertNull(QuickDeleteControllerProvider.from(mWindowAndroid));
    }

    @Test
    public void testShowDialog_NullOrUnattachedWindow_NoOp() {
        QuickDeleteControllerProvider.showDialog(/* window= */ null);
        QuickDeleteControllerProvider.showDialog(mWindowAndroid);

        verifyNoInteractions(mController);
    }

    @Test
    public void testShowDialog_FinishingActivity_NoOp() {
        QuickDeleteControllerProvider.attach(mWindowAndroid, mFactory);
        mActivity.finish();

        QuickDeleteControllerProvider.showDialog(mWindowAndroid);

        verifyNoInteractions(mController);
    }

    @Test
    public void testShowDialog_DestroyedActivity_NoOp() {
        QuickDeleteControllerProvider.attach(mWindowAndroid, mFactory);
        mActivityController.destroy();

        QuickDeleteControllerProvider.showDialog(mWindowAndroid);

        verifyNoInteractions(mController);
    }

    @Test
    public void testShowDialog_AttachedFactory_ShowsDialog() {
        QuickDeleteControllerProvider.attach(mWindowAndroid, mFactory);

        QuickDeleteControllerProvider.showDialog(mWindowAndroid);

        verify(mController).showDialog();
    }
}
