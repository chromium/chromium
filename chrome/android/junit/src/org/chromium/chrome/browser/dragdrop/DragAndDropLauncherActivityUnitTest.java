// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;

import org.chromium.chrome.browser.IntentHandler;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropType;
import org.chromium.ui.dragdrop.DragDropMetricUtils.UrlIntentSource;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link DragAndDropLauncherActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 31)
public class DragAndDropLauncherActivityUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public ExpectedException exception = ExpectedException.none();

    private Context mContext;
    private String mLinkUrl;

    @Before
    public void setup() {
        MultiWindowTestUtils.enableMultiInstance();
        mContext = ContextUtils.getApplicationContext();
        mLinkUrl = JUnitTestGURLs.HTTP_URL.getSpec();
    }

    @Test
    public void testGetLinkLauncherIntent_defaultWindowId() {
        Intent intent =
                DragAndDropLauncherActivity.getLinkLauncherIntent(
                        mContext,
                        mLinkUrl,
                        MultiWindowUtils.INVALID_INSTANCE_ID,
                        UrlIntentSource.LINK);
        assertEquals(
                "The intent action should be DragAndDropLauncherActivity.ACTION_DRAG_DROP_VIEW.",
                DragAndDropLauncherActivity.ACTION_DRAG_DROP_VIEW,
                intent.getAction());
        assertNotNull(
                "The intent creation timestamp should be saved.",
                DragAndDropLauncherActivity.getLinkIntentCreationTimestampMs());
        assertEquals(
                "The intent class should be DragAndDropLauncherActivity.",
                DragAndDropLauncherActivity.class.getName(),
                intent.getComponent().getClassName());
        assertTrue(
                "The intent should contain the CATEGORY_BROWSABLE category.",
                intent.getCategories().contains(Intent.CATEGORY_BROWSABLE));
        Assert.assertFalse(
                "Intent should not contain the EXTRA_WINDOW_ID.",
                intent.hasExtra(IntentHandler.EXTRA_WINDOW_ID));
    }

    @Test
    public void testGetLinkLauncherIntent_specificWindowId() {
        Intent intent =
                DragAndDropLauncherActivity.getLinkLauncherIntent(
                        mContext, mLinkUrl, 3, UrlIntentSource.LINK);
        Assert.assertTrue(
                "Intent should contain the EXTRA_WINDOW_ID.",
                intent.hasExtra(IntentHandler.EXTRA_WINDOW_ID));
        assertEquals(
                "The EXTRA_WINDOW_ID intent extra value should match.",
                3,
                intent.getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
        assertEquals(
                "The EXTRA_URL_SOURCE intent extra value should match.",
                UrlIntentSource.LINK,
                intent.getIntExtra(IntentHandler.EXTRA_URL_DRAG_SOURCE, UrlIntentSource.UNKNOWN));
    }

    @Test
    public void testGetTabLauncherIntent_specificWindowId() {
        Intent intent =
                DragAndDropLauncherActivity.getLinkLauncherIntent(
                        mContext, mLinkUrl, 2, UrlIntentSource.TAB_IN_STRIP);
        Assert.assertTrue(
                "Intent should contain the EXTRA_WINDOW_ID.",
                intent.hasExtra(IntentHandler.EXTRA_WINDOW_ID));
        assertEquals(
                "The EXTRA_WINDOW_ID intent extra value should match.",
                2,
                intent.getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
        assertEquals(
                "The EXTRA_URL_SOURCE intent extra value should match.",
                UrlIntentSource.TAB_IN_STRIP,
                intent.getIntExtra(IntentHandler.EXTRA_URL_DRAG_SOURCE, UrlIntentSource.UNKNOWN));
    }

    @Test
    public void testIsIntentValid_invalidIntentAction() {
        Intent intent =
                DragAndDropLauncherActivity.getLinkLauncherIntent(
                        mContext,
                        mLinkUrl,
                        MultiWindowUtils.INVALID_INSTANCE_ID,
                        UrlIntentSource.LINK);
        intent.setAction(Intent.ACTION_VIEW);
        exception.expect(AssertionError.class);
        exception.expectMessage("The intent action is invalid.");
        Assert.assertFalse(
                "The intent action is invalid.", DragAndDropLauncherActivity.isIntentValid(intent));
    }

    @Test
    public void testIsIntentValid_missingIntentCreationTimestamp() {
        Intent intent =
                DragAndDropLauncherActivity.getLinkLauncherIntent(
                        mContext,
                        mLinkUrl,
                        MultiWindowUtils.INVALID_INSTANCE_ID,
                        UrlIntentSource.LINK);
        DragAndDropLauncherActivity.setLinkIntentCreationTimestampMs(null);
        Assert.assertFalse(
                "The intent creation timestamp is missing.",
                DragAndDropLauncherActivity.isIntentValid(intent));
    }

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
                DragAndDropLauncherActivity.getDragDropTypeFromIntent(intent));
    }
}
