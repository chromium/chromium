// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.view.KeyEvent;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.TestBottomSheetContent;
import org.chromium.ui.base.DeviceFormFactor;

/** This class tests the functionality of the {@link AnchoredDialogCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@Batch(Batch.PER_CLASS)
public class AnchoredDialogTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public FreshCtaTransitTestRule mTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private TestBottomSheetContent mContent;
    private AnchoredDialogCoordinator mCoordinator;
    @Mock private BottomSheetObserver mObserver;

    @Before
    public void setUp() {
        mTestRule.startOnBlankPage();
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new AnchoredDialogCoordinator(
                                    mTestRule.getActivity(),
                                    mTestRule.getActivity().getContentView(),
                                    () ->
                                            mTestRule
                                                    .getActivity()
                                                    .getBrowserControlsManager()
                                                    .getContentOffset());
                    mContent =
                            new TestBottomSheetContent(
                                    mTestRule.getActivity(),
                                    BottomSheetContent.ContentPriority.HIGH,
                                    false);
                    mCoordinator.addObserver(mObserver);
                });
    }

    @After
    public void tearDown() {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.removeObserver(mObserver);
                });
    }

    @Test
    @MediumTest
    public void testShowAndHide() {
        runOnUiThreadBlocking(() -> mCoordinator.requestShowContent(mContent));
        verify(mObserver).onSheetContentChanged(mContent);
        verify(mObserver).onSheetOpened(StateChangeReason.NONE);
        verify(mObserver).onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);

        runOnUiThreadBlocking(
                () -> mCoordinator.hideContent(mContent, StateChangeReason.INTERACTION_COMPLETE));
        verify(mObserver).onSheetClosed(StateChangeReason.INTERACTION_COMPLETE);
        verify(mObserver)
                .onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.INTERACTION_COMPLETE);
        verify(mObserver).onSheetContentChanged(null);
    }

    @Test
    @MediumTest
    public void testReplaceContents() {
        InOrder inOrder = inOrder(mObserver);

        runOnUiThreadBlocking(() -> mCoordinator.requestShowContent(mContent));
        inOrder.verify(mObserver).onSheetContentChanged(mContent);
        inOrder.verify(mObserver).onSheetOpened(StateChangeReason.NONE);
        inOrder.verify(mObserver).onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        clearInvocations(mObserver);

        final TestBottomSheetContent newContent =
                runOnUiThreadBlocking(
                        () ->
                                new TestBottomSheetContent(
                                        mTestRule.getActivity(),
                                        BottomSheetContent.ContentPriority.HIGH,
                                        false));

        runOnUiThreadBlocking(() -> mCoordinator.requestShowContent(newContent));

        inOrder.verify(mObserver).onSheetClosed(StateChangeReason.NONE);
        inOrder.verify(mObserver).onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.NONE);
        inOrder.verify(mObserver).onSheetContentChanged(null);

        inOrder.verify(mObserver).onSheetContentChanged(newContent);
        inOrder.verify(mObserver).onSheetOpened(StateChangeReason.NONE);
        inOrder.verify(mObserver).onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);
        clearInvocations(mObserver);

        runOnUiThreadBlocking(
                () -> mCoordinator.hideContent(newContent, StateChangeReason.INTERACTION_COMPLETE));
        inOrder.verify(mObserver).onSheetClosed(StateChangeReason.INTERACTION_COMPLETE);
        inOrder.verify(mObserver)
                .onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.INTERACTION_COMPLETE);
        inOrder.verify(mObserver).onSheetContentChanged(null);
    }

    @Test
    @MediumTest
    public void testDismissDialogOnBack() {
        runOnUiThreadBlocking(() -> mCoordinator.requestShowContent(mContent));
        verify(mObserver).onSheetOpened(StateChangeReason.NONE);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mContent.getContentView().hasWindowFocus();
                },
                "Dialog couldn't get the focus");
        InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mTestRule.getActivity().hasWindowFocus();
                },
                "Activity couldn't regain the focus");
        verify(mObserver).onSheetClosed(StateChangeReason.NONE);
    }
}
