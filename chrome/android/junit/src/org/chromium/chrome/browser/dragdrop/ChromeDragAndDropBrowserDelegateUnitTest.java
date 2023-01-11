// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.net.Uri;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.url.JUnitTestGURLs;

/**
 * Unit test for {@link ChromeDragAndDropBrowserDelegate}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeDragAndDropBrowserDelegateUnitTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Activity mActivity;
    @Mock
    private DragEvent mDragEvent;
    @Mock
    private DragAndDropPermissions mDragAndDropPermissions;
    @Mock
    private ActivityInfo mActivityInfo;
    @Mock
    private PackageManager mPackageManager;

    private ChromeDragAndDropBrowserDelegate mDelegate;
    private FeatureList.TestValues mTestValues;

    @Before
    public void setup() throws NameNotFoundException {
        mTestValues = new TestValues();
        mTestValues.addFeatureFlagOverride(ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU, true);
        mTestValues.addFeatureFlagOverride(ChromeFeatureList.ANIMATED_IMAGE_DRAG_SHADOW, false);
        FeatureList.setTestValues(mTestValues);

        Context mApplicationContext = Mockito.spy(ContextUtils.getApplicationContext());
        when(mApplicationContext.getPackageManager()).thenReturn(mPackageManager);
        when(mPackageManager.getActivityInfo(any(), anyInt())).thenReturn(mActivityInfo);
        ContextUtils.initApplicationContextForTests(mApplicationContext);

        when(mActivity.requestDragAndDropPermissions(mDragEvent))
                .thenReturn(mDragAndDropPermissions);
        when(mActivity.getApplicationContext())
                .thenReturn(ApplicationProvider.getApplicationContext());

        mDelegate = new ChromeDragAndDropBrowserDelegate(mActivity);
    }

    @Test
    public void testDragAndDropBrowserDelegate_getDragAndDropPermissions() {
        mTestValues.addFieldTrialParamOverride(ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU,
                ChromeDragAndDropBrowserDelegate.PARAM_DROP_IN_CHROME, "true");
        mDelegate = new ChromeDragAndDropBrowserDelegate(mActivity);
        assertTrue("SupportDropInChrome should be true.", mDelegate.getSupportDropInChrome());
        assertFalse("SupportAnimatedImageDragShadow should be false.",
                mDelegate.getSupportAnimatedImageDragShadow());

        DragAndDropPermissions permissions = mDelegate.getDragAndDropPermissions(mDragEvent);
        assertNotNull("DragAndDropPermissions should not be null.", permissions);
    }

    @Test
    public void testDragAndDropBrowserDelegate_NotSupportDropInChrome() {
        mTestValues.addFieldTrialParamOverride(ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU,
                ChromeDragAndDropBrowserDelegate.PARAM_DROP_IN_CHROME, "false");
        mDelegate = new ChromeDragAndDropBrowserDelegate(mActivity);
        assertFalse("SupportDropInChrome should be false.", mDelegate.getSupportDropInChrome());

        AssertionError error = null;
        try {
            mDelegate.getDragAndDropPermissions(mDragEvent);
        } catch (AssertionError e) {
            error = e;
        }

        assertNotNull("getDragAndDropPermissions should raise assert exception "
                        + "when accessed with drop in Chrome disabled.",
                error);
    }

    @Test
    @Config(sdk = 30)
    public void testDragAndDropBrowserDelegate_createLinkIntent_PostR() {
        mActivityInfo.launchMode = ActivityInfo.LAUNCH_SINGLE_INSTANCE_PER_TASK;
        Intent intent = mDelegate.createLinkIntent(JUnitTestGURLs.EXAMPLE_URL);
        assertEquals("The intent flags should match.",
                Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK,
                intent.getFlags());
        assertEquals("The intent class should be ChromeTabbedActivity.",
                ChromeTabbedActivity.class.getName(), intent.getComponent().getClassName());
        assertTrue("preferNew extra should be true.",
                intent.getBooleanExtra(IntentHandler.EXTRA_PREFER_NEW, false));
        assertEquals("The intent should contain Uri data.", Uri.parse(JUnitTestGURLs.EXAMPLE_URL),
                intent.getData());
    }

    @Test
    @Config(sdk = 29)
    public void testDragAndDropBrowserDelegate_createLinkIntent_PreR() {
        Intent intent = mDelegate.createLinkIntent(JUnitTestGURLs.EXAMPLE_URL);
        assertNull("The intent should be null on R- versions.", intent);
    }
}
