// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.os.Looper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.components.search_engines.SearchEnginesFeatures;

@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({
    ChromeFeatureList.SEARCH_ENGINE_CHOICE,
    SearchEnginesFeatures.CLAY_BLOCKING
})
public class ChoiceDialogCoordinatorUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private @Mock ChoiceDialogCoordinator mDialogCoordinator;
    private @Mock SearchEngineChoiceService mSearchEngineChoiceService;

    @Before
    public void setUp() {
        SearchEngineChoiceService.setInstanceForTests(mSearchEngineChoiceService);
    }

    @Test
    public void testShouldShowDeviceChoiceDialog() {
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();
        doReturn(Promise.fulfilled(true))
                .when(mSearchEngineChoiceService)
                .shouldShowDeviceChoiceDialog();

        assertNotNull(ChoiceDialogCoordinator.maybeShowInternal(() -> mDialogCoordinator));
        shadowOf(Looper.getMainLooper()).idle();
        verify(mDialogCoordinator).show();
    }

    @Test
    @Features.EnableFeatures({SearchEnginesFeatures.CLAY_BLOCKING})
    public void testShouldShowDeviceChoiceDialog_doesNotShowWhenNotEligible() {
        doReturn(false).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();

        assertNull(ChoiceDialogCoordinator.maybeShowInternal(() -> mDialogCoordinator));
        shadowOf(Looper.getMainLooper()).idle();
        verify(mDialogCoordinator, never()).show();
    }

    @Test
    public void testShouldShowDeviceChoiceDialog_doesNotShowWhenShouldNot() {
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();
        doReturn(Promise.fulfilled(false))
                .when(mSearchEngineChoiceService)
                .shouldShowDeviceChoiceDialog();

        assertNotNull(ChoiceDialogCoordinator.maybeShowInternal(() -> mDialogCoordinator));
        shadowOf(Looper.getMainLooper()).idle();
        verify(mDialogCoordinator, never()).show();
    }

    @Test
    public void testShouldShowDeviceChoiceDialog_showsAfterDelayedApproval() {
        var pendingPromise = new Promise<Boolean>();
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();
        doReturn(pendingPromise).when(mSearchEngineChoiceService).shouldShowDeviceChoiceDialog();

        assertNotNull(ChoiceDialogCoordinator.maybeShowInternal(() -> mDialogCoordinator));
        shadowOf(Looper.getMainLooper()).idle();
        verify(mDialogCoordinator, never()).show();

        pendingPromise.fulfill(true);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mDialogCoordinator).show();
    }

    @Test
    public void testShouldShowDeviceChoiceDialog_doesNotShowAfterDelayedDisapproval() {
        var pendingPromise = new Promise<Boolean>();
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();
        doReturn(pendingPromise).when(mSearchEngineChoiceService).shouldShowDeviceChoiceDialog();

        assertNotNull(ChoiceDialogCoordinator.maybeShowInternal(() -> mDialogCoordinator));
        shadowOf(Looper.getMainLooper()).idle();
        verify(mDialogCoordinator, never()).show();

        pendingPromise.fulfill(false);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mDialogCoordinator, never()).show();
    }

    @Test
    public void testShouldShowDeviceChoiceDialog_doesNotShowAfterTimeout() {
        var pendingPromise = new Promise<Boolean>();
        doReturn(true).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();
        doReturn(pendingPromise).when(mSearchEngineChoiceService).shouldShowDeviceChoiceDialog();

        assertNotNull(ChoiceDialogCoordinator.maybeShowInternal(() -> mDialogCoordinator));
        shadowOf(Looper.getMainLooper()).idle();
        verify(mDialogCoordinator, never()).show();

        shadowOf(Looper.getMainLooper()).runToEndOfTasks(); // Advance past the timeout delay.
        verify(mDialogCoordinator, never()).show();
    }
}
