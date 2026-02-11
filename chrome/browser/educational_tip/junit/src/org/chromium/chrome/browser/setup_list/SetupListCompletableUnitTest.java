// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.setup_list;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

import androidx.annotation.DrawableRes;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;

/** Test relating to {@link SetupListCompletable} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SetupListCompletableUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private interface MockCompletableProvider
            extends EducationalTipCardProvider, SetupListCompletable {}

    @Mock private MockCompletableProvider mMockCompletableProvider;
    @Mock private SetupListManager mSetupListManager;

    private static final @DrawableRes int DEFAULT_ICON = 1;
    private static final @DrawableRes int COMPLETED_ICON = 2;
    private static final @ModuleType int TEST_MODULE_TYPE = ModuleType.ENHANCED_SAFE_BROWSING_PROMO;

    @Before
    public void setUp() {
        SetupListManager.setInstanceForTesting(mSetupListManager);
    }

    @Test
    @SmallTest
    public void testGetCompletionState_NotSetupListModule() {
        when(mSetupListManager.isSetupListModule(anyInt())).thenReturn(false);

        SetupListCompletable.CompletionState state =
                SetupListCompletable.getCompletionState(mMockCompletableProvider, TEST_MODULE_TYPE);

        assertNull(state);
    }

    @Test
    @SmallTest
    public void testGetCompletionState_SetupListModule_Completable_NotComplete() {
        when(mSetupListManager.isSetupListModule(TEST_MODULE_TYPE)).thenReturn(true);
        when(mMockCompletableProvider.isComplete()).thenReturn(false);
        when(mMockCompletableProvider.getCardImage()).thenReturn(DEFAULT_ICON);

        SetupListCompletable.CompletionState state =
                SetupListCompletable.getCompletionState(mMockCompletableProvider, TEST_MODULE_TYPE);

        assertFalse(state.isCompleted);
        assertEquals(DEFAULT_ICON, state.iconRes);
    }

    @Test
    @SmallTest
    public void testGetCompletionState_SetupListModule_Completable_IsComplete() {
        when(mSetupListManager.isSetupListModule(TEST_MODULE_TYPE)).thenReturn(true);
        when(mMockCompletableProvider.isComplete()).thenReturn(true);
        when(mMockCompletableProvider.getCardImageCompletedResId()).thenReturn(COMPLETED_ICON);

        SetupListCompletable.CompletionState state =
                SetupListCompletable.getCompletionState(mMockCompletableProvider, TEST_MODULE_TYPE);

        assertTrue(state.isCompleted);
        assertEquals(COMPLETED_ICON, state.iconRes);
    }

    @Test
    @SmallTest
    public void testGetCompletionState_AwaitingAnimation() {
        when(mSetupListManager.isSetupListModule(TEST_MODULE_TYPE)).thenReturn(true);
        when(mSetupListManager.isModuleAwaitingCompletionAnimation(TEST_MODULE_TYPE))
                .thenReturn(true);
        when(mMockCompletableProvider.isComplete()).thenReturn(true);
        when(mMockCompletableProvider.getCardImage()).thenReturn(DEFAULT_ICON);

        SetupListCompletable.CompletionState state =
                SetupListCompletable.getCompletionState(mMockCompletableProvider, TEST_MODULE_TYPE);

        // Should return the DEFAULT icon and isCompleted=false initially,
        // even if the task is technically complete, to allow for the animation.
        assertFalse(state.isCompleted);
        assertEquals(DEFAULT_ICON, state.iconRes);
    }
}
