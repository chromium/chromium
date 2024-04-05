// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.desktop_windowing;

import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;

import android.graphics.Rect;
import android.os.Build;

import androidx.annotation.RequiresApi;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.widget.InsetsRectProvider;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

/** Browser test for {@link AppHeaderCoordinator} */
@RequiresApi(Build.VERSION_CODES.R)
@Restriction(UiRestriction.RESTRICTION_TYPE_TABLET)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Batch(Batch.PER_CLASS)
@RunWith(ChromeJUnit4ClassRunner.class)
public class AppHeaderCoordinatorBrowserTest {

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    private @Mock InsetsRectProvider mInsetsRectProvider;
    private @Captor ArgumentCaptor<InsetsRectProvider.Observer> mInsetsRectObserverCaptor;

    private Rect mWidestUnoccludedRect = new Rect();
    private Rect mWindowRect = new Rect();

    @Before
    public void setup() {
        ToolbarFeatures.setIsTabStripLayoutOptimizationEnabledForTesting(true);
        AppHeaderCoordinator.setInsetsRectProviderForTesting(mInsetsRectProvider);

        doAnswer(args -> mWidestUnoccludedRect).when(mInsetsRectProvider).getWidestUnoccludedRect();
        doAnswer(args -> mWindowRect).when(mInsetsRectProvider).getWindowRect();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.DYNAMIC_TOP_CHROME)
    public void testTabStripHeightChangeForTabStripLayoutOptimization() {
        mActivityTestRule.startMainActivityOnBlankPage();
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        // Configure mock InsetsRectProvider.
        activity.getWindow().getDecorView().getGlobalVisibleRect(mWindowRect);

        int topPadding = 5;
        int leftPadding = 10;
        int rightPadding = 20;
        int tabStripHeight =
                activity.getResources().getDimensionPixelSize(R.dimen.tab_strip_height);
        mWidestUnoccludedRect.set(leftPadding, 0, rightPadding, topPadding + tabStripHeight);

        // Invoke observer to trigger browser controls transition.
        verify(mInsetsRectProvider, atLeastOnce()).addObserver(mInsetsRectObserverCaptor.capture());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (var obs : mInsetsRectObserverCaptor.getAllValues()) {
                        obs.onBoundingRectsUpdated(mWidestUnoccludedRect);
                    }
                });

        int newTabStripHeight = tabStripHeight + topPadding;
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(activity.getToolbarManager(), Matchers.notNullValue());
                    Criteria.checkThat(
                            "Tab strip height is different",
                            activity.getToolbarManager().getTabStripHeightSupplier().get(),
                            Matchers.equalTo(newTabStripHeight));
                });
    }
}
