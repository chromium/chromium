// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertEquals;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.fragment.app.Fragment;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.about_settings.AboutChromeSettings;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.util.AttrUtils;

import java.util.Locale;

/** Tests for the Settings menu. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Tests cannot run batched because they launch a Settings activity.")
@DisableFeatures({
    ChromeFeatureList.SETTINGS_IN_TAB, // crbug.com/521895796
    ChromeFeatureList.SETTINGS_IN_TAB_DESKTOP // crbug.com/556881398
})
public class SettingsActivityTest {
    @Rule
    public SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(MainSettings.class);

    @After
    public void tearDown() {
        LocalizationUtils.setRtlForTesting(false);
        if (mSettingsActivityTestRule.getActivity() != null) {
            ActivityTestUtils.clearActivityOrientation(mSettingsActivityTestRule.getActivity());
            mSettingsActivityTestRule.getActivity().finish();
        }
    }

    /** Test status bar is always black in Automotive devices. */
    @Test
    @SmallTest
    @Feature({"StatusBar, Automotive Toolbar"})
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    public void testStatusBarBlackInAutomotive() {
        mSettingsActivityTestRule.startSettingsActivity();
        assertEquals(
                "Status bar should always be black in automotive devices.",
                Color.BLACK,
                mSettingsActivityTestRule.getActivity().getWindow().getStatusBarColor());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "TODO(crbug.com/389790022)")
    public void testEdgeToEdgeEverywhere() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        final @ColorInt int defaultBgColor = SemanticColorUtils.getDefaultBgColor(activity);
        final int defaultStatusBarColor =
                AttrUtils.resolveColor(activity.getTheme(), android.R.attr.statusBarColor);

        assertEquals(
                defaultBgColor,
                activity.ensureEdgeToEdgeLayoutCoordinator().getNavigationBarColor());
        assertEquals(Color.TRANSPARENT, activity.getWindow().getNavigationBarColor());
        assertEquals(
                defaultStatusBarColor,
                activity.getEdgeToEdgeManager()
                        .getEdgeToEdgeSystemBarColorHelper()
                        .getStatusBarColor());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.SETTINGS_SINGLE_ACTIVITY})
    public void testStandaloneFragments() {
        // Start the main settings, which is an embeddable fragment.
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Open an embeddable fragment. This does NOT start a new activity.
        final Intent intent1 =
                SettingsIntentUtil.createIntent(
                        activity, AboutChromeSettings.class.getName(), null);
        activity.startActivity(intent1);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getMainFragment(),
                            Matchers.instanceOf(AboutChromeSettings.class));
                });

        // Open a standalone fragment. This will create a new activity.
        final Intent intent2 =
                SettingsIntentUtil.createIntent(activity, TestFragment.class.getName(), null);
        ApplicationTestUtils.waitForActivityWithClass(
                SettingsActivity.class, Stage.CREATED, () -> activity.startActivity(intent2));

        // Open an embeddable fragment. This starts a new activity as the last fragment is
        // standalone.
        final Intent intent3 =
                SettingsIntentUtil.createIntent(activity, MainSettings.class.getName(), null);
        ApplicationTestUtils.waitForActivityWithClass(
                SettingsActivity.class, Stage.CREATED, () -> activity.startActivity(intent3));
    }

    /** Regression test for crash. https://crbug.com/535398041 */
    @Test
    @SmallTest
    public void testClickSearchDoesNotCrash() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Search UI creation is asynchronous, so wait for the search box to be inflated.
        CriteriaHelper.pollUiThread(() -> activity.findViewById(R.id.search_box) != null);

        onView(withId(R.id.search_box)).perform(click());
    }

    /** Regression test for https://crbug.com/548848118. */
    @Test
    @MediumTest
    @Restriction({
        DeviceFormFactor.ONLY_TABLET,
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    })
    public void testSearchBoxAlignmentInPortrait_Rtl() {
        LocalizationUtils.setRtlForTesting(true);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        ActivityTestUtils.rotateActivityToOrientation(activity, Configuration.ORIENTATION_PORTRAIT);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Configuration config =
                            new Configuration(activity.getResources().getConfiguration());
                    config.setLayoutDirection(new Locale("ar"));
                    activity.getResources()
                            .updateConfiguration(
                                    config, activity.getResources().getDisplayMetrics());
                    activity.getWindow()
                            .getDecorView()
                            .setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
                });

        onViewWaiting(withId(R.id.search_box)).check(matches(isDisplayed()));

        Rect searchBoxBounds = getViewScreenBounds(R.id.search_box);
        Rect searchIconBounds = getViewScreenBounds(R.id.search_icon);

        onViewWaiting(withId(R.id.search_box)).perform(click());
        onViewWaiting(withId(R.id.search_query_container)).check(matches(isDisplayed()));

        Rect queryBounds = getViewScreenBounds(R.id.search_query_container);
        Rect backArrowBounds = getViewScreenBounds(R.id.back_arrow_icon);

        assertEquals(
                "Search query container should match search box width in RTL",
                searchBoxBounds.width(),
                queryBounds.width());
        assertEquals(
                "Search query container should align horizontally with search box in RTL (left)",
                searchBoxBounds.left,
                queryBounds.left);
        assertEquals(
                "Search query container should align horizontally with search box in RTL (right)",
                searchBoxBounds.right,
                queryBounds.right);
        assertEquals(
                "Back arrow icon should horizontally align with search icon in RTL",
                searchIconBounds.left,
                backArrowBounds.left);

        // Change orientation to landscape and then back to portrait.
        ActivityTestUtils.rotateActivityToOrientation(
                activity, Configuration.ORIENTATION_LANDSCAPE);

        ActivityTestUtils.rotateActivityToOrientation(activity, Configuration.ORIENTATION_PORTRAIT);
        onViewWaiting(withId(R.id.search_box)).check(matches(isDisplayed()));

        Rect searchBoxBoundsAfterRotate = getViewScreenBounds(R.id.search_box);
        Rect searchIconBoundsAfterRotate = getViewScreenBounds(R.id.search_icon);

        assertEquals(
                "Search box should match initial width after rotating back in RTL",
                searchBoxBounds.width(),
                searchBoxBoundsAfterRotate.width());
        assertEquals(
                "Search box should align horizontally after rotating back in RTL (left)",
                searchBoxBounds.left,
                searchBoxBoundsAfterRotate.left);
        assertEquals(
                "Search box should align horizontally after rotating back in RTL (right)",
                searchBoxBounds.right,
                searchBoxBoundsAfterRotate.right);
        assertEquals(
                "Search icon should align horizontally after rotating back in RTL",
                searchIconBounds.left,
                searchIconBoundsAfterRotate.left);
    }

    private Rect getViewScreenBounds(int viewId) {
        Rect bounds = new Rect();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var activity = mSettingsActivityTestRule.getActivity();
                    View view = activity.findViewById(viewId);
                    int[] location = new int[2];
                    view.getLocationOnScreen(location);
                    bounds.set(
                            location[0],
                            location[1],
                            location[0] + view.getWidth(),
                            location[1] + view.getHeight());
                });
        return bounds;
    }

    public static class TestFragment extends Fragment implements SettingsFragment {
        @Override
        public @AnimationType int getAnimationType() {
            return AnimationType.PROPERTY;
        }
    }
}
