// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.ComponentName;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Bundle;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.ui.display.DisplayAndroid;

/** Unit test for {@link PopupCreator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PopupCreatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock Activity mActivity;
    @Mock Tab mTab;
    @Mock DisplayAndroid mDisplay;
    @Mock ReparentingTask mReparentingTask;

    private static final int DISPLAY_ID = 73;
    private static final float DENSITY = 1.0f;

    @Before
    public void setup() {
        PopupCreator.setReparentingTaskForTesting(mReparentingTask);
        doReturn(DISPLAY_ID).when(mDisplay).getDisplayId();
        doReturn(DENSITY).when(mDisplay).getDipScale();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)
    public void testPopupsDisabledWhenFeatureDisabled() {
        Assert.assertFalse(
                "Popups should not be enabled when the feature is disabled",
                PopupCreator.arePopupsEnabled(mActivity));
    }

    @Test
    public void testIntentParams() {
        PopupCreator.moveTabToNewPopup(mTab, new WindowFeatures(), mDisplay);

        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(mReparentingTask).begin(any(), captor.capture(), any(), any());
        Intent sentIntent = captor.getValue();

        Assert.assertEquals(
                "The intent sent to reparenting task is not targeted at CustomTabActivity.class",
                new ComponentName(ContextUtils.getApplicationContext(), CustomTabActivity.class),
                sentIntent.getComponent());
        Assert.assertEquals(
                "The intent sent to reparenting task doesn't specify POPUP CCT UI type",
                CustomTabsUiType.POPUP,
                sentIntent.getIntExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, -1));
        Assert.assertEquals(
                "The intent sent to reparenting task doesn't specify FLAG_ACTIVITY_NEW_TASK",
                Intent.FLAG_ACTIVITY_NEW_TASK,
                sentIntent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK);
    }

    private ActivityOptions getActivityOptionsPassedToReparentingTask() {
        ArgumentCaptor<Bundle> captor = ArgumentCaptor.forClass(Bundle.class);
        verify(mReparentingTask).begin(any(), any(), captor.capture(), any());
        Bundle aoBundle = captor.getValue();
        return new ActivityOptions(aoBundle);
    }

    @Test
    public void testActivityOptionsSetWhenWindowFeaturesComplete() {
        WindowFeatures windowFeatures = new WindowFeatures(100, 200, 300, 400);

        PopupCreator.moveTabToNewPopup(mTab, windowFeatures, mDisplay);
        ActivityOptions activityOptions = getActivityOptionsPassedToReparentingTask();

        Assert.assertEquals(
                "The launch display ID specified in ActivityOptions is incorrect",
                mDisplay.getDisplayId(),
                activityOptions.getLaunchDisplayId());
        Assert.assertNotNull(
                "The launch bounds specified in ActivityOptions are null",
                activityOptions.getLaunchBounds());
    }

    @Test
    public void testNullBoundsWhenWindowFeaturesDegenerated() {
        WindowFeatures windowFeatures = new WindowFeatures(null, null, null, 100);

        PopupCreator.moveTabToNewPopup(mTab, windowFeatures, mDisplay);
        ActivityOptions activityOptions = getActivityOptionsPassedToReparentingTask();

        Assert.assertNull(
                "The launch bounds specified in ActivityOptions should be null",
                activityOptions.getLaunchBounds());
    }

    @Test
    public void testSizeIsPreservedIfSpecifiedInWindowFeatures() {
        WindowFeatures windowFeatures = new WindowFeatures(null, null, 300, 400);

        PopupCreator.moveTabToNewPopup(mTab, windowFeatures, mDisplay);
        ActivityOptions activityOptions = getActivityOptionsPassedToReparentingTask();

        Rect launchBounds = activityOptions.getLaunchBounds();
        Rect targetBounds = new Rect(0, 0, 300, 400);
        launchBounds.offsetTo(0, 0);
        targetBounds.offsetTo(0, 0);

        Assert.assertEquals(
                "The launch bounds specified in ActivityOptions have not preserved the size"
                        + " provided",
                targetBounds,
                launchBounds);
    }
}
