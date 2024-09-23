// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageButton;
import android.widget.RemoteViews;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.night_mode.SystemNightModeMonitor;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

/** Unit test for {@link CustomTabBottomBarDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class CustomTabBottomBarDelegateUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private BrowserControlsSizer mBrowserControlsSizer;
    @Mock private CustomTabNightModeStateController mNightModeStateController;
    @Mock private SystemNightModeMonitor mSystemNightModeMonitor;
    @Mock private CustomTabActivityTabProvider mTabProvider;
    @Mock private CustomTabCompositorContentInitializer mCompositorContentInitializer;
    @Mock private CustomTabBottomBarView mBottomBarView;
    @Mock private View mShadowView;
    @Mock private RemoteViews mRemoteViews;
    @Mock private Intent mIntent;
    @Mock private PendingIntent mRemoteViewsPendingIntent;
    @Mock private ApplicationViewportInsetSupplier mViewportInsetSupplier;
    @Mock private PendingIntent mSwipeUpPendingIntent;
    @Mock private ImageButton mButtonView;

    private Activity mActivity;
    private BrowserServicesIntentDataProvider mIntentDataProvider;
    private CustomTabBottomBarDelegate mBottomBarDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        when(mIntent.getParcelableExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS))
                .thenReturn(mRemoteViews);
        when(mIntent.getIntArrayExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS_VIEW_IDS))
                .thenReturn(new int[] {1});
        when(mIntent.getParcelableExtra(CustomTabsIntent.EXTRA_REMOTEVIEWS_PENDINGINTENT))
                .thenReturn(mRemoteViewsPendingIntent);
        when(mIntent.getParcelableExtra(
                        CustomTabIntentDataProvider.EXTRA_SECONDARY_TOOLBAR_SWIPE_UP_ACTION))
                .thenReturn(mSwipeUpPendingIntent);
        when(mWindowAndroid.getApplicationBottomInsetSupplier()).thenReturn(mViewportInsetSupplier);
        mIntentDataProvider =
                new CustomTabIntentDataProvider(
                        mIntent, mActivity, CustomTabsIntent.COLOR_SCHEME_LIGHT);
        mBottomBarDelegate =
                new CustomTabBottomBarDelegate(
                        mActivity,
                        mWindowAndroid,
                        mIntentDataProvider,
                        mBrowserControlsSizer,
                        mNightModeStateController,
                        mSystemNightModeMonitor,
                        mTabProvider,
                        mCompositorContentInitializer);
        when(mBottomBarView.findViewById(eq(R.id.bottombar_shadow))).thenReturn(mShadowView);
        mBottomBarDelegate.setBottomBarViewForTesting(mBottomBarView);
    }

    @Test
    public void testIsSwipeEnabled() {
        // Swipe should only be enabled when the bottom bar is visible and the direction is up.
        when(mBottomBarView.getVisibility()).thenReturn(View.VISIBLE);
        assertTrue(mBottomBarDelegate.isSwipeEnabled(ScrollDirection.UP));
        assertFalse(mBottomBarDelegate.isSwipeEnabled(ScrollDirection.DOWN));

        when(mBottomBarView.getVisibility()).thenReturn(View.INVISIBLE);
        assertFalse(mBottomBarDelegate.isSwipeEnabled(ScrollDirection.UP));
    }

    @Test
    public void testSendsSwipeIntent() throws CanceledException {
        mBottomBarDelegate.showBottomBarIfNecessary();
        assertEquals(
                mSwipeUpPendingIntent,
                mIntentDataProvider.getSecondaryToolbarSwipeUpPendingIntent());
        // Simulate a swipe up gesture.
        mBottomBarDelegate.onSwipeStarted(
                ScrollDirection.UP, MotionEvent.obtain(0, 10, MotionEvent.ACTION_MOVE, 0f, 10f, 0));
        // Verify the intent is sent.
        verify(mSwipeUpPendingIntent)
                .send(eq(mActivity), anyInt(), any(), any(), any(), any(), any());
    }

    @Test
    public void testSwipeIntentAfterUpdate() throws CanceledException {
        mBottomBarDelegate.showBottomBarIfNecessary();
        var pendingIntent = Mockito.mock(PendingIntent.class);
        mBottomBarDelegate.updateSwipeUpPendingIntent(pendingIntent);
        // Simulate a swipe up gesture.
        mBottomBarDelegate.onSwipeStarted(
                ScrollDirection.UP, MotionEvent.obtain(0, 10, MotionEvent.ACTION_MOVE, 0f, 10f, 0));
        // Verify the intent is sent.
        verify(pendingIntent).send(eq(mActivity), anyInt(), any(), any(), any(), any(), any());
    }

    @Test
    public void testSwipeIntentAfterUpdateToNull() {
        mBottomBarDelegate.showBottomBarIfNecessary();
        mBottomBarDelegate.updateSwipeUpPendingIntent(null);
        // Simulate a swipe up gesture.
        mBottomBarDelegate.onSwipeStarted(
                ScrollDirection.UP, MotionEvent.obtain(0, 10, MotionEvent.ACTION_MOVE, 0f, 10f, 0));
        // No exception should be thrown.
    }

    @Test
    public void testUpdateBottomBarButtons() {
        when(mBottomBarView.findViewById(1)).thenReturn(mButtonView);
        Drawable icon = Mockito.mock(Drawable.class);
        var description = "description";
        CustomButtonParams customButtonParams = Mockito.mock(CustomButtonParams.class);
        when(customButtonParams.getId()).thenReturn(1);
        when(customButtonParams.getIcon(any())).thenReturn(icon);
        when(customButtonParams.getDescription()).thenReturn(description);

        mBottomBarDelegate.updateBottomBarButtons(customButtonParams);

        verify(mButtonView).setImageDrawable(any());
        verify(mButtonView).setContentDescription(eq(description));
    }

    @Test
    public void testUpdateBottomBarButtons_updaterSet_noInteractionsWithButtonView() {
        when(mBottomBarView.findViewById(1)).thenReturn(mButtonView);
        Drawable icon = Mockito.mock(Drawable.class);
        var description = "description";
        CustomButtonParams customButtonParams = Mockito.mock(CustomButtonParams.class);
        when(customButtonParams.getId()).thenReturn(1);
        when(customButtonParams.getIcon(any())).thenReturn(icon);
        when(customButtonParams.getDescription()).thenReturn(description);
        CustomTabBottomBarDelegate.CustomButtonsUpdater updater =
                Mockito.mock(CustomTabBottomBarDelegate.CustomButtonsUpdater.class);
        when(updater.updateBottomBarButton(customButtonParams)).thenReturn(true);
        mBottomBarDelegate.setCustomButtonsUpdater(updater);

        mBottomBarDelegate.updateBottomBarButtons(customButtonParams);

        verify(updater).updateBottomBarButton(eq(customButtonParams));
        verifyNoInteractions(mButtonView);
    }

    @Test
    public void testOnControlsOffsetChanged() {
        when(mBrowserControlsSizer.getBottomControlsMinHeight()).thenReturn(100);

        mBottomBarDelegate.onControlsOffsetChanged(
                /* topOffset= */ 0,
                /* topControlsMinHeightOffset= */ 0,
                /* bottomOffset= */ 0,
                /* bottomControlsMinHeightOffset= */ 0,
                /* needsAnimate= */ false,
                /* isVisibilityForced= */ false);
        verify(mBottomBarView).setTranslationY(-100);
    }

    @Test
    public void testOnBottomControlsHeightChanged() {
        when(mBrowserControlsSizer.getBottomControlsMinHeightOffset()).thenReturn(100);
        when(mBrowserControlsSizer.getBrowserControlHiddenRatio()).thenReturn(1f);
        mBottomBarDelegate.onBottomControlsHeightChanged(
                /* bottomControlsHeight= */ 50, /* bottomControlsMinHeight= */ 0);

        verify(mBottomBarView).setTranslationY(-50); // 1.0 * 50 - 100
    }
}
