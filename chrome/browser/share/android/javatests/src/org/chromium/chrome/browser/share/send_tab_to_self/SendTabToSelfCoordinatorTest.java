// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Tests for SendTabToSelfCoordinator */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.UNIT_TESTS)
public class SendTabToSelfCoordinatorTest {
    @Mock
    private BottomSheetController mBottomSheetController;
    @Mock
    private BottomSheetContent mBottomSheetContent;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @SmallTest
    public void testShow() {
        SendTabToSelfCoordinator.setBottomSheetContentForTesting(mBottomSheetContent);
        SendTabToSelfCoordinator coordinator = new SendTabToSelfCoordinator(/*context*/ null,
                "test", "test", mBottomSheetController, /*settingsLauncher=*/null,
                /*isSyncEnabled=*/true, /*navigationTime=*/0);
        coordinator.show();
        verify(mBottomSheetController).requestShowContent(any(BottomSheetContent.class), eq(true));
        SendTabToSelfCoordinator.setBottomSheetContentForTesting(null);
    }
}
