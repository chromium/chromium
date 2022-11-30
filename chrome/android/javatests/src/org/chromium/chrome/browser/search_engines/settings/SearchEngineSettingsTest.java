// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.LoadListener;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Tests for Search Engine Settings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class SearchEngineSettingsTest {
    private final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private final SettingsActivityTestRule<SearchEngineSettings> mSearchEngineSettingsTestRule =
            new SettingsActivityTestRule<>(SearchEngineSettings.class);

    private final SettingsActivityTestRule<MainSettings> mMainSettingsTestRule =
            new SettingsActivityTestRule<>(MainSettings.class);

    // We need to destroy the SettingsActivity before tearing down the mock sign-in environment
    // setup in ChromeBrowserTestRule to avoid code crash.
    @Rule
    public final RuleChain mRuleChain = RuleChain.outerRule(mBrowserTestRule)
                                                .around(mMainSettingsTestRule)
                                                .around(mSearchEngineSettingsTestRule);

    /**
     * Change search engine and make sure it works correctly.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableIf.Build(hardware_is = "sprout", message = "crashes on android-one: crbug.com/540720")
    public void testSearchEnginePreference() throws Exception {
        ensureTemplateUrlServiceLoaded();

        mSearchEngineSettingsTestRule.startSettingsActivity();

        // Set the second search engine as the default using TemplateUrlService.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SearchEngineSettings pref = mSearchEngineSettingsTestRule.getFragment();
            pref.setValueForTesting("1");

            // Ensure that the second search engine in the list is selected.
            Assert.assertNotNull(pref);
            Assert.assertEquals("1", pref.getValueForTesting());

            // Simulate selecting the third search engine, ensure that TemplateUrlService is
            // updated.
            String keyword2 = pref.setValueForTesting("2");
            TemplateUrlService templateUrlService = TemplateUrlServiceFactory.get();
            Assert.assertEquals(
                    keyword2, templateUrlService.getDefaultSearchEngineTemplateUrl().getKeyword());

            // Simulate selecting the fourth search engine.
            String keyword3 = pref.getKeywordFromIndexForTesting(3);
            String url = templateUrlService.getSearchEngineUrlFromTemplateUrl(keyword3);
            keyword3 = pref.setValueForTesting("3");
            Assert.assertEquals(keyword3,
                    TemplateUrlServiceFactory.get()
                            .getDefaultSearchEngineTemplateUrl()
                            .getKeyword());
        });
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({ @Policies.Item(key = "DefaultSearchProviderEnabled", string = "false") })
    public void testSearchEnginePreference_DisabledIfNoDefaultSearchEngine() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ChromeBrowserInitializer.getInstance().handleSynchronousStartup(); });

        ensureTemplateUrlServiceLoaded();
        CriteriaHelper.pollUiThread(() -> TemplateUrlServiceFactory.get().isDefaultSearchManaged());

        mMainSettingsTestRule.startSettingsActivity();

        final MainSettings mainSettings = mMainSettingsTestRule.getFragment();

        final Preference searchEnginePref =
                waitForPreference(mainSettings, MainSettings.PREF_SEARCH_ENGINE);

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(searchEnginePref.getFragment(), Matchers.nullValue());
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ManagedPreferenceDelegate managedPrefDelegate =
                    mainSettings.getManagedPreferenceDelegateForTest();
            Assert.assertTrue(managedPrefDelegate.isPreferenceControlledByPolicy(searchEnginePref));
        });
    }

    /**
     * Make sure that when a user switches to a search engine that uses HTTP, the location
     * permission is not added.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisabledTest(message = "crbug.com/540706")
    @DisableIf.Build(hardware_is = "sprout", message = "fails on android-one: crbug.com/540706")
    public void testSearchEnginePreferenceHttp() throws Exception {
        ensureTemplateUrlServiceLoaded();

        mSearchEngineSettingsTestRule.startSettingsActivity();

        // Set the first search engine as the default using TemplateUrlService.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SearchEngineSettings pref = mSearchEngineSettingsTestRule.getFragment();
            pref.setValueForTesting("0");
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Ensure that the first search engine in the list is selected.
            SearchEngineSettings pref = mSearchEngineSettingsTestRule.getFragment();
            Assert.assertNotNull(pref);
            Assert.assertEquals("0", pref.getValueForTesting());

            // Simulate selecting a search engine that uses HTTP.
            int index = indexOfFirstHttpSearchEngine(pref);
            String keyword = pref.setValueForTesting(Integer.toString(index));

            TemplateUrlService templateUrlService = TemplateUrlServiceFactory.get();
            Assert.assertEquals(
                    keyword, templateUrlService.getDefaultSearchEngineTemplateUrl().getKeyword());
        });
    }

    private int indexOfFirstHttpSearchEngine(SearchEngineSettings pref) {
        TemplateUrlService templateUrlService = TemplateUrlServiceFactory.get();
        List<TemplateUrl> urls = templateUrlService.getTemplateUrls();
        int index;
        for (index = 0; index < urls.size(); ++index) {
            String keyword = pref.getKeywordFromIndexForTesting(index);
            String url = templateUrlService.getSearchEngineUrlFromTemplateUrl(keyword);
            if (url.startsWith("http:")) {
                return index;
            }
        }
        Assert.fail();
        return index;
    }

    private void ensureTemplateUrlServiceLoaded() throws Exception {
        // Make sure the template_url_service is loaded.
        final CallbackHelper onTemplateUrlServiceLoadedHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            if (TemplateUrlServiceFactory.get().isLoaded()) {
                onTemplateUrlServiceLoadedHelper.notifyCalled();
            } else {
                TemplateUrlServiceFactory.get().registerLoadListener(new LoadListener() {
                    @Override
                    public void onTemplateUrlServiceLoaded() {
                        onTemplateUrlServiceLoadedHelper.notifyCalled();
                    }
                });
                TemplateUrlServiceFactory.get().load();
            }
        });
        onTemplateUrlServiceLoadedHelper.waitForCallback(0);
    }

    private static Preference waitForPreference(final PreferenceFragmentCompat prefFragment,
            final String preferenceKey) throws ExecutionException {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("Expected valid preference for: " + preferenceKey,
                    prefFragment.findPreference(preferenceKey), Matchers.notNullValue());
        });

        return TestThreadUtils.runOnUiThreadBlocking(
                () -> prefFragment.findPreference(preferenceKey));
    }
}
