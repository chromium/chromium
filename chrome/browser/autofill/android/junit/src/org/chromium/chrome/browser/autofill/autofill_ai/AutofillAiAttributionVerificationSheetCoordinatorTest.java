// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiAttributionVerificationSheetCoordinator.Delegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link AutofillAiAttributionVerificationSheetCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillAiAttributionVerificationSheetCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Delegate mDelegate;

    private TestActivity mActivity;
    private AutofillAiAttributionVerificationSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mCoordinator =
                new AutofillAiAttributionVerificationSheetCoordinator(
                        mActivity, mBottomSheetController, mDelegate);
    }

    @Test
    public void testInitialSetup() {
        assertNotNull(mCoordinator.getPropertyModelForTesting());
        assertNotNull(mCoordinator.getViewForTesting());
    }

    @Test
    public void testRequestShowContent_showsAndExpandsSheet() {
        AutofillAiAttributionVerificationSheetView view = mCoordinator.getViewForTesting();
        when(mBottomSheetController.requestShowContent(eq(view), eq(true))).thenReturn(true);

        mCoordinator.requestShowContent();

        verify(mBottomSheetController).requestShowContent(eq(view), /* animate= */ eq(true));
        verify(mBottomSheetController).expandSheet();
    }

    @Test
    public void testRequestShowContent_rejectedNotifiesDelegate() {
        AutofillAiAttributionVerificationSheetView view = mCoordinator.getViewForTesting();
        when(mBottomSheetController.requestShowContent(eq(view), eq(true))).thenReturn(false);

        mCoordinator.requestShowContent();

        verify(mDelegate, times(1)).onDismissed();
    }

    @Test
    public void testViewDestroyed_notifiesDelegateOnce() {
        AutofillAiAttributionVerificationSheetView view = mCoordinator.getViewForTesting();
        when(mBottomSheetController.requestShowContent(eq(view), eq(true))).thenReturn(true);
        mCoordinator.requestShowContent();

        view.destroy();
        view.destroy();

        verify(mDelegate, times(1)).onDismissed();
    }
}
