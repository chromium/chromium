// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.app.tab_activity_glue.PopupCreator.EXTRA_REQUESTED_WINDOW_FEATURES;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Rect;
import android.os.Bundle;
import android.util.AndroidRuntimeException;
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
import org.chromium.chrome.browser.util.PictureInPictureWindowOptions;
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.ui.insets.InsetObserver;

/** Unit test for {@link PopupCreator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_PREDICT_FINAL_BOUNDS)
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
    @Mock WebContents mWebContents;

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
    private static final int CUSTOM_TABS_POPUP_TITLE_BAR_MIN_HEIGHT = 62;
    private static final int CUSTOM_TABS_POPUP_TITLE_BAR_TEXT_HEIGHT = 75;

    @Before
    public void setup() {
        DisplayAndroidManager.setInstanceForTesting(mDisplayAndroidManager);

        PopupCreator.setReparentingTaskForTesting(mReparentingTask);
        PopupCreator.setInsetsForecastForTesting(Insets.NONE);
        PopupCreator.initializePopupIntentCreator();

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

        doReturn(mActivity).when(mTab).getContext();
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
        doReturn(CUSTOM_TABS_POPUP_TITLE_BAR_MIN_HEIGHT)
                .when(mResources)
                .getDimensionPixelSize(R.dimen.custom_tabs_popup_title_bar_min_height);
        doReturn(CUSTOM_TABS_POPUP_TITLE_BAR_TEXT_HEIGHT)
                .when(mResources)
                .getDimensionPixelSize(R.dimen.custom_tabs_popup_title_bar_text_height);

        AconfigFlaggedApiDelegate.setInstanceForTesting(mFlaggedApiDelegate);
        doAnswer(
                        invocation -> {
                            return invocation.getArgument(0);
                        })
                .when(mFlaggedApiDelegate)
                .setMovableTaskRequired(any());

        doReturn(mWindow).when(mWebContents).getTopLevelNativeWindow();
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
        verify(mFlaggedApiDelegate).setMovableTaskRequired(any());
    }

    @Test
    public void testActivityOptionsWhenWindowFeaturesDegenerated_trivialApiDelegate() {
        doReturn(null).when(mFlaggedApiDelegate).setMovableTaskRequired(any());

        WindowFeatures windowFeatures = new WindowFeatures(null, null, null, 100);

        Assert.assertFalse(
                "moveTabToNewPopup should have returned false",
                PopupCreator.moveTabToNewPopup(mTab, windowFeatures));
        verify(mReparentingTask, never()).begin(any(), any(), any(), any());
        verify(mFlaggedApiDelegate).setMovableTaskRequired(any());
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
    public void testRequestedBoundsAreClampedToDisplayBounds() {
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
    public void testInsetsForecastUsedForNewPopups() {
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
    @DisableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_CUSTOM_TAB_UI)
    public void testPopupInsetsForecastUseExpectedValues_standardUiMode() {
        PopupCreator.setInsetsForecastForTesting(null);

        /* Insets.of(
         *     0,
         *     0,
         *     -(left inset + right inset),
         *     -(top inset + CCT toolbar height + hairline height + bottom inset)) */
        Assert.assertEquals(
                "The insets returned are invalid",
                Insets.of(0, 0, -(12 + 56), -(34 + 20 + 9 + 78)),
                PopupCreator.getPopupInsetsForecast(mWindow, mDisplay));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_CUSTOM_TAB_UI)
    public void testPopupInsetsForecastUseExpectedValuesCrossDisplays_standardUiMode() {
        PopupCreator.setInsetsForecastForTesting(null);

        /* Pixel values of insets are scaled by the density quotient between displays.
         * Insets.of(
         *     0,
         *     0,
         *     -(left inset + right inset),
         *     -(top inset + CCT toolbar height + hairline height + bottom inset)) */
        Assert.assertEquals(
                "The insets returned are invalid",
                Insets.of(0, 0, -(24 + 112), -(68 + 20 + 9 + 156)),
                PopupCreator.getPopupInsetsForecast(mWindow, mExternalDisplay));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_CUSTOM_TAB_UI)
    public void testPopupInsetsForecastUseExpectedValues_E2EUiMode() {
        PopupCreator.setInsetsForecastForTesting(null);

        /* Insets.of(
         *     0,
         *     0,
         *     -(left inset + right inset),
         *     -(popup header height + CCT toolbar height + hairline height + bottom inset)) */
        Assert.assertEquals(
                "The insets returned are invalid",
                Insets.of(0, 0, -(12 + 56), -(75 + 20 + 9 + 78)),
                PopupCreator.getPopupInsetsForecast(mWindow, mDisplay));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_CUSTOM_TAB_UI)
    public void testPopupInsetsForecastUseExpectedValuesCrossDisplays_E2EUiMode() {
        PopupCreator.setInsetsForecastForTesting(null);

        /* Pixel values of insets are scaled by the density quotient between displays.
         * Insets.of(
         *     0,
         *     0,
         *     -(left inset + right inset),
         *     -(popup header height + CCT toolbar height + hairline height + bottom inset)) */
        Assert.assertEquals(
                "The insets returned are invalid",
                Insets.of(0, 0, -(24 + 112), -(75 + 20 + 9 + 156)),
                PopupCreator.getPopupInsetsForecast(mWindow, mExternalDisplay));
    }

    /**
     * In this test case the minimal height of popup header is smaller than the top inset. The popup
     * header height should match the top inset height.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_CUSTOM_TAB_UI)
    public void testPopupInsetsForecastUseExpectedValues_E2EUiMode_smallHeader() {
        PopupCreator.setInsetsForecastForTesting(null);
        doReturn(10)
                .when(mResources)
                .getDimensionPixelSize(R.dimen.custom_tabs_popup_title_bar_min_height);
        doReturn(10)
                .when(mResources)
                .getDimensionPixelSize(R.dimen.custom_tabs_popup_title_bar_text_height);

        /* Insets.of(
         *     0,
         *     0,
         *     -(left inset + right inset),
         *     -(top inset + CCT toolbar height + hairline height + bottom inset)) */
        Assert.assertEquals(
                "The insets returned are invalid",
                Insets.of(0, 0, -(12 + 56), -(34 + 20 + 9 + 78)),
                PopupCreator.getPopupInsetsForecast(mWindow, mDisplay));
    }

    /**
     * In this test case the minimal height of popup header is smaller than the top inset. The popup
     * header height should match the top inset height.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_CUSTOM_TAB_UI)
    public void testPopupInsetsForecastUseExpectedValuesCrossDisplays_E2EUiMode_smallHeader() {
        PopupCreator.setInsetsForecastForTesting(null);
        doReturn(10)
                .when(mResources)
                .getDimensionPixelSize(R.dimen.custom_tabs_popup_title_bar_min_height);
        doReturn(10)
                .when(mResources)
                .getDimensionPixelSize(R.dimen.custom_tabs_popup_title_bar_text_height);

        /* Pixel values of insets are scaled by the density quotient between displays.
         * Insets.of(
         *     0,
         *     0,
         *     -(left inset + right inset),
         *     -(top inset + CCT toolbar height + hairline height + bottom inset)) */
        Assert.assertEquals(
                "The insets returned are invalid",
                Insets.of(0, 0, -(24 + 112), -(68 + 20 + 9 + 156)),
                PopupCreator.getPopupInsetsForecast(mWindow, mExternalDisplay));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_CUSTOM_TAB_UI)
    public void testPopupOnExternalDisplay_standardUiMode() {
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

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_WINDOW_POPUP_CUSTOM_TAB_UI)
    public void testPopupOnExternalDisplay_E2EUiMode() {
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
                        (400 + 78) * 2 + 75 + 20 + 9); // left, top, right, bottom
        Assert.assertEquals(
                "The launch display ID specified in ActivityOptions is incorrect",
                EXTERNAL_DISPLAY_ID,
                activityOptions.getLaunchDisplayId());
        Assert.assertEquals(
                "The launch bounds specified in ActivityOptions is incorrect",
                targetBounds,
                activityOptions.getLaunchBounds());
    }

    @Test
    public void testMoveWebContentsToNewDocPipWindow_startsActivity() {
        ContextUtils.initApplicationContextForTests(mContext);
        final PictureInPictureWindowOptions windowOptions = new PictureInPictureWindowOptions();

        PopupCreator.moveWebContentsToNewDocumentPictureInPictureWindow(
                mWebContents, windowOptions);

        verify(mFlaggedApiDelegate).setMovableTaskRequired(any());
        verify(mContext).startActivity(any(), any());
    }

    @Test
    public void testMoveWebContentsToNewDocPipWindow_trivialApiDelegate() {
        doReturn(null).when(mFlaggedApiDelegate).setMovableTaskRequired(any());

        ContextUtils.initApplicationContextForTests(mContext);
        final PictureInPictureWindowOptions windowOptions = new PictureInPictureWindowOptions();

        Assert.assertFalse(
                "moveWebContentsToNewDocumentPictureInPictureWindow should have returned false",
                PopupCreator.moveWebContentsToNewDocumentPictureInPictureWindow(
                        mWebContents, windowOptions));
        verify(mFlaggedApiDelegate).setMovableTaskRequired(any());
        verify(mContext, never()).startActivity(any(), any());
    }

    @Test
    public void testTryStartActivity_success() {
        final Intent intent = mock(Intent.class);
        final Bundle ao = new Bundle();

        Assert.assertTrue(
                "tryStartActivity should have returned true due to success",
                PopupCreator.tryStartActivity(mContext, intent, ao));
        verify(mContext).startActivity(intent, ao);
    }

    @Test
    public void testTryStartActivity_securityException() {
        final Intent intent = mock(Intent.class);
        final Bundle ao = new Bundle();
        doThrow(new SecurityException()).when(mContext).startActivity(intent, ao);

        Assert.assertFalse(
                "tryStartActivity should have returned false due to an exception being thrown",
                PopupCreator.tryStartActivity(mContext, intent, ao));
        verify(mContext).startActivity(intent, ao);
    }

    @Test
    public void testTryStartActivity_infeasibleActivityOptionsException() {
        final Intent intent = mock(Intent.class);
        final Bundle ao = new Bundle();
        final AndroidRuntimeException e = new AndroidRuntimeException();
        doThrow(e).when(mContext).startActivity(intent, ao);
        doReturn(true).when(mFlaggedApiDelegate).isInfeasibleActivityOptionsException(e);

        Assert.assertFalse(
                "tryStartActivity should have returned false due to an exception being thrown",
                PopupCreator.tryStartActivity(mContext, intent, ao));
        verify(mContext).startActivity(intent, ao);
        verify(mFlaggedApiDelegate).isInfeasibleActivityOptionsException(e);
    }

    @Test
    public void testTryStartActivity_genericArtException() {
        final Intent intent = mock(Intent.class);
        final Bundle ao = new Bundle();
        final AndroidRuntimeException e = new AndroidRuntimeException("Test message");
        doThrow(e).when(mContext).startActivity(intent, ao);
        doReturn(false).when(mFlaggedApiDelegate).isInfeasibleActivityOptionsException(e);

        final AndroidRuntimeException thrown =
                Assert.assertThrows(
                        AndroidRuntimeException.class,
                        () -> PopupCreator.tryStartActivity(mContext, intent, ao));
        Assert.assertEquals(e, thrown);
        verify(mContext).startActivity(intent, ao);
    }

    @Test
    public void testTryStartActivity_genericRuntimeException() {
        final Intent intent = mock(Intent.class);
        final Bundle ao = new Bundle();
        final RuntimeException e = new RuntimeException("Test message");
        doThrow(e).when(mContext).startActivity(intent, ao);

        final RuntimeException thrown =
                Assert.assertThrows(
                        RuntimeException.class,
                        () -> PopupCreator.tryStartActivity(mContext, intent, ao));
        Assert.assertEquals(e, thrown);
        verify(mContext).startActivity(intent, ao);
    }

    /**
     * This ensures that the {@link android.app.Context#startActivity} call that starts a pop-up
     * window is performed on an {@link android.app.Activity} object. See crbug.com/482253723 for
     * more context.
     */
    @Test
    public void testNewActivityHasCorrectSource() {
        final WindowFeatures windowFeatures = new WindowFeatures(12, 34, 56, null);
        PopupCreator.moveTabToNewPopup(mTab, windowFeatures);

        ArgumentCaptor<Context> captor = ArgumentCaptor.forClass(Context.class);
        verify(mReparentingTask).begin(captor.capture(), any(), any(), any());
        final Context sentContext = captor.getValue();

        Assert.assertTrue(
                "The Context passed to ReparentingTask#begin should be an Activity",
                sentContext instanceof Activity);
    }
}
