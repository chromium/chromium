// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import static org.junit.Assert.assertEquals;

import android.content.Intent;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropType;
import org.chromium.ui.dragdrop.DragDropMetricUtils.UrlIntentSource;

/** Unit tests for {@link ChromeDragDropUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeDragDropUtilsUnitTest {
    @Test
    public void testGetDragDropTypeFromIntent() {
        testGetDragDropTypeFromIntent(UrlIntentSource.LINK, DragDropType.LINK_TO_NEW_INSTANCE);
        testGetDragDropTypeFromIntent(
                UrlIntentSource.TAB_IN_STRIP, DragDropType.TAB_STRIP_TO_NEW_INSTANCE);
        testGetDragDropTypeFromIntent(
                UrlIntentSource.UNKNOWN, DragDropType.UNKNOWN_TO_NEW_INSTANCE);
    }

    private void testGetDragDropTypeFromIntent(
            @UrlIntentSource int intentSrc, @DragDropType int dragDropType) {
        Intent intent = new Intent();
        intent.putExtra(IntentHandler.EXTRA_URL_DRAG_SOURCE, intentSrc);
        assertEquals(
                "The DragDropType should match.",
                dragDropType,
                ChromeDragDropUtils.getDragDropTypeFromIntent(intent));
    }
}
