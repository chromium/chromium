// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.chrome.browser.ui.system.StatusBarColorController.UNDEFINED_STATUS_BAR_COLOR;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.browserservices.intents.BitmapHelper;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests for splash screens. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebappSplashScreenTest {
    @Rule public final WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();

    private int getHistogramTotalCountFor(String histogram, int buckets) {
        int count = 0;

        for (int i = 0; i < buckets; ++i) {
            count += RecordHistogram.getHistogramValueCountForTesting(histogram, i);
        }

        return count;
    }

    private boolean hasHistogramEntry(String histogram, int maxValue) {
        for (int i = 0; i < maxValue; ++i) {
            if (RecordHistogram.getHistogramValueCountForTesting(histogram, i) > 0) {
                return true;
            }
        }
        return false;
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testDefaultBackgroundColor() {
        ViewGroup splashScreen = mActivityTestRule.startWebappActivityAndWaitForSplashScreen();
        ColorDrawable background = (ColorDrawable) splashScreen.getBackground();

        Assert.assertEquals(
                mActivityTestRule.getActivity().getColor(R.color.webapp_default_bg),
                background.getColor());
    }

    @Test
    @SmallTest
    @Feature({"StatusBar", "Webapps"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testThemeColorWhenNotSpecified() {
        mActivityTestRule.startWebappActivityAndWaitForSplashScreen();

        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        // UNDEFINED_STATUS_BAR_COLOR signals we're using the tab's theme color.
        Assert.assertEquals(
                UNDEFINED_STATUS_BAR_COLOR,
                mActivityTestRule.getActivity().getBaseStatusBarColor(tab));
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterFirstPaint() {
        mActivityTestRule.startWebappActivityAndWaitForSplashScreen();
        Assert.assertTrue(mActivityTestRule.isSplashScreenVisible());

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () ->
                        TabTestUtils.simulateFirstVisuallyNonEmptyPaint(
                                mActivityTestRule.getActivity().getActivityTab()));

        mActivityTestRule.waitUntilSplashscreenHides();
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterCrash() {
        mActivityTestRule.startWebappActivityAndWaitForSplashScreen();
        Assert.assertTrue(mActivityTestRule.isSplashScreenVisible());

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () ->
                        TabTestUtils.simulateCrash(
                                mActivityTestRule.getActivity().getActivityTab(), true));

        mActivityTestRule.waitUntilSplashscreenHides();
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterLoadCompletes() {
        mActivityTestRule.startWebappActivityAndWaitForSplashScreen();
        Assert.assertTrue(mActivityTestRule.isSplashScreenVisible());

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () ->
                        TabTestUtils.simulatePageLoadFinished(
                                mActivityTestRule.getActivity().getActivityTab()));

        mActivityTestRule.waitUntilSplashscreenHides();
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterLoadFails() {
        mActivityTestRule.startWebappActivityAndWaitForSplashScreen();
        Assert.assertTrue(mActivityTestRule.isSplashScreenVisible());

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () ->
                        TabTestUtils.simulatePageLoadFailed(
                                mActivityTestRule.getActivity().getActivityTab(), 0));

        mActivityTestRule.waitUntilSplashscreenHides();
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testHidesAfterMultipleEvents() {
        mActivityTestRule.startWebappActivityAndWaitForSplashScreen();
        Assert.assertTrue(mActivityTestRule.isSplashScreenVisible());

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    Tab tab = mActivityTestRule.getActivity().getActivityTab();

                    TabTestUtils.simulatePageLoadFinished(tab);
                    TabTestUtils.simulatePageLoadFailed(tab, 0);
                    TabTestUtils.simulateFirstVisuallyNonEmptyPaint(tab);
                });

        mActivityTestRule.waitUntilSplashscreenHides();
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testRegularSplashScreenAppears() throws Exception {
        // Register a properly-sized icon for the splash screen.
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        int thresholdSize =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.webapp_splash_image_size_minimum);
        int size = thresholdSize + 1;
        Bitmap splashBitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888);
        String bitmapString = BitmapHelper.encodeBitmapAsString(splashBitmap);

        TestFetchStorageCallback callback = new TestFetchStorageCallback();
        WebappRegistry.getInstance().register(WebappActivityTestRule.WEBAPP_ID, callback);
        callback.waitForCallback(0);
        callback.getStorage().updateSplashScreenImage(bitmapString);

        ViewGroup splashScreen = mActivityTestRule.startWebappActivityAndWaitForSplashScreen();
        Assert.assertTrue(mActivityTestRule.isSplashScreenVisible());

        ImageView splashImage = splashScreen.findViewById(R.id.webapp_splash_screen_icon);
        Assert.assertEquals(size, splashImage.getMeasuredWidth());
        Assert.assertEquals(size, splashImage.getMeasuredHeight());

        TextView splashText = splashScreen.findViewById(R.id.webapp_splash_screen_name);
        int[] rules = ((RelativeLayout.LayoutParams) splashText.getLayoutParams()).getRules();
        Assert.assertEquals(RelativeLayout.TRUE, rules[RelativeLayout.ALIGN_PARENT_BOTTOM]);
        Assert.assertEquals(0, rules[RelativeLayout.BELOW]);
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testSplashScreenWithoutImageAppears() throws Exception {
        // Register an image that's too small for the splash screen.
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        int size =
                context.getResources()
                                .getDimensionPixelSize(R.dimen.webapp_splash_image_size_minimum)
                        - 1;
        Bitmap splashBitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888);
        String bitmapString = BitmapHelper.encodeBitmapAsString(splashBitmap);

        TestFetchStorageCallback callback = new TestFetchStorageCallback();
        WebappRegistry.getInstance().register(WebappActivityTestRule.WEBAPP_ID, callback);
        callback.waitForCallback(0);
        callback.getStorage().updateSplashScreenImage(bitmapString);

        ViewGroup splashScreen =
                mActivityTestRule.startWebappActivityAndWaitForSplashScreen(
                        mActivityTestRule
                                .createIntent()
                                .putExtra(WebappConstants.EXTRA_IS_ICON_GENERATED, true));
        Assert.assertTrue(mActivityTestRule.isSplashScreenVisible());

        // There's no icon displayed.
        ImageView splashImage = splashScreen.findViewById(R.id.webapp_splash_screen_icon);
        Assert.assertNull(splashImage);

        View spacer = splashScreen.findViewById(R.id.webapp_splash_space);
        Assert.assertNotNull(spacer);

        // The web app name is anchored to the top of the spacer.
        TextView splashText = splashScreen.findViewById(R.id.webapp_splash_screen_name);
        int[] rules = ((RelativeLayout.LayoutParams) splashText.getLayoutParams()).getRules();
        Assert.assertEquals(0, rules[RelativeLayout.ALIGN_PARENT_BOTTOM]);
        Assert.assertEquals(0, rules[RelativeLayout.BELOW]);
        Assert.assertEquals(R.id.webapp_splash_space, rules[RelativeLayout.ABOVE]);
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testSplashScreenAppearsWithoutRegisteredSplashImage() {
        // Don't register anything for the web app, which represents apps that were added to the
        // home screen before splash screen images were downloaded.
        ViewGroup splashScreen = mActivityTestRule.startWebappActivityAndWaitForSplashScreen();
        Assert.assertTrue(mActivityTestRule.isSplashScreenVisible());

        // There's no icon displayed.
        ImageView splashImage = splashScreen.findViewById(R.id.webapp_splash_screen_icon);
        Assert.assertNull(splashImage);

        View spacer = splashScreen.findViewById(R.id.webapp_splash_space);
        Assert.assertNotNull(spacer);

        // The web app name is anchored to the top of the spacer.
        TextView splashText = splashScreen.findViewById(R.id.webapp_splash_screen_name);
        int[] rules = ((RelativeLayout.LayoutParams) splashText.getLayoutParams()).getRules();
        Assert.assertEquals(0, rules[RelativeLayout.ALIGN_PARENT_BOTTOM]);
        Assert.assertEquals(0, rules[RelativeLayout.BELOW]);
        Assert.assertEquals(0, rules[RelativeLayout.CENTER_IN_PARENT]);
        Assert.assertEquals(R.id.webapp_splash_space, rules[RelativeLayout.ABOVE]);
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testSplashScreenWithSynchronousLayoutInflation() {
        WebappActivity.setOverrideCoreCountForTesting(2);

        mActivityTestRule.startWebappActivityAndWaitForSplashScreen();
        Assert.assertTrue(mActivityTestRule.isSplashScreenVisible());
        Assert.assertTrue(mActivityTestRule.getActivity().isInitialLayoutInflationComplete());
    }
}
