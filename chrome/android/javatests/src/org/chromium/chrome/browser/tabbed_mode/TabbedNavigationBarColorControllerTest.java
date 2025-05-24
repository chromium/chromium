// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Color;
import android.os.Build;
import android.view.View;
import android.view.Window;

import androidx.annotation.ColorInt;
import androidx.annotation.RequiresApi;
import androidx.core.graphics.ColorUtils;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.TestAnimations.EnableAnimations;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;
import org.chromium.components.browser_ui.edge_to_edge.WindowSystemBarColorHelper;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for {@link TabbedNavigationBarColorController}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.DisableFeatures({ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE})
@MinAndroidSdkLevel(Build.VERSION_CODES.O_MR1)
@RequiresApi(Build.VERSION_CODES.O_MR1)
@SuppressLint("NewApi")
public class TabbedNavigationBarColorControllerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    private static final int ANIMATION_CHECK_INTERVAL_MS = 100;
    private static final int ANIMATION_MAX_TIMEOUT_MS = 2000;

    private Window mWindow;
    private @ColorInt int mRegularNavigationColor;
    private @ColorInt int mDarkNavigationColor;
    private TabbedNavigationBarColorController mTabbedNavigationBarColorController;
    private EdgeToEdgeSystemBarColorHelper mEdgeToEdgeSystemBarColorHelper;
    private WindowSystemBarColorHelper mWindowSystemBarColorHelper;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mWindow = mActivityTestRule.getActivity().getWindow();
        Context context = mActivityTestRule.getActivity();
        mRegularNavigationColor = SemanticColorUtils.getBottomSystemNavColor(context);
        mDarkNavigationColor = context.getColor(R.color.default_bg_color_dark);
        TabbedRootUiCoordinator tabbedRootUiCoordinator =
                (TabbedRootUiCoordinator)
                        mActivityTestRule.getActivity().getRootUiCoordinatorForTesting();
        mTabbedNavigationBarColorController =
                tabbedRootUiCoordinator
                        .getTabbedSystemUiCoordinatorForTesting()
                        .getNavigationBarColorController();
        mEdgeToEdgeSystemBarColorHelper =
                mActivityTestRule
                        .getActivity()
                        .getEdgeToEdgeManager()
                        .getEdgeToEdgeSystemBarColorHelper();
        mWindowSystemBarColorHelper = mEdgeToEdgeSystemBarColorHelper.getWindowHelperForTesting();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN)
    public void testToggleOverview() {
        assertEquals(
                "Navigation bar should match the tab background before entering overview mode.",
                mActivityTestRule.getActivity().getActivityTab().getBackgroundColor(),
                mWindow.getNavigationBarColor());

        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER, false);

        assertEquals(
                "Navigation bar should be colorSurface in overview mode.",
                mRegularNavigationColor,
                mWindow.getNavigationBarColor());

        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING, false);

        assertEquals(
                "Navigation bar should match the tab background after exiting overview mode.",
                mActivityTestRule.getActivity().getActivityTab().getBackgroundColor(),
                mWindow.getNavigationBarColor());
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN)
    public void testToggleIncognito() {
        assertEquals(
                "Navigation bar should match the tab background on normal tabs.",
                mActivityTestRule.getActivity().getActivityTab().getBackgroundColor(),
                mWindow.getNavigationBarColor());

        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                true,
                true);

        assertEquals(
                "Navigation bar should be dark_elev_3 on incognito tabs.",
                mDarkNavigationColor,
                mWindow.getNavigationBarColor());

        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                false,
                true);

        assertEquals(
                "Navigation bar should match the tab background after switching back to normal"
                        + " tab.",
                mActivityTestRule.getActivity().getActivityTab().getBackgroundColor(),
                mWindow.getNavigationBarColor());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1381509")
    @DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN)
    public void testToggleFullscreen() throws TimeoutException {
        assertEquals(
                "Navigation bar should be colorSurface before entering fullscreen mode.",
                mRegularNavigationColor,
                mWindow.getNavigationBarColor());

        String url =
                mTestServerRule.getServer().getURL("/content/test/data/media/video-player.html");
        mActivityTestRule.loadUrl(url);
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        FullscreenToggleObserver observer = new FullscreenToggleObserver();
        ThreadUtils.runOnUiThreadBlocking(
                () -> activity.getFullscreenManager().addObserver(observer));

        enterFullscreen(observer, activity.getCurrentWebContents());

        assertEquals(
                "Navigation bar should be dark in fullscreen mode.",
                mDarkNavigationColor,
                mWindow.getNavigationBarColor());

        exitFullscreen(observer, activity.getCurrentWebContents());

        assertEquals(
                "Navigation bar should be colorSurface after exiting fullscreen mode.",
                mRegularNavigationColor,
                mWindow.getNavigationBarColor());
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN)
    public void testSetNavigationBarScrimFraction() {
        assertEquals(
                "Navigation bar should match the tab background on normal tabs.",
                mActivityTestRule.getActivity().getActivityTab().getBackgroundColor(),
                mWindow.getNavigationBarColor());

        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        View rootView = activity.findViewById(R.id.tab_switcher_view_holder_stub);
        ScrimManager scrimManager = activity.getRootUiCoordinatorForTesting().getScrimManager();

        PropertyModel outerPropertyModel =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            PropertyModel propertyModel =
                                    new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                                            .with(ScrimProperties.ANCHOR_VIEW, rootView)
                                            .with(ScrimProperties.AFFECTS_NAVIGATION_BAR, true)
                                            .build();
                            scrimManager.showScrim(propertyModel);
                            scrimManager.forceAnimationToFinish(propertyModel);
                            return propertyModel;
                        });

        double regularBrightness = ColorUtils.calculateLuminance(mRegularNavigationColor);
        @ColorInt int withScrim = mWindow.getNavigationBarColor();
        assertNotEquals(
                mActivityTestRule.getActivity().getActivityTab().getBackgroundColor(), withScrim);
        assertTrue(regularBrightness > ColorUtils.calculateLuminance(withScrim));

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        scrimManager.hideScrim(
                                outerPropertyModel, /* animate= */ false, /* duration= */ 0));
        assertEquals(
                mActivityTestRule.getActivity().getActivityTab().getBackgroundColor(),
                mWindow.getNavigationBarColor());
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.NAV_BAR_COLOR_ANIMATION,
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN
    })
    @DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE)
    @EnableAnimations
    @Restriction({DeviceFormFactor.PHONE, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    public void testNavBarColorAnimationsEdgeToEdgeBottomChin() throws InterruptedException {
        Assume.assumeTrue(
                "E2E not applicable.",
                EdgeToEdgeControllerFactory.isSupportedConfiguration(
                        mActivityTestRule.getActivity()));
        testNavBarColorAnimations();
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.NAV_BAR_COLOR_ANIMATION,
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE
    })
    @DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN)
    @EnableAnimations
    @Restriction({DeviceFormFactor.PHONE, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    public void testNavBarColorAnimationsEdgeToEdgeEverywhere() throws InterruptedException {
        testNavBarColorAnimations();
    }

    // Disable the dedicated feature flag.
    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN,
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE
    })
    @DisableFeatures(ChromeFeatureList.NAV_BAR_COLOR_ANIMATION)
    public void testNavBarColorAnimationsFeatureFlagDisabled() {
        Assume.assumeTrue(
                "E2E not applicable.",
                EdgeToEdgeControllerFactory.isSupportedConfiguration(
                        mActivityTestRule.getActivity()));
        testNavBarColorAnimationsDisabled();
    }

    // Disable the two cached params.
    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.NAV_BAR_COLOR_ANIMATION
                + ":disable_bottom_chin_color_animation/true/disable_edge_to_edge_layout_color_animation/true"
    })
    public void testNavBarColorAnimationsCachedParamsDisabled() {
        testNavBarColorAnimationsDisabled();
    }

    private void enterFullscreen(FullscreenToggleObserver observer, WebContents webContents)
            throws TimeoutException {
        String video = "video";
        int onEnterCallCount = observer.getOnEnterFullscreenHelper().getCallCount();

        // Start playback to guarantee it's properly loaded.
        assertTrue("Failed to load video", DOMUtils.isMediaPaused(webContents, video));

        // Trigger requestFullscreen() via a click on a button.
        assertTrue(
                "Failed to click fullscreen node", DOMUtils.clickNode(webContents, "fullscreen"));
        observer.getOnEnterFullscreenHelper()
                .waitForCallback("Failed to enter full screen", onEnterCallCount);
    }

    private void exitFullscreen(FullscreenToggleObserver observer, WebContents webContents)
            throws TimeoutException {
        int onExitCallCount = observer.getOnExitFullscreenHelper().getCallCount();
        DOMUtils.exitFullscreen(webContents);
        observer.getOnExitFullscreenHelper()
                .waitForCallback("Failed to exit full screen", onExitCallCount);
    }

    private static class FullscreenToggleObserver implements FullscreenManager.Observer {
        private final CallbackHelper mOnEnterFullscreenHelper;
        private final CallbackHelper mOnExitFullscreenHelper;

        public FullscreenToggleObserver() {
            mOnEnterFullscreenHelper = new CallbackHelper();
            mOnExitFullscreenHelper = new CallbackHelper();
        }

        public CallbackHelper getOnEnterFullscreenHelper() {
            return mOnEnterFullscreenHelper;
        }

        public CallbackHelper getOnExitFullscreenHelper() {
            return mOnExitFullscreenHelper;
        }

        @Override
        public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
            mOnEnterFullscreenHelper.notifyCalled();
        }

        @Override
        public void onExitFullscreen(Tab tab) {
            mOnExitFullscreenHelper.notifyCalled();
        }
    }

    private void testNavBarColorAnimationsDisabled() {
        assert mTabbedNavigationBarColorController != null;

        // Create spies from real instances and inject the spies back.
        EdgeToEdgeSystemBarColorHelper spyEdgeToEdgeSystemBarColorHelper =
                Mockito.spy(mEdgeToEdgeSystemBarColorHelper);

        mTabbedNavigationBarColorController.setEdgeToEdgeSystemBarColorHelperForTesting(
                spyEdgeToEdgeSystemBarColorHelper);

        Mockito.clearInvocations(spyEdgeToEdgeSystemBarColorHelper);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabbedNavigationBarColorController.onBottomAttachedColorChanged(
                            Color.RED, false, false);
                });

        // Verify that setNavigationBarColor is called exactly once with Color.RED since animations
        // are disabled.
        verify(spyEdgeToEdgeSystemBarColorHelper, times(1)).setNavigationBarColor(eq(Color.RED));

        // Since animations are disabled, setNavigationBarColor should not be called with any other
        // colors other than Color.RED.
        verify(spyEdgeToEdgeSystemBarColorHelper, times(1)).setNavigationBarColor(anyInt());
    }

    private void testNavBarColorAnimations() {
        assert mTabbedNavigationBarColorController != null;

        // Create spies from real instances and inject the spies back.
        EdgeToEdgeSystemBarColorHelper spyEdgeToEdgeSystemBarColorHelper =
                Mockito.spy(mEdgeToEdgeSystemBarColorHelper);

        mTabbedNavigationBarColorController.setEdgeToEdgeSystemBarColorHelperForTesting(
                spyEdgeToEdgeSystemBarColorHelper);

        WindowSystemBarColorHelper spyWindowSystemBarColorHelper =
                Mockito.spy(mWindowSystemBarColorHelper);

        // Inject the spy back into spyEdgeToEdgeSystemBarColorHelper.
        spyEdgeToEdgeSystemBarColorHelper.setWindowHelperForTesting(spyWindowSystemBarColorHelper);

        Mockito.clearInvocations(spyEdgeToEdgeSystemBarColorHelper);

        // The initial nav bar color.
        int startColor = Color.BLUE;

        // The target nav bar color after animations.
        int endColor = Color.RED;

        // Trigger the color change on the UI thread.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Set the initial navigation bar color to blue.
                    mTabbedNavigationBarColorController.onBottomAttachedColorChanged(
                            startColor, false, /* disableAnimation= */ true);
                });

        verify(spyEdgeToEdgeSystemBarColorHelper, atLeastOnce())
                .setNavigationBarColor(eq(Color.BLUE));

        // Trigger the color change on the UI thread.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Change navigation color from blue to red with nav bar color animations
                    // enabled.
                    mTabbedNavigationBarColorController.onBottomAttachedColorChanged(
                            endColor, false, /* disableAnimation= */ false);
                });

        // Poll on the UI thread to ensure that the asserts are called only after the animations are
        // fully over.
        CriteriaHelper.pollUiThread(
                () -> {
                    ValueAnimator valueAnimator =
                            mTabbedNavigationBarColorController
                                    .getNavbarColorTransitionAnimationForTesting();

                    return valueAnimator == null || !valueAnimator.isRunning();
                },
                "Nav bar color animations did not finish.",
                ANIMATION_MAX_TIMEOUT_MS,
                ANIMATION_CHECK_INTERVAL_MS);

        ArgumentCaptor<Integer> argumentCaptor = ArgumentCaptor.forClass(Integer.class);

        verify(spyEdgeToEdgeSystemBarColorHelper, atLeastOnce())
                .setNavigationBarColor(argumentCaptor.capture());
        List<Integer> capturedColors = argumentCaptor.getAllValues();

        assertEquals(
                "The first animation color should match the start color.",
                startColor,
                (int) capturedColors.get(0));

        assertEquals(
                "The last animation color should match the end color.",
                endColor,
                (int) capturedColors.get(capturedColors.size() - 1));

        // Ensure that triggering animations don't change the window color.
        verify(spyWindowSystemBarColorHelper, never()).applyNavBarColor();
    }
}
