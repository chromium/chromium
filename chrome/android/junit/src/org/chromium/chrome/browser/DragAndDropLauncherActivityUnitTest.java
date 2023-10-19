// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link DragAndDropLauncherActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 31)
public class DragAndDropLauncherActivityUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public ExpectedException exception = ExpectedException.none();

    @Mock private ActivityInfo mActivityInfo;
    @Mock private PackageManager mPackageManager;

    private Context mContext;
    private String mLinkUrl;

    @Before
    public void setup() throws NameNotFoundException {
        mContext = Mockito.spy(ContextUtils.getApplicationContext());
        when(mContext.getPackageManager()).thenReturn(mPackageManager);
        when(mPackageManager.getActivityInfo(any(), anyInt())).thenReturn(mActivityInfo);
        ContextUtils.initApplicationContextForTests(mContext);
        mActivityInfo.launchMode = ActivityInfo.LAUNCH_SINGLE_INSTANCE_PER_TASK;
        mLinkUrl = JUnitTestGURLs.HTTP_URL.getSpec();
    }

    @Test
    public void testGetLinkLauncherIntent_defaultWindowId() {
        Intent intent = DragAndDropLauncherActivity.getLinkLauncherIntent(mContext, mLinkUrl, null);
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
        Intent intent = DragAndDropLauncherActivity.getLinkLauncherIntent(mContext, mLinkUrl, 3);
        Assert.assertTrue(
                "Intent should contain the EXTRA_WINDOW_ID.",
                intent.hasExtra(IntentHandler.EXTRA_WINDOW_ID));
        assertEquals(
                "The EXTRA_WINDOW_ID intent extra value should match.",
                3,
                intent.getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
    }

    @Test
    public void testIsIntentValid_invalidIntentAction() {
        Intent intent = DragAndDropLauncherActivity.getLinkLauncherIntent(mContext, mLinkUrl, null);
        intent.setAction(Intent.ACTION_VIEW);
        exception.expect(AssertionError.class);
        exception.expectMessage("The intent action is invalid.");
        Assert.assertFalse(
                "The intent action is invalid.", DragAndDropLauncherActivity.isIntentValid(intent));
    }

    @Test
    public void testIsIntentValid_missingIntentCreationTimestamp() {
        Intent intent = DragAndDropLauncherActivity.getLinkLauncherIntent(mContext, mLinkUrl, null);
        DragAndDropLauncherActivity.setLinkIntentCreationTimestampMs(null);
        Assert.assertFalse(
                "The intent creation timestamp is missing.",
                DragAndDropLauncherActivity.isIntentValid(intent));
    }
}
