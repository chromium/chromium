// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.app.tab_activity_glue.PopupCreator.EXTRA_REQUESTED_WINDOW_FEATURES;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Rect;
import android.os.Bundle;
import android.view.Display;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.IncognitoCctCallerId;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.ui.insets.InsetObserver;

/** Unit test for {@link PopupCreator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PopupCreatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock Activity mActivity;
    @Mock Tab mTab;
    @Mock WindowAndroid mWindow;
    @Mock DisplayAndroid mDisplay;
    @Mock DisplayAndroid mExternalDisplay;
    @Mock DisplayAndroidManager mDisplayAndroidManager;
    @Mock ReparentingTask mReparentingTask;
    @Mock AconfigFlaggedApiDelegate mFlaggedApiDelegate;
    @Mock Context mContext;
    @Mock Resources mResources;
    @Mock InsetObserver mInsetObserver;
    @Mock WindowInsetsCompat mWindowInsetsCompat;

    private static final int DISPLAY_ID = 73;
    private static final float DENSITY = 1.0f;
    private static final Rect BOUNDS = new Rect(0, 0, 1920, 1080);
    private static final Rect LOCAL_BOUNDS = new Rect(0, 0, 1920, 1080);

    private static final int EXTERNAL_DISPLAY_ID = 101;
    private static final float EXTERNAL_DISPLAY_DENSITY = 2.0f;
    private static final Rect EXTERNAL_DISPLAY_BOUNDS = new Rect(-290, -1250, 2210, 0);
    private static final Rect EXTERNAL_DISPLAY_LOCAL_BOUNDS = new Rect(0, 0, 5000, 2500);

    private static final Insets LAST_RAW_WINDOW_INSETS = Insets.of(12, 34, 56, 78);
    private static final int CUSTOM_TABS_CONTROL_CONTAINER_HEIGHT = 20;
    private static final int TOOLBAR_HAIRLINE_HEIGHT = 9;

    @Before
    public void setup() {
        DisplayAndroidManager.setInstanceForTesting(mDisplayAndroidManager);

        PopupCreator.setReparentingTaskForTesting(mReparentingTask);
        PopupCreator.setInsetsForecastForTesting(Insets.NONE);

        doReturn(DISPLAY_ID).when(mDisplay).getDisplayId();
        doReturn(DENSITY).when(mDisplay).getDipScale();
        doReturn(BOUNDS).when(mDisplay).getBounds();
        doReturn(LOCAL_BOUNDS).when(mDisplay).getLocalBounds();
        doReturn(mContext).when(mDisplay).getWindowContext();

        doReturn(EXTERNAL_DISPLAY_ID).when(mExternalDisplay).getDisplayId();
        doReturn(EXTERNAL_DISPLAY_DENSITY).when(mExternalDisplay).getDipScale();
        doReturn(EXTERNAL_DISPLAY_BOUNDS).when(mExternalDisplay).getBounds();
        doReturn(EXTERNAL_DISPLAY_LOCAL_BOUNDS).when(mExternalDisplay).getLocalBounds();
        doReturn(mContext).when(mExternalDisplay).getWindowContext();

        doReturn(mWindow).when(mTab).getWindowAndroid();
        doReturn(mDisplay).when(mWindow).getDisplay();
        doReturn(mInsetObserver).when(mWindow).getInsetObserver();
        doReturn(mWindowInsetsCompat).when(mInsetObserver).getLastRawWindowInsets();
        doReturn(LAST_RAW_WINDOW_INSETS)
                .when(mWindowInsetsCompat)
                .getInsets(WindowInsetsCompat.Type.captionBar());

        doReturn(mResources).when(mContext).getResources();
        doReturn(CUSTOM_TABS_CONTROL_CONTAINER_HEIGHT)
                .when(mResources)
                .getDimensionPixelSize(R.dimen.custom_tabs_control_container_height);
        doReturn(TOOLBAR_HAIRLINE_HEIGHT)
                .when(mResources)
                .getDimensionPixelSize(R.dimen.toolbar_hairline_height);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)
    public void testPopupsDisabledWhenFeatureDisabled() {
        Assert.assertFalse(
                "Popups should not be enabled when the feature is disabled",
                PopupCreator.arePopupsEnabled(new WindowFeatures(), mDisplay));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)
    public void testPopupsDisabledWhenDelegateReturnsFalse() {
        AconfigFlaggedApiDelegate.setInstanceForTesting(mFlaggedApiDelegate);
        doReturn(false)
                .when(mFlaggedApiDelegate)
                .isTaskMoveAllowedOnDisplay(any(ActivityManager.class), eq(DISPLAY_ID));

        Assert.assertFalse(
                "Popups should not be enabled if the delegate returns false when"
                        + " isTaskMoveAllowedOnDisplay is called",
                PopupCreator.arePopupsEnabled(new WindowFeatures(), mDisplay));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)
    public void testPopupsDisabledWhenFlaggedApiDelegateNull() {
        AconfigFlaggedApiDelegate.setInstanceForTesting(null);
        Assert.assertFalse(
                "Popups should not be enabled if the delegate is null",
                PopupCreator.arePopupsEnabled(new WindowFeatures(), mDisplay));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)
    public void testPopupsEnabledWhenDelegateReturnsTrue() {
        AconfigFlaggedApiDelegate.setInstanceForTesting(mFlaggedApiDelegate);
        doReturn(true)
                .when(mFlaggedApiDelegate)
                .isTaskMoveAllowedOnDisplay(any(ActivityManager.class), eq(DISPLAY_ID));

        Assert.assertTrue(
                "Popups should be enabled if the delegate returns true when"
                        + " isTaskMoveAllowedOnDisplay is called",
                PopupCreator.arePopupsEnabled(new WindowFeatures(), mDisplay));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_LARGE_SCREEN)
    public void testPopupsEnabledWhenDelegateReturnsTrue_WindowFeaturesOnDifferentDisplay() {
        final WindowFeatures windowFeatures =
                new WindowFeatures(100, 200, 300, 400); // left, top, width, height
        final Rect windowBounds = new Rect(100, 200, 400, 600); // left, top, right, bottom
        doReturn(mExternalDisplay).when(mDisplayAndroidManager).getDisplayMatching(windowBounds);

        AconfigFlaggedApiDelegate.setInstanceForTesting(mFlaggedApiDelegate);
        doReturn(false)
                .when(mFlaggedApiDelegate)
                .isTaskMoveAllowedOnDisplay(any(ActivityManager.class), eq(DISPLAY_ID));
        doReturn(true)
                .when(mFlaggedApiDelegate)
                .isTaskMoveAllowedOnDisplay(any(ActivityManager.class), eq(EXTERNAL_DISPLAY_ID));

        Assert.assertTrue(
                "Popups should be enabled if the delegate returns true when"
                        + " isTaskMoveAllowedOnDisplay is called",
                PopupCreator.arePopupsEnabled(windowFeatures, mDisplay));
    }

    @Test
    public void testIntentParams() {
        final WindowFeatures windowFeatures = new WindowFeatures(12, 34, 56, null);
        PopupCreator.moveTabToNewPopup(mTab, windowFeatures);

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
        Assert.assertEquals(
                "The intent sent to reparenting task doesn't specify a correct Bundle of requested"
                        + " window features",
                windowFeatures,
                new WindowFeatures(sentIntent.getBundleExtra(EXTRA_REQUESTED_WINDOW_FEATURES)));
    }

    @Test
    public void testIntentParams_incognitoOpener() {
        final WindowFeatures windowFeatures = new WindowFeatures(12, 34, 56, null);
        doReturn(true).when(mTab).isIncognitoBranded();
        PopupCreator.moveTabToNewPopup(mTab, windowFeatures);

        final ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(mReparentingTask).begin(any(), captor.capture(), any(), any());
        final Intent sentIntent = captor.getValue();

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
        Assert.assertEquals(
                "The intent sent to reparenting task doesn't specify a correct Bundle of requested"
                        + " window features",
                windowFeatures,
                new WindowFeatures(sentIntent.getBundleExtra(EXTRA_REQUESTED_WINDOW_FEATURES)));
        Assert.assertEquals(
                "The intent sent to reparenting task doesn't specify Incognito CCT Caller ID",
                IncognitoCctCallerId.CONTEXTUAL_POPUP,
                sentIntent.getIntExtra(IntentHandler.EXTRA_INCOGNITO_CCT_CALLER_ID, -1));
    }

    private ActivityOptions getActivityOptionsPassedToReparentingTask() {
        ArgumentCaptor<Bundle> captor = ArgumentCaptor.forClass(Bundle.class);
        verify(mReparentingTask).begin(any(), any(), captor.capture(), any());
        Bundle aoBundle = captor.getValue();
        return new ActivityOptions(aoBundle);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_PREDICT_FINAL_BOUNDS)
    public void testActivityOptionsSetWhenWindowFeaturesComplete() {
        final WindowFeatures windowFeatures =
                new WindowFeatures(100, 200, 300, 400); // left, top, width, height
        final Rect windowBounds = new Rect(100, 200, 400, 600); // left, top, right, bottom

        doReturn(mDisplay).when(mDisplayAndroidManager).getDisplayMatching(windowBounds);

        PopupCreator.moveTabToNewPopup(mTab, windowFeatures);
        ActivityOptions activityOptions = getActivityOptionsPassedToReparentingTask();

        Assert.assertEquals(
                "The launch display ID specified in ActivityOptions is incorrect",
                DISPLAY_ID,
                activityOptions.getLaunchDisplayId());
        Assert.assertEquals(
                "The launch bounds specified in ActivityOptions are incorrect",
                windowBounds,
                activityOptions.getLaunchBounds());
    }

    @Test
    public void testActivityOptionsWhenWindowFeaturesDegenerated() {
        WindowFeatures windowFeatures = new WindowFeatures(null, null, null, 100);

        PopupCreator.moveTabToNewPopup(mTab, windowFeatures);
        ActivityOptions activityOptions = getActivityOptionsPassedToReparentingTask();

        Assert.assertEquals(
                "The launch display ID specified in ActivityOptions should be invalid",
                Display.INVALID_DISPLAY,
                activityOptions.getLaunchDisplayId());
        Assert.assertNull(
                "The launch bounds specified in ActivityOptions should be null",
                activityOptions.getLaunchBounds());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_PREDICT_FINAL_BOUNDS)
    public void testSizeIsPreservedIfSpecifiedInWindowFeatures() {
        final WindowFeatures windowFeatures =
                new WindowFeatures(null, null, 300, 400); // left, top, width, height
        final Rect windowBounds = new Rect(0, 0, 300, 400); // left, top, right, bottom

        doReturn(mDisplay).when(mDisplayAndroidManager).getDisplayMatching(windowBounds);

        PopupCreator.moveTabToNewPopup(mTab, windowFeatures);
        ActivityOptions activityOptions = getActivityOptionsPassedToReparentingTask();

        Rect launchBounds = activityOptions.getLaunchBounds();

        Assert.assertEquals(
                "The launch bounds specified in ActivityOptions have not preserved the provided"
                        + " width",
                windowBounds.width(),
                launchBounds.width());
        Assert.assertEquals(
                "The launch bounds specified in ActivityOptions have not preserved the provided"
                        + " height",
                windowBounds.height(),
                launchBounds.height());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_PREDICT_FINAL_BOUNDS)
    public void testRequestedBoundsAreClampedToDisplayBounds_predictionFlagDisabled() {
        final Rect displayLocalBounds = new Rect(0, 0, 600, 800);
        doReturn(displayLocalBounds).when(mDisplay).getLocalBounds();
        final WindowFeatures windowFeatures =
                new WindowFeatures(-100, -100, 1000, 1000); // left, top, width, height
        final Rect windowBounds = new Rect(-100, -100, 900, 900); // left, top, right, bottom

        doReturn(mDisplay).when(mDisplayAndroidManager).getDisplayMatching(windowBounds);

        PopupCreator.moveTabToNewPopup(mTab, windowFeatures);
        Rect launchBounds = getActivityOptionsPassedToReparentingTask().getLaunchBounds();

        Assert.assertTrue(
                "The launch bounds specified in ActivityOptions do not fit inside display",
                displayLocalBounds.contains(launchBounds));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_PREDICT_FINAL_BOUNDS)
    public void testRequestedBoundsAreClampedToDisplayBounds_predictionFlagEnabled() {
        final Rect displayLocalBounds = new Rect(0, 0, 600, 800);
        doReturn(displayLocalBounds).when(mDisplay).getLocalBounds();
        final WindowFeatures windowFeatures =
                new WindowFeatures(-100, -100, 1000, 1000); // left, top, width, height
        final Rect windowBounds = new Rect(-100, -100, 900, 900); // left, top, right, bottom

        doReturn(mDisplay).when(mDisplayAndroidManager).getDisplayMatching(windowBounds);

        PopupCreator.moveTabToNewPopup(mTab, windowFeatures);
        Rect launchBounds = getActivityOptionsPassedToReparentingTask().getLaunchBounds();

        Assert.assertTrue(
                "The launch bounds specified in ActivityOptions do not fit inside display",
                displayLocalBounds.contains(launchBounds));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_PREDICT_FINAL_BOUNDS)
    public void testInsetsForecastNotUsedForNewPopups_flagDisabled() {
        PopupCreator.setInsetsForecastForTesting(
                Insets.of(-12, -34, -56, -78)); // left, top, right, bottom
        WindowFeatures windowFeatures =
                new WindowFeatures(100, 200, 300, 400); // left, top, width, height
        final Rect windowBounds = new Rect(100, 200, 400, 600); // left, top, right, bottom

        doReturn(mDisplay).when(mDisplayAndroidManager).getDisplayMatching(windowBounds);

        PopupCreator.moveTabToNewPopup(mTab, windowFeatures);
        ActivityOptions activityOptions = getActivityOptionsPassedToReparentingTask();

        Rect launchBounds = activityOptions.getLaunchBounds();

        Assert.assertEquals(
                "The launch bounds specified in ActivityOptions are incorrect",
                windowBounds,
                launchBounds);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_PREDICT_FINAL_BOUNDS)
    public void testInsetsForecastUsedForNewPopups_flagEnabled() {
        WindowFeatures windowFeatures =
                new WindowFeatures(100, 200, 300, 400); // left, top, width, height
        final Rect windowBounds = new Rect(100, 200, 400, 600); // left, top, right, bottom
        final Insets insets = Insets.of(-12, -34, -56, -78); // left, top, right, bottom

        doReturn(mDisplay).when(mDisplayAndroidManager).getDisplayMatching(windowBounds);

        PopupCreator.setInsetsForecastForTesting(insets);
        PopupCreator.moveTabToNewPopup(mTab, windowFeatures);
        ActivityOptions activityOptions = getActivityOptionsPassedToReparentingTask();

        Rect launchBounds = activityOptions.getLaunchBounds();
        final Rect targetBounds =
                new Rect(100 - 12, 200 - 34, 400 + 56, 600 + 78); // left, top, right, bottom

        Assert.assertEquals(
                "The launch bounds specified in ActivityOptions have not been preserved and outset"
                        + " by given window insets",
                targetBounds,
                launchBounds);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_PREDICT_FINAL_BOUNDS)
    public void testPopupInsetsForecastUseExpectedValues() {
        PopupCreator.setInsetsForecastForTesting(null);

        Assert.assertEquals(
                "The insets returned are invalid",
                Insets.of(0, 0, -(12 + 56), -(34 + 20 + 9 + 78)),
                PopupCreator.getPopupInsetsForecast(mWindow, mDisplay));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_PREDICT_FINAL_BOUNDS)
    public void testPopupInsetsForecastUseExpectedValuesCrossDisplays() {
        PopupCreator.setInsetsForecastForTesting(null);

        Assert.assertEquals(
                "The insets returned are invalid",
                Insets.of(0, 0, -(24 + 112), -(68 + 20 + 9 + 156)),
                PopupCreator.getPopupInsetsForecast(mWindow, mExternalDisplay));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_PREDICT_FINAL_BOUNDS)
    public void testPopupOnExternalDisplay() {
        final WindowFeatures windowFeatures =
                new WindowFeatures(-390, -1350, 300, 400); // left, top, width, height
        final Rect windowBounds = new Rect(-390, -1350, -90, -950); // left, top, right, bottom

        doReturn(mExternalDisplay).when(mDisplayAndroidManager).getDisplayMatching(windowBounds);

        PopupCreator.setInsetsForecastForTesting(null);
        PopupCreator.moveTabToNewPopup(mTab, windowFeatures);

        final ActivityOptions activityOptions = getActivityOptionsPassedToReparentingTask();

        final Rect targetBounds =
                new Rect(
                        0,
                        0,
                        (300 + 12 + 56) * 2,
                        (400 + 34 + 78) * 2 + 20 + 9); // left, top, right, bottom
        Assert.assertEquals(
                "The launch display ID specified in ActivityOptions is incorrect",
                EXTERNAL_DISPLAY_ID,
                activityOptions.getLaunchDisplayId());
        Assert.assertEquals(
                "The launch bounds specified in ActivityOptions is incorrect",
                targetBounds,
                activityOptions.getLaunchBounds());
    }
}
