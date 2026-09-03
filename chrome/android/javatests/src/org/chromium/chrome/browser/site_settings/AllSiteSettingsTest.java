// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.CoreMatchers.containsString;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.graphics.drawable.Drawable;
import android.graphics.drawable.RippleDrawable;
import android.view.View;

import androidx.preference.PreferenceFragmentCompat;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.widget.containment.ContainerStyle;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemDecoration;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.util.concurrent.TimeoutException;

@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(AllSiteSettingsTest.TEST_BATCH_NAME)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
// TODO(https://crbug.com/464015936): these tests could be flaky because of AnimatedProgressBar.
@DisableFeatures({
    ChromeFeatureList.SETTINGS_MULTI_COLUMN,
    ChromeFeatureList.ANDROID_ANIMATED_PROGRESS_BAR_IN_BROWSER
})
public class AllSiteSettingsTest {
    public static final String TEST_BATCH_NAME = "AllSiteSettingsTest";
    private static final String A_GITHUB_IO = "a.github.io";
    private static final String B_GITHUB_IO = "b.github.io";
    private static final int RENDER_TEST_REVISION = 1;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setBugComponent(Component.UI_BROWSER_MOBILE_SETTINGS)
                    .build();

    @ClassRule
    public static final AutoResetCtaTransitTestRule sActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @ClassRule
    public static final PermissionTestRule sPermissionTestRule =
            new PermissionTestRule(sActivityTestRule.getActivityTestRule(), true);

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule.getActivityTestRule(), false);

    private static BrowserContextHandle getBrowserContextHandle() {
        return ProfileManager.getLastUsedRegularProfile();
    }

    @Before
    public void setUp() throws TimeoutException {
        SiteSettingsTestUtils.cleanUpCookiesAndPermissions();
    }

    @Test
    @SmallTest
    @Feature({"Preferences", "RenderTest"})
    public void testAllSitesViewEmpty() throws Exception {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startAllSitesSettings(SiteSettingsCategory.Type.ALL_SITES);
        onViewWaiting(withText(containsString("Delete browsing"))).check(matches(isDisplayed()));
        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            PreferenceFragmentCompat preferenceFragment =
                                    (PreferenceFragmentCompat) settingsActivity.getMainFragment();
                            return preferenceFragment.getView();
                        });
        ChromeRenderTestRule.sanitize(view);
        mRenderTestRule.render(view, "site_settings_all_sites_empty");
        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences", "RenderTest"})
    public void testAllSitesViewSingleDomain() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            "google.com",
                            "*",
                            ContentSetting.ALLOW);
                });

        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startAllSitesSettings(SiteSettingsCategory.Type.ALL_SITES);
        onViewWaiting(withText(containsString("Delete browsing"))).check(matches(isDisplayed()));
        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            PreferenceFragmentCompat preferenceFragment =
                                    (PreferenceFragmentCompat) settingsActivity.getMainFragment();
                            return preferenceFragment.getView();
                        });
        ChromeRenderTestRule.sanitize(view);
        mRenderTestRule.render(view, "site_settings_all_sites_single_domain");
        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllSitesUsePublicSuffixList() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            A_GITHUB_IO,
                            "*",
                            ContentSetting.ALLOW);
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            B_GITHUB_IO,
                            "*",
                            ContentSetting.ALLOW);
                });

        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startAllSitesSettings(SiteSettingsCategory.Type.ALL_SITES);
        onViewWaiting(withText(containsString("Delete browsing"))).check(matches(isDisplayed()));
        onView(withText(A_GITHUB_IO)).check(matches(isDisplayed()));
        onView(withText(B_GITHUB_IO)).check(matches(isDisplayed()));

        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllSitesContainmentBackgroundOnInitialLoad() throws Exception {
        // Pre-populate cookie permissions for test origins before opening the settings screen
        // to simulate a user visiting sites before inspecting All Sites settings.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            A_GITHUB_IO,
                            "*",
                            ContentSetting.ALLOW);
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            getBrowserContextHandle(),
                            ContentSettingsType.COOKIES,
                            B_GITHUB_IO,
                            "*",
                            ContentSetting.ALLOW);
                });

        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startAllSitesSettings(SiteSettingsCategory.Type.ALL_SITES);
        onViewWaiting(withText(containsString("Delete browsing"))).check(matches(isDisplayed()));
        onView(withText(A_GITHUB_IO)).check(matches(isDisplayed()));
        onView(withText(B_GITHUB_IO)).check(matches(isDisplayed()));

        // Verify that the RecyclerView has ContainmentItemDecoration attached and that all
        // populated child preference views have a valid container style and card background
        // drawable applied. Asynchronous preference loading must synchronize with the layout
        // pass so child views are not left with unstyled or default transparent backgrounds.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PreferenceFragmentCompat preferenceFragment =
                            (PreferenceFragmentCompat) settingsActivity.getMainFragment();
                    RecyclerView listView = preferenceFragment.getListView();
                    assertThat(listView).isNotNull();

                    ContainmentItemDecoration containmentDecoration = null;
                    for (int i = 0; i < listView.getItemDecorationCount(); i++) {
                        if (listView.getItemDecorationAt(i)
                                instanceof ContainmentItemDecoration itemDecoration) {
                            containmentDecoration = itemDecoration;
                            break;
                        }
                    }
                    assertThat(containmentDecoration).isNotNull();

                    assertThat(listView.getChildCount()).isGreaterThan(0);
                    for (int i = 0; i < listView.getChildCount(); i++) {
                        View child = listView.getChildAt(i);
                        int adapterPos = listView.getChildAdapterPosition(child);
                        assertThat(adapterPos).isNotEqualTo(RecyclerView.NO_POSITION);

                        ContainerStyle style = containmentDecoration.getContainerStyle(adapterPos);
                        assertThat(style).isNotNull();
                        assertThat(style).isNotEqualTo(ContainerStyle.EMPTY);

                        Drawable background = child.getBackground();
                        assertThat(background).isNotNull();
                        assertThat(background).isInstanceOf(RippleDrawable.class);
                    }
                });

        settingsActivity.finish();
    }
}
