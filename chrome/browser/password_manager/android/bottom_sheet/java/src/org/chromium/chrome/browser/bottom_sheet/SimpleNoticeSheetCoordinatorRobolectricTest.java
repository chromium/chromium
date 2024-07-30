// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottom_sheet;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;

import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.VISIBLE;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link SimpleNoticeSheetCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class SimpleNoticeSheetCoordinatorRobolectricTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private SimpleNoticeSheetCoordinator mCoordinator;

    @Mock private BottomSheetController mBottomSheetController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mCoordinator = new SimpleNoticeSheetCoordinator(mBottomSheetController);
    }

    @Test
    public void testShowWarningUpdatesModel() {
        PropertyModel model = mCoordinator.getModel();
        assertNotNull(model.get(DISMISS_HANDLER));
        assertThat(model.get(VISIBLE), is(false));

        mCoordinator.showSheet();

        assertThat(model.get(VISIBLE), is(true));
    }
}
