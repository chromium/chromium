// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

/** Tests for the ContextualSearchPolicy class. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ContextualSearchPolicyTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Mock private ContextualSearchFakeServer mMockServer;

    private ContextualSearchPolicy mPolicy;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(() -> mPolicy = new ContextualSearchPolicy(null, mMockServer));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Clear Prefs
                    PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
                    prefService.clearPref(Pref.CONTEXTUAL_SEARCH_ENABLED);
                    prefService.clearPref(Pref.CONTEXTUAL_SEARCH_WAS_FULLY_PRIVACY_ENABLED);
                    prefService.clearPref(Pref.CONTEXTUAL_SEARCH_PROMO_CARD_SHOWN_COUNT);
                });
    }

    @After
    public void tearDown() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Clear Prefs
                    PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
                    prefService.clearPref(Pref.CONTEXTUAL_SEARCH_ENABLED);
                    prefService.clearPref(Pref.CONTEXTUAL_SEARCH_WAS_FULLY_PRIVACY_ENABLED);
                    prefService.clearPref(Pref.CONTEXTUAL_SEARCH_PROMO_CARD_SHOWN_COUNT);
                });
    }

    /** Call on the UI thread to set up all the conditions needed for sending the URL. */
    private void setupAllConditionsToSendUrl() {
        mPolicy.overrideDecidedStateForTesting(true);
        UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                Profile.getLastUsedRegularProfile(), true);
        try {
            when(mMockServer.getBasePageUrl()).thenReturn(new GURL("https://someUrl"));
        } catch (Exception e) {
            Assert.fail("Exception raised building a sample URL");
        }
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testDoSendBasePageUrlDefaultCase() {
        // We don't send the URL by default.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertFalse(mPolicy.doSendBasePageUrl()));
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testDoSendBasePageUrlEnabledCase() {
        // Test that we do send the URL when all the requirements are enabled.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setupAllConditionsToSendUrl();
                    Assert.assertTrue(mPolicy.doSendBasePageUrl());
                });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testDoSendBasePageUrlWhenNotOptedIn() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setupAllConditionsToSendUrl();
                    mPolicy.overrideDecidedStateForTesting(false);
                    Assert.assertFalse(mPolicy.doSendBasePageUrl());
                });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testDoSendBasePageUrlWhenNotMakingSearchAndBrowsingBetter() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setupAllConditionsToSendUrl();
                    UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                            Profile.getLastUsedRegularProfile(), false);
                    Assert.assertFalse(mPolicy.doSendBasePageUrl());
                });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testDoSendBasePageUrlWhenFtpProtocol() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setupAllConditionsToSendUrl();
                    try {
                        when(mMockServer.getBasePageUrl()).thenReturn(new GURL("ftp://someSource"));
                    } catch (Exception e) {
                        Assert.fail("Exception building FTP Uri");
                    }
                    Assert.assertFalse(mPolicy.doSendBasePageUrl());
                });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testDoSendBasePageUrlWhenNonGoogleSearchEngine() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TemplateUrlService templateUrlService =
                            TemplateUrlServiceFactory.getForProfile(
                                    Profile.getLastUsedRegularProfile());
                    TemplateUrl defaultSearchEngine =
                            templateUrlService.getDefaultSearchEngineTemplateUrl();
                    setupAllConditionsToSendUrl();
                    templateUrlService.setSearchEngine("yahoo.com");
                    Assert.assertFalse(mPolicy.doSendBasePageUrl());
                    // Set default search engine back to default to prevent cross-talk from
                    // this test which sets it to Yahoo
                    templateUrlService.setSearchEngine(defaultSearchEngine.getShortName());
                });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testIsUserUndecided_Disable() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mPolicy.isUserUndecided());
                    ContextualSearchPolicy.setContextualSearchFullyOptedIn(false);
                    Assert.assertFalse(mPolicy.isUserUndecided());
                    Assert.assertTrue(ContextualSearchPolicy.isContextualSearchUninitialized());
                });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testIsUserUndecided_Enable() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mPolicy.isUserUndecided());
                    ContextualSearchPolicy.setContextualSearchFullyOptedIn(true);
                    Assert.assertFalse(mPolicy.isUserUndecided());
                    Assert.assertTrue(ContextualSearchPolicy.isContextualSearchEnabled());
                });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testIsPromoAvailable() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mPolicy.isPromoAvailable());
                    Assert.assertEquals(
                            0, ContextualSearchPolicy.getContextualSearchPromoCardShownCount());

                    // Promo show 1 time and promo is still available.
                    ContextualSearchPolicy.onPromoShown();
                    Assert.assertTrue(mPolicy.isPromoAvailable());
                    Assert.assertEquals(
                            1, ContextualSearchPolicy.getContextualSearchPromoCardShownCount());

                    // After promo show 3 times, promo is not available.
                    ContextualSearchPolicy.onPromoShown();
                    ContextualSearchPolicy.onPromoShown();
                    Assert.assertFalse(mPolicy.isPromoAvailable());
                    Assert.assertEquals(
                            3, ContextualSearchPolicy.getContextualSearchPromoCardShownCount());
                });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testIsContextualSearchFullyOptedIn() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Default is not fully opted in.
                    Assert.assertFalse(ContextualSearchPolicy.isContextualSearchPrefFullyOptedIn());

                    // Choose not to opt in.
                    ContextualSearchPolicy.setContextualSearchFullyOptedIn(false);
                    Assert.assertFalse(ContextualSearchPolicy.isContextualSearchPrefFullyOptedIn());

                    // Choose to opt in.
                    ContextualSearchPolicy.setContextualSearchFullyOptedIn(true);
                    Assert.assertTrue(ContextualSearchPolicy.isContextualSearchPrefFullyOptedIn());
                });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testSetContextualSearchFullyOptedIn() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Default is not fully opted in.
                    Assert.assertFalse(ContextualSearchPolicy.isContextualSearchPrefFullyOptedIn());

                    // Choose to fully opt in.
                    ContextualSearchPolicy.setContextualSearchFullyOptedIn(true);
                    Assert.assertTrue(ContextualSearchPolicy.isContextualSearchPrefFullyOptedIn());

                    // Choose to disable Contextual Search.
                    ContextualSearchPolicy.setContextualSearchState(false);
                    // The Contextual Search pref is disabled, but opt-in pref should still be
                    // enabled since it is not disabled explicitly.
                    Assert.assertTrue(ContextualSearchPolicy.isContextualSearchDisabled());
                    Assert.assertTrue(ContextualSearchPolicy.isContextualSearchPrefFullyOptedIn());

                    // Enable the Contextual Search again.
                    ContextualSearchPolicy.setContextualSearchState(true);
                    Assert.assertTrue(ContextualSearchPolicy.isContextualSearchPrefFullyOptedIn());
                });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testShouldPreviousGestureResolve() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(mPolicy.shouldPreviousGestureResolve());

                    ContextualSearchPolicy.setContextualSearchFullyOptedIn(true);
                    Assert.assertTrue(mPolicy.shouldPreviousGestureResolve());

                    ContextualSearchPolicy.setContextualSearchFullyOptedIn(false);
                    Assert.assertFalse(mPolicy.shouldPreviousGestureResolve());
                });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testIsContextualSearchFullyEnabled() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(mPolicy.isContextualSearchFullyEnabled());

                    ContextualSearchPolicy.setContextualSearchFullyOptedIn(true);
                    Assert.assertTrue(mPolicy.isContextualSearchFullyEnabled());

                    ContextualSearchPolicy.setContextualSearchFullyOptedIn(false);
                    Assert.assertFalse(mPolicy.isContextualSearchFullyEnabled());
                });
    }
    // TODO(donnd): This set of tests is not complete, add more tests.
}
