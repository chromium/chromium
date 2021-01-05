// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.mockito.Mockito.doReturn;

import android.content.Intent;
import android.content.res.Configuration;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Instrumentation tests for the LocationBar component.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LocationBarTest {
    private static final String TEST_QUERY = "testing query";
    private static final List<String> TEST_PARAMS = Arrays.asList("foo=bar");

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    TemplateUrlService mTemplateUrlService;
    @Mock
    private TemplateUrl mGoogleSearchEngine;
    @Mock
    private TemplateUrl mNonGoogleSearchEngine;
    @Mock
    private LocaleManager mLocaleManager;

    ChromeTabbedActivity mActivity;
    UrlBar mUrlBar;
    LocationBarMediator mLocationBarMediator;
    String mSearchUrl;

    @Before
    public void setUp() throws InterruptedException {
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        LocaleManager.setInstanceForTest(mLocaleManager);
    }

    @After
    public void tearDown() {
        TemplateUrlServiceFactory.setInstanceForTesting(null);
        LocaleManager.setInstanceForTest(null);
    }

    private void startActivityNormally() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        doPostActivitySetup(mActivity);
    }

    private void startActivityWithDeferredNativeInitialization() {
        CommandLine.getInstance().appendSwitch(ChromeSwitches.DISABLE_NATIVE_INITIALIZATION);
        Intent intent = new Intent("about:blank");
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityTestRule.prepareUrlIntent(intent, "about:blank");
        mActivityTestRule.launchActivity(intent);
        mActivity = mActivityTestRule.getActivity();
        if (!mActivity.isInitialLayoutInflationComplete()) {
            AtomicBoolean isInflated = new AtomicBoolean();
            mActivity.getLifecycleDispatcher().register(new InflationObserver() {
                @Override
                public void onPreInflationStartup() {}

                @Override
                public void onPostInflationStartup() {
                    isInflated.set(true);
                }
            });
            CriteriaHelper.pollUiThread(isInflated::get);
        }
        doPostActivitySetup(mActivity);
    }

    private void triggerAndWaitForDeferredNativeInitialization() {
        CommandLine.getInstance().removeSwitch(ChromeSwitches.DISABLE_NATIVE_INITIALIZATION);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().startDelayedNativeInitializationForTests();
        });
        mActivityTestRule.waitForActivityNativeInitializationComplete();
    }

    private void doPostActivitySetup(ChromeActivity activity) {
        mUrlBar = activity.findViewById(org.chromium.chrome.R.id.url_bar);
        // clang-format off
        mLocationBarMediator = ((LocationBarCoordinator) activity.getToolbarManager()
            .getToolbarLayoutForTesting()
            .getLocationBar())
            .getMediatorForTesting();
        // clang-format on
        mSearchUrl = mActivityTestRule.getEmbeddedTestServerRule().getServer().getURL("/search");
    }

    @Test
    @MediumTest
    public void testSetSearchQueryFocusesUrlBar() {
        startActivityNormally();
        final String query = "testing query";

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mLocationBarMediator.setSearchQuery(query);
            Assert.assertEquals(query, mUrlBar.getTextWithoutAutocomplete());
            Assert.assertTrue(mLocationBarMediator.isUrlBarFocused());
        });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(mActivity, mUrlBar),
                    Matchers.is(true));
        });
    }

    @Test
    @MediumTest
    public void testSetSearchQueryFocusesUrlBar_preNative() {
        startActivityWithDeferredNativeInitialization();
        final String query = "testing query";

        TestThreadUtils.runOnUiThreadBlocking(() -> mLocationBarMediator.setSearchQuery(query));

        triggerAndWaitForDeferredNativeInitialization();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(query, mUrlBar.getTextWithoutAutocomplete());
            Assert.assertTrue(mLocationBarMediator.isUrlBarFocused());
        });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(mActivity, mUrlBar),
                    Matchers.is(true));
        });
    }

    @Test
    @MediumTest
    public void testPerformSearchQuery() {
        startActivityNormally();
        final List<String> params = Arrays.asList("foo=bar");
        doReturn(mSearchUrl)
                .when(mTemplateUrlService)
                .getUrlForSearchQuery(TEST_QUERY, TEST_PARAMS);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mLocationBarMediator.performSearchQuery(TEST_QUERY, TEST_PARAMS));

        ChromeTabUtils.waitForTabPageLoaded(
                mActivityTestRule.getActivity().getActivityTab(), mSearchUrl);
    }

    @Test
    @MediumTest
    public void testPerformSearchQuery_emptyUrl() {
        startActivityNormally();
        doReturn("").when(mTemplateUrlService).getUrlForSearchQuery(TEST_QUERY, TEST_PARAMS);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mLocationBarMediator.performSearchQuery(TEST_QUERY, TEST_PARAMS);
            Assert.assertEquals(TEST_QUERY, mUrlBar.getTextWithoutAutocomplete());
        });
    }

    @Test
    @MediumTest
    public void testOnConfigurationChanged() {
        startActivityNormally();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mLocationBarMediator.showUrlBarCursorWithoutFocusAnimations();
            Assert.assertTrue(mLocationBarMediator.isUrlBarFocused());
        });

        Configuration configuration = mActivity.getSavedConfigurationForTesting();
        configuration.keyboard = Configuration.KEYBOARD_12KEY;

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mLocationBarMediator.onConfigurationChanged(configuration);
            Assert.assertFalse(mLocationBarMediator.isUrlBarFocused());
        });
    }

    @Test
    @MediumTest
    public void testTemplateUrlServiceChange() throws InterruptedException {
        doReturn(false).when(mLocaleManager).needToCheckForSearchEnginePromo();
        doReturn(mGoogleSearchEngine).when(mTemplateUrlService).getDefaultSearchEngineTemplateUrl();
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        startActivityNormally();
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mLocationBarMediator.onTemplateURLServiceChanged());

        mActivityTestRule.typeInOmnibox("", true);
        Assert.assertEquals(R.drawable.ic_logo_googleg_20dp,
                mLocationBarMediator.getStatusCoordinatorForTesting()
                        .getSecurityIconResourceIdForTesting());

        doReturn(mNonGoogleSearchEngine)
                .when(mTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mLocationBarMediator.onTemplateURLServiceChanged());

        mActivityTestRule.typeInOmnibox("", true);
        Assert.assertEquals(R.drawable.ic_search,
                mLocationBarMediator.getStatusCoordinatorForTesting()
                        .getSecurityIconResourceIdForTesting());
    }

    @Test
    @MediumTest
    public void testPostDestroyFocusLogic() {
        startActivityNormally();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            LocationBarLayout locationBarLayout =
                    mActivity.findViewById(org.chromium.chrome.R.id.location_bar);
            locationBarLayout.destroy();
            locationBarLayout.finishUrlFocusChange(true, true);
            locationBarLayout.setUrlFocusChangeInProgress(false);
        });
    }
}
