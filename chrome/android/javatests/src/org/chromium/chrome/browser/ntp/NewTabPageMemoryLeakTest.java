// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.GarbageCollectionTestUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.composeplate.ComposeplateUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.TilesLinearLayout;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.net.test.EmbeddedTestServer;

import java.lang.ref.WeakReference;
import java.util.List;

/** Tests to check memory leak of the native android New Tab Page. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "disable-features=IPH_FeedHeaderMenu"
})
@Batch(Batch.PER_CLASS)
public class NewTabPageMemoryLeakTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();
    @Rule public SigninTestRule mSigninTestRule = new SigninTestRule();

    private NewTabPage mNtp;

    @Before
    public void setUp() throws Exception {
        ComposeplateUtils.setIsEnabledForTesting(true);
        mActivityTestRule.startOnBlankPage();
        var templateUrlService =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                TemplateUrlServiceFactory.getForProfile(
                                        ProfileManager.getLastUsedRegularProfile()));
        TemplateUrlServiceFactory.setInstanceForTesting(templateUrlService);

        new OmniboxTestUtils(mActivityTestRule.getActivity());

        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

        List<SiteSuggestion> siteSuggestions =
                NewTabPageTestUtils.createFakeSiteSuggestions(testServer);
        FakeMostVisitedSites mostVisitedSites = new FakeMostVisitedSites();
        mostVisitedSites.setTileSuggestions(siteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mostVisitedSites;

        mActivityTestRule.loadUrl(getOriginalNativeNtpUrl());
        Tab tab = mActivityTestRule.getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(tab);

        Assert.assertTrue(tab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) tab.getNativePage();
        TilesLinearLayout mvTilesLayout = mNtp.getView().findViewById(R.id.mv_tiles_layout);
        Assert.assertEquals(siteSuggestions.size(), mvTilesLayout.getTileCount());
    }

    @Test
    @MediumTest
    @Feature("NewTabPage")
    public void testNewTabPageCanBeGarbageCollected() {
        WeakReference<NewTabPage> ntpRef = new WeakReference<>(mNtp);

        mActivityTestRule.loadUrl("about:blank");

        mNtp = null;
        Assert.assertTrue(GarbageCollectionTestUtils.canBeGarbageCollected(ntpRef));
    }
}
