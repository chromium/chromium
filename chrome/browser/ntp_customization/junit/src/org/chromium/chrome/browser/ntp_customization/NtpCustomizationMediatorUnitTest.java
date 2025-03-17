// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LAYOUT_TO_DISPLAY;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Map;

/** Unit tests for {@link NtpCustomizationMediator} */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpCustomizationMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private NtpCustomizationBottomSheetContent mBottomSheetContent;
    @Mock private PropertyModel mPropertyModel;

    private NtpCustomizationMediator mMediator;
    private Map<Integer, Integer> mViewFlipperMap;

    @Before
    public void setUp() {
        mMediator =
                new NtpCustomizationMediator(
                        mBottomSheetController, mBottomSheetContent, mPropertyModel);
        mViewFlipperMap = mMediator.getViewFlipperMapForTesting();
    }

    @Test
    @SmallTest
    public void testMaybeAddOneBottomSheetLayout() {
        // Verifies that the value and the key added to mViewFlipperMap are correct when adding
        // the key to mViewFlipperMap at the first time.
        mMediator.registerBottomSheetLayout(BottomSheetType.NTP_CARDS);

        assertEquals(1, mViewFlipperMap.size());
        assertEquals(0, (int) mViewFlipperMap.get(BottomSheetType.NTP_CARDS));

        // Verifies that adding the same key again won't change mViewFlipperMap and
        // mViewFlipperView.
        mMediator.registerBottomSheetLayout(BottomSheetType.NTP_CARDS);

        assertEquals(1, mViewFlipperMap.size());
        assertEquals(0, (int) mViewFlipperMap.get(BottomSheetType.NTP_CARDS));
    }

    @Test
    @SmallTest
    public void testMaybeAddTwoBottomSheetLayout() {
        // Verifies that the value and the key added to mViewFlipperMap are correct when first
        // adding the key.
        mMediator.registerBottomSheetLayout(BottomSheetType.NTP_CARDS);
        mMediator.registerBottomSheetLayout(BottomSheetType.MAIN);

        assertEquals(2, mViewFlipperMap.size());
        assertEquals(0, (int) mViewFlipperMap.get(BottomSheetType.NTP_CARDS));
        assertEquals(1, (int) mViewFlipperMap.get(BottomSheetType.MAIN));

        // Verifies that calling maybeAddBottomSheetLayout() with same type multiple times won't
        // change mViewFlipperMap and mViewFlipperView.
        mMediator.registerBottomSheetLayout(BottomSheetType.NTP_CARDS);
        mMediator.registerBottomSheetLayout(BottomSheetType.NTP_CARDS);
        mMediator.registerBottomSheetLayout(BottomSheetType.MAIN);
        mMediator.registerBottomSheetLayout(BottomSheetType.NTP_CARDS);
        mMediator.registerBottomSheetLayout(BottomSheetType.MAIN);

        assertEquals(2, mViewFlipperMap.size());
        assertEquals(0, (int) mViewFlipperMap.get(BottomSheetType.NTP_CARDS));
        assertEquals(1, (int) mViewFlipperMap.get(BottomSheetType.MAIN));
    }

    @Test
    @SmallTest
    public void testRequestShowContentCalledOnlyOnce() {
        // Verifies that requestShowContent() is called only when showBottomSheet() is called at the
        // first time.
        mViewFlipperMap.put(BottomSheetType.NTP_CARDS, 5);

        mMediator.showBottomSheet(BottomSheetType.NTP_CARDS);

        verify(mBottomSheetController).requestShowContent(eq(mBottomSheetContent), eq(true));
        clearInvocations(mBottomSheetController);

        mViewFlipperMap.put(11, 101);
        mMediator.showBottomSheet(11);
        verify(mBottomSheetController, never())
                .requestShowContent(eq(mBottomSheetContent), eq(true));
    }

    @Test
    @SmallTest
    public void testShowBottomSheet() {
        // Verifies that setDisplayChild() is called and mCurrentBottomSheet is set correctly.
        @ModuleDelegate.ModuleType int bottomSheetType = BottomSheetType.NTP_CARDS;
        int viewFlipperIndex = 2;
        mViewFlipperMap.put(bottomSheetType, viewFlipperIndex);

        mMediator.showBottomSheet(bottomSheetType);

        verify(mPropertyModel).set(eq(LAYOUT_TO_DISPLAY), eq(viewFlipperIndex));
        assertEquals(bottomSheetType, (int) mMediator.getCurrentBottomSheetForTesting());
    }

    @Test
    @SmallTest
    public void testShowBottomSheetAssertionError() {
        // Verifies that AssertionError will be raised if maybeAddBottomSheetLayout() is not
        // called before calling showBottomSheet()
        assertThrows(AssertionError.class, () -> mMediator.showBottomSheet(BottomSheetType.MAIN));
        assertThrows(
                AssertionError.class, () -> mMediator.showBottomSheet(BottomSheetType.NTP_CARDS));
    }

    @Test
    @SmallTest
    public void testBackPressNotInitialized() {
        // Verifies that backPressOnCurrentBottomSheet() will do nothing if mCurrentBottomSheet is
        // not initialized.
        mMediator.backPressOnCurrentBottomSheet();

        verify(mBottomSheetController, never())
                .hideContent(any(BottomSheetContent.class), anyBoolean());
        verify(mPropertyModel, never()).set(eq(LAYOUT_TO_DISPLAY), anyInt());
    }

    @Test
    @SmallTest
    public void testBackPressOnMainBottomSheet() {
        mMediator.setCurrentBottomSheetForTesting(BottomSheetType.MAIN);

        mMediator.backPressOnCurrentBottomSheet();

        // Verifies that hideContent() is called and mCurrentBottomSheet is set to null.
        verify(mBottomSheetController).hideContent(eq(mBottomSheetContent), eq(true));
        assertNull(mMediator.getCurrentBottomSheetForTesting());

        // Verifies that showBottomSheet() is not called.
        verify(mPropertyModel, never()).set(eq(LAYOUT_TO_DISPLAY), anyInt());
    }

    @Test
    @SmallTest
    public void testBackPressOnNtpCardsBottomSheet() {
        mViewFlipperMap.put(BottomSheetType.MAIN, 10);
        mMediator.setCurrentBottomSheetForTesting(BottomSheetType.NTP_CARDS);

        mMediator.backPressOnCurrentBottomSheet();

        // Verifies that hideContent() is not called and showBottomSheet() is called to change the
        // value of mCurrentBottomSheet and to set the value of mPropertyModel.
        verify(mBottomSheetController, never())
                .hideContent(any(BottomSheetContent.class), anyBoolean());
        assertEquals(BottomSheetType.MAIN, (int) mMediator.getCurrentBottomSheetForTesting());
        verify(mPropertyModel).set(eq(LAYOUT_TO_DISPLAY), eq(10));
    }

    @Test
    @SmallTest
    public void testDestroy() {
        mViewFlipperMap.put(BottomSheetType.NTP_CARDS, 9);
        mViewFlipperMap.put(BottomSheetType.MAIN, 10);

        assertEquals(2, mViewFlipperMap.size());
        mMediator.destroy();
        assertEquals(0, mViewFlipperMap.size());
    }

    @Test
    @SmallTest
    public void testBottomSheetObserver() {
        // Verifies the supplier is set to true when the sheet opens.
        BottomSheetObserver observer = mMediator.getBottomSheetObserver();
        observer.onSheetOpened(0);
        verify(mBottomSheetContent).onSheetOpened();

        // Verifies the supplier is set to false when the sheet closes and the observer is removed.
        observer.onSheetClosed(3); // Closes the sheet by clicking the trim.
        verify(mBottomSheetContent).onSheetClosed();
        verify(mBottomSheetController).removeObserver(eq(observer));
        clearInvocations(mBottomSheetContent);
        clearInvocations(mBottomSheetController);

        observer.onSheetClosed(0); // Closes the sheet by clicking the system back button.
        verify(mBottomSheetContent).onSheetClosed();
        verify(mBottomSheetController).removeObserver(eq(observer));
    }
}
