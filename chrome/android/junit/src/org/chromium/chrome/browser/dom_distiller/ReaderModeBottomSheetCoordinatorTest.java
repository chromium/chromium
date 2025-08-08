// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.components.dom_distiller.core.DomDistillerService;

/** Unit tests for {@link ReaderModeBottomSheetCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ReaderModeBottomSheetCoordinatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private DomDistillerServiceFactoryJni mDomDistillerServiceFactoryJni;
    @Mock private DomDistillerService mDomDistillerService;
    @Mock private DistilledPagePrefs mDistilledPagePrefs;
    @Mock private Profile mProfile;

    private ReaderModeBottomSheetCoordinator mCoordinator;
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mDomDistillerService.getDistilledPagePrefs()).thenReturn(mDistilledPagePrefs);
        when(mDomDistillerServiceFactoryJni.getForProfile(any())).thenReturn(mDomDistillerService);
        DomDistillerServiceFactoryJni.setInstanceForTesting(mDomDistillerServiceFactoryJni);
        mCoordinator =
                new ReaderModeBottomSheetCoordinator(mActivity, mProfile, mBottomSheetController);
    }

    @Test
    public void testShow() {
        mCoordinator.show(/* showFullSheet= */ false);

        verify(mBottomSheetController).requestShowContent(any(), eq(true));
        verify(mBottomSheetController, times(0)).expandSheet();
    }

    @Test
    public void testShow_fullSheet() {
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mCoordinator.show(/* showFullSheet= */ true);

        verify(mBottomSheetController).requestShowContent(any(), eq(true));
        verify(mBottomSheetController).expandSheet();
    }

    @Test
    public void testShow_alreadyShowing_expandsSheet() {
        // Mock that the bottom sheet is already showing
        when(mBottomSheetController.getCurrentSheetContent())
                .thenReturn(mCoordinator.getBottomSheetContentForTesting());

        // Show the bottom sheet again, but this time, ask it to be expanded.
        mCoordinator.show(/* showFullSheet= */ true);

        // Verify that the sheet was expanded
        verify(mBottomSheetController).expandSheet();
    }
}
