// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Tests for various Safety Hub settings surfaces. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB)
@Batch(Batch.PER_CLASS)
public final class SafetyHubTest {
    private static final PermissionsData PERMISSIONS_DATA_1 =
            PermissionsData.create(
                    "http://example1.com",
                    new int[] {
                        ContentSettingsType.MEDIASTREAM_CAMERA, ContentSettingsType.MEDIASTREAM_MIC
                    },
                    0,
                    0);

    private static final PermissionsData PERMISSIONS_DATA_2 =
            PermissionsData.create(
                    "http://example2.com",
                    new int[] {
                        ContentSettingsType.MEDIASTREAM_CAMERA, ContentSettingsType.MEDIASTREAM_MIC
                    },
                    0,
                    0);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public SettingsActivityTestRule<SafetyHubPermissionsFragment> mPermissionsFragmentTestRule =
            new SettingsActivityTestRule<>(SafetyHubPermissionsFragment.class);

    @Rule
    public SettingsActivityTestRule<SafetyHubNotificationsFragment> mNotificationsFragmentTestRule =
            new SettingsActivityTestRule<>(SafetyHubNotificationsFragment.class);

    @Rule
    public SettingsActivityTestRule<SafetyHubFragment> mSafetyHubFragmentTestRule =
            new SettingsActivityTestRule<>(SafetyHubFragment.class);

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_SETTINGS_PRIVACY)
                    .setRevision(1)
                    .build();

    private FakeUnusedSitePermissionsBridge mUnusedPermissionsBridge =
            new FakeUnusedSitePermissionsBridge();

    @Before
    public void setUp() {
        mJniMocker.mock(UnusedSitePermissionsBridgeJni.TEST_HOOKS, mUnusedPermissionsBridge);
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testPermissionsSubpageAppearance() throws IOException {
        mUnusedPermissionsBridge.setPermissionsDataForReview(
                new PermissionsData[] {PERMISSIONS_DATA_1, PERMISSIONS_DATA_2});
        SettingsActivity settingsActivity = mPermissionsFragmentTestRule.startSettingsActivity();
        mRenderTestRule.render(
                settingsActivity.findViewById(android.R.id.content).getRootView(),
                "permissions_subpage");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testNotificationsSubpageAppearance() throws IOException {
        SettingsActivity settingsActivity = mPermissionsFragmentTestRule.startSettingsActivity();
        mRenderTestRule.render(
                settingsActivity.findViewById(android.R.id.content).getRootView(),
                "notifications_subpage");
    }

    @Test
    @LargeTest
    public void testPermissionRegrant() {
        mUnusedPermissionsBridge.setPermissionsDataForReview(
                new PermissionsData[] {PERMISSIONS_DATA_1});
        mPermissionsFragmentTestRule.startSettingsActivity();

        // Regrant the permissions by clicking the corresponding action button.
        clickOnButtonNextToText(PERMISSIONS_DATA_1.getOrigin());
        onView(withText(PERMISSIONS_DATA_1.getOrigin())).check(doesNotExist());

        // Click on the action button of the snackbar to undo the above action.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(PERMISSIONS_DATA_1.getOrigin())).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    public void testClearPermissionsReviewList() {
        mUnusedPermissionsBridge.setPermissionsDataForReview(
                new PermissionsData[] {PERMISSIONS_DATA_1, PERMISSIONS_DATA_2});
        mSafetyHubFragmentTestRule.startSettingsActivity();

        // Verify the permissions module is displaying the warning state.
        String permissionsTitle =
                mSafetyHubFragmentTestRule
                        .getActivity()
                        .getResources()
                        .getQuantityString(R.plurals.safety_hub_permissions_warning_title, 2, 2);
        onView(withText(permissionsTitle)).check(matches(isDisplayed()));

        // Open the permissions subpage.
        onView(withText(permissionsTitle)).perform(click());

        // Verify that 2 sites are displayed.
        onView(withText(PERMISSIONS_DATA_1.getOrigin())).check(matches(isDisplayed()));
        onView(withText(PERMISSIONS_DATA_2.getOrigin())).check(matches(isDisplayed()));

        // Click the button at the bottom of the page.
        onView(withText(R.string.got_it)).perform(click());

        // Verify tha the permissions subpage has been dismissed and the state of the permissions
        // module has changed.
        onViewWaiting(withText(R.string.safety_hub_permissions_ok_title))
                .check(matches(isDisplayed()));

        // Click on the snackbar action button and verify that the warning is displayed
        // again.
        onViewWaiting(withText(R.string.undo)).perform(click());
        onViewWaiting(withText(permissionsTitle)).check(matches(isDisplayed()));
    }

    private void clickOnButtonNextToText(String text) {
        onView(allOf(withId(R.id.button), withParent(hasSibling(withChild(withText(text))))))
                .perform(click());
    }
}
