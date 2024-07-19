// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottom_sheet;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link SimpleNoticeSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class SimpleNoticeSheetMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private SimpleNoticeSheetMediator mMediator = new SimpleNoticeSheetMediator();
    private PropertyModel mModel;

    @Mock private BottomSheetController mBottomSheetController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModel = SimpleNoticeSheetProperties.createDefaultModel(mMediator::onDismissed);
        mMediator.initialize(mModel);
    }

    @Test
    public void testShowWarningChangesVisibility() {
        assertFalse(mModel.get(VISIBLE));
        mMediator.showSheet();
        assertTrue(mModel.get(VISIBLE));
    }

    @Test
    public void testOnDismissedChangesVisibility() {
        mMediator.showSheet();
        assertTrue(mModel.get(VISIBLE));
        mMediator.onDismissed(StateChangeReason.NONE);
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testDismissHandlerChangesVisibility() {
        assertNotNull(mModel.get(DISMISS_HANDLER));
        assertFalse(mModel.get(VISIBLE));
        mMediator.showSheet();
        assertTrue(mModel.get(VISIBLE));
        mModel.get(DISMISS_HANDLER).onResult(StateChangeReason.NONE);
        assertFalse(mModel.get(VISIBLE));
    }
}
