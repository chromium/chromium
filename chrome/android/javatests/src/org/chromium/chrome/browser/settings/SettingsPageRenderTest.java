// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewFinder.waitForNoView;
import static org.chromium.base.test.transit.ViewFinder.waitForView;
import static org.chromium.base.test.util.Batch.PER_CLASS;

import android.content.res.Configuration;
import android.view.View;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.base.DeviceFormFactor;

import java.io.IOException;

/** Render tests for {@link SettingsPage} inside a native tab. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
public class SettingsPageRenderTest {
    private static final int RENDER_TEST_REVISION = 1;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_SETTINGS)
                    .build();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @Restriction(DeviceFormFactor.ONLY_TABLET)
    public void testSettingsInTab_tablet() throws IOException {
        var activity = mActivityTestRule.getActivity();
        var resources = activity.getResources();
        // Ensure landscape orientation so settings has the wide two-column layout.
        if (resources.getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT) {
            ActivityTestUtils.rotateActivityToOrientation(
                    activity, Configuration.ORIENTATION_LANDSCAPE);
            // Wait for window layout to complete in landscape.
            CriteriaHelper.pollUiThread(
                    () -> {
                        View decorView = activity.getWindow().getDecorView();
                        return decorView.getWidth() > decorView.getHeight();
                    },
                    "Window should be laid out in landscape orientation.");
        }
        renderMainPage("settings_page_tablet");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @Restriction(DeviceFormFactor.DESKTOP)
    public void testSettingsInTab_desktop() throws IOException {
        renderMainPage("settings_page_desktop");
    }

    private void renderMainPage(String renderId) throws IOException {
        mActivityTestRule.loadUrl("chrome-native://settings/");

        // Verify the settings page loads by checking for a top-level preference item.
        onView(withText(R.string.search_engine_settings)).check(matches(isDisplayed()));

        // Ensure the screen is wide enough to show two-column settings.
        CriteriaHelper.pollUiThread(
                () -> {
                    SettingsHostFragment hostFragment =
                            SettingsHostFragment.get(mActivityTestRule.getActivity());
                    return hostFragment != null && hostFragment.isTwoColumnSettingsVisible();
                },
                "Settings should be shown in two-column mode.");

        // Wait for the default browser promo view to disappear to avoid flakiness due to race
        // conditions.
        waitForNoView(withId(R.id.promo_card_view));

        // Wait for the right column to load the default detail page (Google services).
        waitForView(withText(R.string.allow_chrome_signin_title));

        View contentView = mActivityTestRule.getActivity().findViewById(android.R.id.content);
        View rootView = contentView.getRootView();
        ChromeRenderTestRule.sanitize(rootView);
        mRenderTestRule.render(rootView, renderId);
    }
}
