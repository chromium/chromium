// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import androidx.preference.Preference;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.site_settings.SiteSettingsTestUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;
import org.chromium.components.browser_ui.site_settings.ContentSettingException;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.ProviderType;
import org.chromium.components.embedder_support.util.Origin;

import java.util.Arrays;
import java.util.List;

/** Tests for TrustedWebActivity functionality under Settings > Site Settings. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
})
@DisableFeatures({
    ChromeFeatureList.SETTINGS_IN_TAB, // crbug.com/521895796
    ChromeFeatureList.SETTINGS_IN_TAB_DESKTOP // crbug.com/556881398
})
@Batch(Batch.PER_CLASS)
public class TrustedWebActivityPreferencesUiTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private String mPackage;
    private String mSite;
    private Origin mOrigin;
    private SettingsActivity mSettingsActivity;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startOnBlankPage();

        mPackage = ApplicationProvider.getApplicationContext().getPackageName();
        mSite = "http://example.com";
        mOrigin = Origin.create(mSite);
    }

    @After
    public void tearDown() throws Exception {
        if (mOrigin != null) {
            runOnUiThreadBlocking(() -> InstalledWebappPermissionManager.unregister(mOrigin));
        }
        if (mSettingsActivity != null) {
            ApplicationTestUtils.finishActivity(mSettingsActivity);
            mSettingsActivity = null;
        }
    }

    /**
     * Tests that the 'Managed by' section appears correctly and that it contains our registered
     * website.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSingleCategoryManagedBy() throws Exception {
        runOnUiThreadBlocking(
                () ->
                        InstalledWebappPermissionManager.updatePermission(
                                mOrigin,
                                mPackage,
                                ContentSettingsType.NOTIFICATIONS,
                                ContentSetting.ALLOW));

        mSettingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.NOTIFICATIONS);
        final String groupName = "managed_group";

        CriteriaHelper.pollUiThread(
                () -> {
                    final SingleCategorySettings preferences =
                            (SingleCategorySettings) mSettingsActivity.getMainFragment();
                    final ExpandablePreferenceGroup group =
                            (ExpandablePreferenceGroup) preferences.findPreference(groupName);
                    return group.isExpanded();
                });

        runOnUiThreadBlocking(
                () -> {
                    final SingleCategorySettings preferences =
                            (SingleCategorySettings) mSettingsActivity.getMainFragment();
                    final ExpandablePreferenceGroup group =
                            (ExpandablePreferenceGroup) preferences.findPreference(groupName);
                    Assert.assertEquals(1, group.getPreferenceCount());
                    Preference preference = group.getPreference(0);
                    CharSequence title = preference.getTitle();
                    Assert.assertEquals("example.com", title.toString());
                });
    }

    public static class ContentSettingParam implements ParameterProvider {
        private static final List<ParameterSet> sMethodParam =
                Arrays.asList(
                        new ParameterSet().value(ContentSetting.ALLOW).name("allow"),
                        new ParameterSet().value(ContentSetting.ASK).name("ask"),
                        new ParameterSet().value(ContentSetting.BLOCK).name("block"));

        @Override
        public List<ParameterSet> getParameters() {
            return sMethodParam;
        }
    }

    /**
     * Tests that the 'Managed by' is displayed when notifications are allowed or blocked, or when
     * hasRequestedNotificationsPermission is true. A Subscribe button is not displayed for TWAs.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @UseMethodParameter(ContentSettingParam.class)
    public void testWebsitePreferencesManagedBy_SubscribeButtonNotDisplayed(
            @ContentSetting int contentSetting) {
        runOnUiThreadBlocking(
                () ->
                        InstalledWebappPermissionManager.updatePermission(
                                mOrigin,
                                mPackage,
                                ContentSettingsType.NOTIFICATIONS,
                                contentSetting));

        Website website = createWebsiteWithContentSettingException(contentSetting);

        mSettingsActivity = SiteSettingsTestUtils.startSingleWebsitePreferences(website);

        runOnUiThreadBlocking(
                () -> {
                    // Simulate the Page Info prompt flow.
                    final SingleWebsiteSettings websitePreferences =
                            (SingleWebsiteSettings) mSettingsActivity.getMainFragment();
                    websitePreferences.setHasRequestedNotificationsPermission(
                            contentSetting == ContentSetting.ASK);
                    websitePreferences.refreshSitePermissions();
                    final Preference preference =
                            websitePreferences.findPreference(
                                    SingleWebsiteSettings.getPreferenceKey(
                                            ContentSettingsType.NOTIFICATIONS));
                    Assert.assertNotNull("Notification Preference not found.", preference);
                    Assert.assertTrue(
                            "Preference should be ChromeImageViewPreference",
                            preference instanceof ChromeImageViewPreference);
                    CharSequence summary = preference.getSummary();
                    Assert.assertTrue(summary.toString().startsWith("Managed by "));
                });
        onView(withText(R.string.notifications_permission_subscribe)).check(doesNotExist());
    }

    /**
     * Tests that no entry is displayed in the ASK state if hasRequestedNotificationsPermission is
     * false.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testWebsitePreferencesManagedBy_NotDisplayedOnAsk() {
        runOnUiThreadBlocking(
                () ->
                        InstalledWebappPermissionManager.updatePermission(
                                mOrigin,
                                mPackage,
                                ContentSettingsType.NOTIFICATIONS,
                                ContentSetting.ASK));

        Website website = createWebsiteWithContentSettingException(ContentSetting.ASK);

        mSettingsActivity = SiteSettingsTestUtils.startSingleWebsitePreferences(website);

        runOnUiThreadBlocking(
                () -> {
                    final SingleWebsiteSettings websitePreferences =
                            (SingleWebsiteSettings) mSettingsActivity.getMainFragment();
                    final Preference preference =
                            websitePreferences.findPreference(
                                    SingleWebsiteSettings.getPreferenceKey(
                                            ContentSettingsType.NOTIFICATIONS));
                    Assert.assertNull("Notification Preference found.", preference);
                });
    }

    private Website createWebsiteWithContentSettingException(@ContentSetting int value) {
        WebsiteAddress address = WebsiteAddress.create(mSite);
        Website website = new Website(address, address);
        website.setContentSettingException(
                ContentSettingsType.NOTIFICATIONS,
                new ContentSettingException(
                        ContentSettingsType.NOTIFICATIONS,
                        website.getAddress().getOrigin(),
                        value,
                        ProviderType.PREF_PROVIDER,
                        /* isEmbargoed= */ false));
        return website;
    }
}
