// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.preferences.ChromeImageViewPreference;
import org.chromium.chrome.browser.preferences.ExpandablePreferenceGroup;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.website.SingleCategoryPreferences;
import org.chromium.chrome.browser.preferences.website.SingleWebsitePreferences;
import org.chromium.chrome.browser.preferences.website.SiteSettingsCategory;
import org.chromium.chrome.browser.preferences.website.SiteSettingsTestUtils;
import org.chromium.chrome.browser.preferences.website.Website;
import org.chromium.chrome.browser.preferences.website.WebsiteAddress;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for TrustedWebActivity functionality under Settings > Site Settings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
})
public class TrustedWebActivityPreferencesUiTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private String mPackage;
    private TrustedWebActivityPermissionManager mPermissionMananger;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        mPackage = InstrumentationRegistry.getTargetContext().getPackageName();
        mPermissionMananger = ChromeApplication.getComponent().resolveTwaPermissionManager();
    }

    /**
     * Tests that the 'Managed by' section appears correctly and that it contains our registered
     * website.
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSingleCategoryManagedBy() throws Exception {
        final String site = "http://example.com";
        final Origin origin = Origin.create(site);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPermissionMananger.register(origin, mPackage, true));

        Preferences preferenceActivity = SiteSettingsTestUtils.startSiteSettingsCategory(
                SiteSettingsCategory.Type.NOTIFICATIONS);
        final String groupName = "managed_group";

        final SingleCategoryPreferences websitePreferences =
                TestThreadUtils.runOnUiThreadBlocking(() -> {
                    final SingleCategoryPreferences preferences =
                            (SingleCategoryPreferences) preferenceActivity.getMainFragment();
                    final ExpandablePreferenceGroup group =
                            (ExpandablePreferenceGroup) preferences.findPreference(groupName);
                    preferences.onPreferenceClick(group);
                    return preferences;
                });

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                // The preference group gets recreated in onPreferenceClick, so we need to find it
                // again.
                final ExpandablePreferenceGroup group =
                        (ExpandablePreferenceGroup) websitePreferences.findPreference(groupName);
                return group.isExpanded();
            }
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final ExpandablePreferenceGroup group =
                    (ExpandablePreferenceGroup) websitePreferences.findPreference(groupName);
            Assert.assertEquals(1, group.getPreferenceCount());
            android.support.v7.preference.Preference preference = group.getPreference(0);
            CharSequence title = preference.getTitle();
            Assert.assertEquals("example.com", title.toString());
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> mPermissionMananger.unregister(origin));

        preferenceActivity.finish();
    }

    /**
     * Tests that registered sites show 'Managed by' in the title when viewing the details for a
     * single website.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testWebsitePreferencesManagedBy() {
        final String site = "http://example.com";
        final Origin origin = Origin.create(site);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mPermissionMananger.register(origin, mPackage, true));

        WebsiteAddress address = WebsiteAddress.create(site);
        Website website = new Website(address, address);
        final Preferences preferenceActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(website);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final SingleWebsitePreferences websitePreferences =
                    (SingleWebsitePreferences) preferenceActivity.getMainFragment();
            final ChromeImageViewPreference notificationPreference =
                    (ChromeImageViewPreference) websitePreferences.findPreference(
                            "push_notifications_list");
            CharSequence summary = notificationPreference.getSummary();
            Assert.assertTrue(summary.toString().startsWith("Managed by "));
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> mPermissionMananger.unregister(origin));

        preferenceActivity.finish();
    }
}
