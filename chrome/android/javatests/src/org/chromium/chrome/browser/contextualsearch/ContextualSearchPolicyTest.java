// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.mockito.Mockito.when;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

/**
 * Tests for the ContextualSearchPolicy class.
 */
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

    @Mock
    private ContextualSearchFakeServer mMockServer;

    private ContextualSearchPolicy mPolicy;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> mPolicy = new ContextualSearchPolicy(null, mMockServer));
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            setupAllConditionsToSendUrl();
            Assert.assertTrue(mPolicy.doSendBasePageUrl());
        });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testDoSendBasePageUrlWhenNotOptedIn() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            setupAllConditionsToSendUrl();
            mPolicy.overrideDecidedStateForTesting(false);
            Assert.assertFalse(mPolicy.doSendBasePageUrl());
        });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testDoSendBasePageUrlWhenNotMakingSearchAndBrowsingBetter() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TemplateUrl defaultSearchEngine =
                    TemplateUrlServiceFactory.get().getDefaultSearchEngineTemplateUrl();
            setupAllConditionsToSendUrl();
            TemplateUrlServiceFactory.get().setSearchEngine("yahoo.com");
            Assert.assertFalse(mPolicy.doSendBasePageUrl());
            // Set default search engine back to default to prevent cross-talk from
            // this test which sets it to Yahoo
            TemplateUrlServiceFactory.get().setSearchEngine(defaultSearchEngine.getShortName());
        });
    }

    // TODO(donnd): This set of tests is not complete, add more tests.
}
