// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Unit tests for {@link TabBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabBottomSheetMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private TabBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        mMediator = new TabBottomSheetMediator();
    }

    @Test
    @SmallTest
    public void testOnSheetStateChanged_expanded() {
        mMediator.onSheetStateChanged(BottomSheetController.SheetState.FULL);
        assertEquals(BottomSheetController.SheetState.FULL, mMediator.getSheetStateForTesting());
    }
}
