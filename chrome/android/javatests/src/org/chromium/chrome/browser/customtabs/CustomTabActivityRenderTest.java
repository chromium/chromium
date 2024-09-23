// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.CLOSE_BUTTON_POSITION_END;
import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_CLOSE_BUTTON_POSITION;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.customtabs.CustomTabsTestUtils.createTestBitmap;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.IntentUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedFeatureUtils;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Instrumentation Render tests for default {@link CustomTabActivity} UI. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CustomTabActivityRenderTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParameter =
            Arrays.asList(
                    new ParameterSet().name("HTTPS").value(true),
                    new ParameterSet().name("HTTP").value(false));

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final int PORT_NO = 31415;

    private final boolean mRunWithHttps;
    private String mUrl;
    private Intent mIntent;

    static class CustomTabTopActionIconHelper {
        private static final Bitmap ICON_CREDIT_CARD =
                createVectorDrawableBitmap(R.drawable.ic_credit_card_black, 48, 48);
        private static final Bitmap ICON_EMAIL =
                createVectorDrawableBitmap(R.drawable.ic_email_googblue_36dp, 48, 48);

        public static void addMaxTopActionIconToIntent(Intent intent) {
            ArrayList<Bundle> toolbarItems = new ArrayList<>(2);
            PendingIntent pendingIntent =
                    PendingIntent.getBroadcast(
                            ApplicationProvider.getApplicationContext(),
                            0,
                            new Intent(),
                            IntentUtils.getPendingIntentMutabilityFlag(true));

            toolbarItems.add(
                    CustomTabsIntentTestUtils.makeToolbarItemBundle(
                            ICON_CREDIT_CARD, "Top Action #1", pendingIntent, 1));
            toolbarItems.add(
                    CustomTabsIntentTestUtils.makeToolbarItemBundle(
                            ICON_EMAIL, "Top Action #2", pendingIntent, 2));
            intent.putParcelableArrayListExtra(CustomTabsIntent.EXTRA_TOOLBAR_ITEMS, toolbarItems);
        }
    }

    @Rule
    public final CustomTabActivityTestRule mCustomTabActivityTestRule =
            new CustomTabActivityTestRule();

    @Rule
    public final EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(4)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_CUSTOM_TABS)
                    .build();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tracker mTracker;

    @Before
    public void setUp() {
        mEmbeddedTestServerRule.setServerUsesHttps(mRunWithHttps);
        mEmbeddedTestServerRule.setServerPort(PORT_NO);
        prepareCCTIntent();
        // Disable IPH to prevent the highlight showing in the renders.
        when(mTracker.shouldTriggerHelpUI(anyString())).thenReturn(false);
        TrackerFactory.setTrackerForTests(mTracker);
    }

    public CustomTabActivityRenderTest(boolean runWithHttps) {
        mRunWithHttps = runWithHttps;
    }

    private static Bitmap createVectorDrawableBitmap(
            @DrawableRes int resId, int widthDp, int heightDp) {
        Context context = ApplicationProvider.getApplicationContext();
        Drawable vectorDrawable = AppCompatResources.getDrawable(context, resId);
        Bitmap bitmap = createTestBitmap(widthDp, heightDp);
        Canvas canvas = new Canvas(bitmap);
        float density = context.getResources().getDisplayMetrics().density;
        int widthPx = (int) (density * widthDp);
        int heightPx = (int) (density * heightDp);
        vectorDrawable.setBounds(0, 0, widthPx, heightPx);
        vectorDrawable.draw(canvas);
        return bitmap;
    }

    private void prepareCCTIntent() {
        mUrl = mEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);
        mIntent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), mUrl);
    }

    private void startActivityAndRenderToolbar(String renderTestId) throws IOException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(mIntent);
        View toolbarView = mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        mRenderTestRule.render(toolbarView, renderTestId);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testCCTToolbar() throws IOException {
        startActivityAndRenderToolbar("default_cct_toolbar_with_https_" + mRunWithHttps);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testCCTToolbarWithTitle() throws IOException {
        mIntent.putExtra(
                CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE, CustomTabsIntent.SHOW_PAGE_TITLE);
        startActivityAndRenderToolbar("cct_toolbar_with_title_with_https_" + mRunWithHttps);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.CCT_MINIMIZED})
    public void testCCTToolbarWithMinimizeButton() throws IOException {
        MinimizedFeatureUtils.setDeviceEligibleForMinimizedCustomTabForTesting(true);
        startActivityAndRenderToolbar(
                "default_cct_toolbar_with_https_" + mRunWithHttps + "_minimize_button");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testCCTToolbarWithCustomCloseButton() throws IOException {
        Bitmap closeIcon = createVectorDrawableBitmap(R.drawable.btn_back, 24, 24);
        mIntent.putExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON, closeIcon);
        startActivityAndRenderToolbar(
                "cct_toolbar_with_custom_close_button_and_with_https_" + mRunWithHttps);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testCCTToolbarWithDefaultCloseButtonAndMaxTopActionItems() throws IOException {
        CustomTabTopActionIconHelper.addMaxTopActionIconToIntent(mIntent);
        startActivityAndRenderToolbar(
                "cct_toolbar_with_default_close_button_and_max_top_action_icon_and_with_https_"
                        + mRunWithHttps);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testCCTToolbarWithCustomCloseButtonAndMaxTopActionItems() throws IOException {
        Bitmap closeIcon = createVectorDrawableBitmap(R.drawable.btn_back, 24, 24);
        mIntent.putExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON, closeIcon);
        CustomTabTopActionIconHelper.addMaxTopActionIconToIntent(mIntent);
        startActivityAndRenderToolbar(
                "cct_toolbar_with_custom_close_button_and_max_top_action_icon_and_with_https_"
                        + mRunWithHttps);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testCCTToolbarWithEndCloseButton() throws IOException {
        mIntent.putExtra(EXTRA_CLOSE_BUTTON_POSITION, CLOSE_BUTTON_POSITION_END);

        startActivityAndRenderToolbar("cct_close_button_end_with_https_" + mRunWithHttps);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.CCT_MINIMIZED})
    public void testCCTToolbarWithEndCloseButtonWithMinimizeButton() throws IOException {
        MinimizedFeatureUtils.setDeviceEligibleForMinimizedCustomTabForTesting(true);
        mIntent.putExtra(EXTRA_CLOSE_BUTTON_POSITION, CLOSE_BUTTON_POSITION_END);

        startActivityAndRenderToolbar(
                "cct_close_button_end_with_https_" + mRunWithHttps + "_minimize_button");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testCCTToolbarWithOmnibox() throws IOException {
        // Permit Omnibox for any upcoming intent(s).
        var connection = spy(CustomTabsConnection.getInstance());
        doReturn(true).when(connection).shouldEnableOmniboxForIntent(any());
        CustomTabsConnection.setInstanceForTesting(connection);
        startActivityAndRenderToolbar("cct_omnibox_" + mRunWithHttps);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void custom_color_red() throws IOException {
        Context context = ApplicationProvider.getApplicationContext();
        mIntent =
                CustomTabsIntentTestUtils.createCustomTabIntent(
                        context,
                        mUrl,
                        true,
                        builder -> {
                            builder.setToolbarColor(Color.RED);
                        });

        startActivityAndRenderToolbar("cct_red" + mRunWithHttps);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void custom_color_black() throws IOException {
        Context context = ApplicationProvider.getApplicationContext();
        mIntent =
                CustomTabsIntentTestUtils.createCustomTabIntent(
                        context,
                        mUrl,
                        true,
                        builder -> {
                            builder.setToolbarColor(Color.BLACK);
                        });

        startActivityAndRenderToolbar("cct_black" + mRunWithHttps);
    }
}
