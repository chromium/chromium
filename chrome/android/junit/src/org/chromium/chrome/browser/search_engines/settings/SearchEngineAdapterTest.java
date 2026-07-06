// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.notNullValue;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.components.search_engines.TemplateUrlTestHelpers.buildMockTemplateUrl;

import android.content.Context;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.regional_capabilities.RegionalCapabilitiesServiceFactory;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.regional_capabilities.RegionalCapabilitiesService;
import org.chromium.components.search_engines.PrepopulatedAndRecentlyVisitedTemplateURLs;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.TestActivity;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link SearchEngineAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures({
    OmniboxFeatureList.OMNIBOX_SITE_SEARCH,
    ChromeFeatureList.SEARCH_SETTINGS_UPDATE_V2
})
public class SearchEngineAdapterTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private @Mock Profile mProfile;
    private @Mock TemplateUrlService mTemplateUrlService;
    private @Mock RegionalCapabilitiesService mRegionalCapabilities;
    private @Mock LargeIconBridge.Natives mLargeIconBridgeNativeMock;
    private Context mContext;

    @Before
    public void setUp() {
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeNativeMock);
        mActivityScenarioRule.getScenario().onActivity(activity -> mContext = activity);
    }

    @Test
    public void testSortAndFilterUnnecessaryTemplateUrl_PrepopulatedEnginesSorting() {
        String name = "prepopulated";
        long lastVisitedTime = System.currentTimeMillis();
        TemplateUrl p1 = buildMockTemplateUrl(name, 1, lastVisitedTime);
        TemplateUrl p2 = buildMockTemplateUrl(name, 2, lastVisitedTime);
        TemplateUrl p3 = buildMockTemplateUrl(name, 3, lastVisitedTime);
        TemplateUrl p4 = buildMockTemplateUrl(name, 4, lastVisitedTime);

        List<TemplateUrl> templateUrls = List.of(p2, p1, p4, p3);
        TemplateUrl[] expectedSortedUrls = new TemplateUrl[] {p1, p2, p3, p4};
        TemplateUrl[] expectedNonSortedUrls = new TemplateUrl[] {p2, p1, p4, p3};

        // When computing the list for the new settings in the EEA, don't re-sort prepopulated
        // engines.

        List<TemplateUrl> modifiedList = new ArrayList<>(templateUrls);
        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(
                modifiedList, p3, /* isEeaChoiceCountry= */ true);
        assertThat(modifiedList, contains(expectedNonSortedUrls));

        // In all the other cases (old settings or out of EEA), keep sorting by ID.

        modifiedList = new ArrayList<>(templateUrls);
        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(
                modifiedList, p3, /* isEeaChoiceCountry= */ false);
        assertThat(modifiedList, contains(expectedSortedUrls));
    }

    @Test
    public void testSortAndFilterUnnecessaryTemplateUrl_PrePopBeforeCustom() {
        long lastVisitedTime = System.currentTimeMillis();
        TemplateUrl p1 = buildMockTemplateUrl("prepopulated1", 1, lastVisitedTime);
        TemplateUrl p2 = buildMockTemplateUrl("prepopulated2", 2, lastVisitedTime);
        TemplateUrl c1 = buildMockTemplateUrl("custom1", 0, lastVisitedTime);

        List<TemplateUrl> templateUrls = new ArrayList<>(List.of(p1, c1, p2));
        checkSortAndFilterOutput(templateUrls, p1, List.of(p1, p2, c1));
    }

    @Test
    public void testSortAndFilterUnnecessaryTemplateUrl_CustomSortedByRecency() {
        long lastVisitedTime = System.currentTimeMillis();
        TemplateUrl c1 = buildMockTemplateUrl("custom1", 0, lastVisitedTime);
        TemplateUrl c2 = buildMockTemplateUrl("custom2", 0, lastVisitedTime - 1);
        TemplateUrl c3 = buildMockTemplateUrl("custom3", 0, lastVisitedTime - 2);

        List<TemplateUrl> templateUrls = new ArrayList<>(List.of(c3, c1, c2));
        checkSortAndFilterOutput(
                templateUrls, buildMockTemplateUrl("default", 0, 0), List.of(c1, c2, c3));
    }

    @Test
    public void testSortAndFilterUnnecessaryTemplateUrl_DefaultCustomSortedUp() {
        long lastVisitedTime = System.currentTimeMillis();
        TemplateUrl p1 = buildMockTemplateUrl("prepopulated", 1, lastVisitedTime - 5);
        TemplateUrl c1 = buildMockTemplateUrl("custom1", 0, lastVisitedTime);
        TemplateUrl c2 = buildMockTemplateUrl("custom2", 0, lastVisitedTime - 1);
        TemplateUrl c3 = buildMockTemplateUrl("custom3", 0, lastVisitedTime - 2);

        List<TemplateUrl> templateUrls = new ArrayList<>(List.of(c3, c1, c2, p1));
        checkSortAndFilterOutput(templateUrls, c2, List.of(p1, c2, c1, c3));
    }

    @Test
    public void testSortAndFilterUnnecessaryTemplateUrl_equalInstancesNotReordered() {
        String name = "prepopulated";
        long lastVisitedTime = System.currentTimeMillis();
        TemplateUrl p1 = buildMockTemplateUrl(name, 0, lastVisitedTime, 42);
        TemplateUrl p2 = buildMockTemplateUrl(name, 0, lastVisitedTime, 42);
        TemplateUrl p3 = buildMockTemplateUrl(name, 0, lastVisitedTime, 42);

        List<TemplateUrl> templateUrls = new ArrayList<>(List.of(p2, p1, p3));
        checkSortAndFilterOutput(templateUrls, p3, List.of(p2, p1, p3));

        // Instead of using the test helper, call the method directly and explicitly compare
        // identity for the output instead of equality here, as all instances are equal.
        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(
                templateUrls, p3, /* isEeaChoiceCountry= */ true);

        Assert.assertSame(templateUrls.get(0), p2);
        Assert.assertSame(templateUrls.get(1), p1);
        Assert.assertSame(templateUrls.get(2), p3);
    }

    @Test
    public void testSortAndFilterUnnecessaryTemplateUrl_LimitsCustomCount() {
        long lastVisitedTime = System.currentTimeMillis();
        TemplateUrl p1 = buildMockTemplateUrl("prepopulated", 1, lastVisitedTime);
        TemplateUrl c1 = buildMockTemplateUrl("custom1", 0, lastVisitedTime);
        TemplateUrl c2 = buildMockTemplateUrl("custom2", 0, lastVisitedTime);
        TemplateUrl c3 = buildMockTemplateUrl("custom3", 0, lastVisitedTime);
        TemplateUrl c4 = buildMockTemplateUrl("custom4", 0, lastVisitedTime);
        TemplateUrl c5 = buildMockTemplateUrl("custom5", 0, lastVisitedTime);

        List<TemplateUrl> templateUrls = new ArrayList<>(List.of(p1, c1, c2, c3, c4, c5));
        checkSortAndFilterOutput(templateUrls, p1, List.of(p1, c1, c2, c3));
    }

    @Test
    public void testSortAndFilterUnnecessaryTemplateUrl_LimitsCustomCountDseNotCounting() {
        long lastVisitedTime = System.currentTimeMillis();
        TemplateUrl p1 = buildMockTemplateUrl("prepopulated", 1, lastVisitedTime);
        TemplateUrl c1 = buildMockTemplateUrl("custom1", 0, lastVisitedTime);
        TemplateUrl c2 = buildMockTemplateUrl("custom2", 0, lastVisitedTime);
        TemplateUrl c3 = buildMockTemplateUrl("custom3", 0, lastVisitedTime);
        TemplateUrl c4 = buildMockTemplateUrl("custom4", 0, lastVisitedTime);
        TemplateUrl c5 = buildMockTemplateUrl("custom5", 0, lastVisitedTime);

        List<TemplateUrl> templateUrls = new ArrayList<>(List.of(p1, c1, c2, c3, c4, c5));
        checkSortAndFilterOutput(templateUrls, c1, List.of(p1, c1, c2, c3, c4));
    }

    @Test
    public void testSortAndFilterUnnecessaryTemplateUrl_RemovesOldCustom() {
        long recentTime = System.currentTimeMillis();
        long pastCutoffTime =
                System.currentTimeMillis() - SearchEngineAdapter.MAX_DISPLAY_TIME_SPAN_MS - 1;
        TemplateUrl p1 = buildMockTemplateUrl("prepopulated", 1, pastCutoffTime);
        TemplateUrl c1 = buildMockTemplateUrl("custom1", 0, recentTime);
        TemplateUrl c2 = buildMockTemplateUrl("custom2", 0, pastCutoffTime);
        TemplateUrl c3 = buildMockTemplateUrl("custom3", 0, pastCutoffTime);

        List<TemplateUrl> templateUrls = new ArrayList<>(List.of(p1, c1, c2, c3));
        checkSortAndFilterOutput(templateUrls, c3, List.of(p1, c3, c1));
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_SITE_SEARCH)
    public void testSortAndFilterUnnecessaryTemplateUrl_DisableRecentSearchEngines() {
        long lastVisitedTime = System.currentTimeMillis();
        TemplateUrl p1 = buildMockTemplateUrl("prepopulated", 1, lastVisitedTime);
        TemplateUrl c1 = buildMockTemplateUrl("custom1", 0, lastVisitedTime);
        TemplateUrl r1 = buildMockTemplateUrl("recent1", 0, lastVisitedTime);
        TemplateUrl r2 = buildMockTemplateUrl("recent2", 0, lastVisitedTime);

        List<TemplateUrl> templateUrls = new ArrayList<>(List.of(p1, c1, r1, r2));
        checkSortAndFilterOutput(templateUrls, c1, List.of(p1, c1));
    }

    /**
     * Calls {@link SearchEngineAdapter#sortAndFilterUnnecessaryTemplateUrl} twice to verify that
     * the outputs are consistent. The first time it indicates that the user is in the EEA, and the
     * second that they are not. Other inputs are kept the same.
     */
    private void checkSortAndFilterOutput(
            List<TemplateUrl> input,
            TemplateUrl defaultSearchEngine,
            List<TemplateUrl> expectedOutput) {
        List<TemplateUrl> modifiedList = new ArrayList<>(input);
        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(
                modifiedList, defaultSearchEngine, /* isEeaChoiceCountry= */ true);
        assertThat(modifiedList, contains(expectedOutput.toArray()));

        modifiedList = new ArrayList<>(input);
        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(
                modifiedList, defaultSearchEngine, /* isEeaChoiceCountry= */ false);
        assertThat(modifiedList, contains(expectedOutput.toArray()));

        modifiedList = new ArrayList<>(input);
        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(
                modifiedList, defaultSearchEngine, /* isEeaChoiceCountry= */ true);
        assertThat(modifiedList, contains(expectedOutput.toArray()));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SEARCH_SETTINGS_UPDATE_V2)
    public void testGetView_New() {
        TemplateUrl p1 = buildMockTemplateUrl("prepopulated1", 1);
        TemplateUrl p2 = buildMockTemplateUrl("", 2);
        TemplateUrl c1 = buildMockTemplateUrl("custom1", 0);

        doReturn(true).when(mTemplateUrlService).isLoaded();
        doReturn(new PrepopulatedAndRecentlyVisitedTemplateURLs(List.of(p1, p2), List.of(c1)))
                .when(mTemplateUrlService)
                .getPrepopulatedAndRecentlyVisitedTemplateURLs();
        doReturn(p2).when(mTemplateUrlService).getDefaultSearchEngineTemplateUrl();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        var adapter =
                new SearchEngineAdapter(mContext, mProfile, /* siteSearchClickHandler= */ null);
        adapter.start();

        assertEquals(4, adapter.getCount());

        // Checking the data that was used to render the view.
        assertEquals(SearchEngineAdapter.ViewType.ITEM, adapter.getItemViewType(0));
        View v = adapter.getView(0, null, null);
        verify(p1, atLeastOnce()).getShortName();
        assertEquals(View.VISIBLE, v.findViewById(R.id.url).getVisibility());
        assertThat(v.findViewById(R.id.logo), notNullValue());

        assertEquals(SearchEngineAdapter.ViewType.ITEM, adapter.getItemViewType(1));
        v = adapter.getView(1, null, null);
        verify(p2, atLeastOnce()).getShortName();
        assertEquals(View.GONE, v.findViewById(R.id.url).getVisibility()); // Because no keyword.
        assertThat(v.findViewById(R.id.logo), notNullValue());

        assertEquals(SearchEngineAdapter.ViewType.DIVIDER, adapter.getItemViewType(2));
        assertNotNull(adapter.getView(2, null, null));

        assertEquals(SearchEngineAdapter.ViewType.ITEM, adapter.getItemViewType(3));
        v = adapter.getView(3, null, null);
        verify(c1, atLeastOnce()).getShortName();
        assertEquals(View.VISIBLE, v.findViewById(R.id.url).getVisibility());
        assertThat(v.findViewById(R.id.logo), notNullValue());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SEARCH_SETTINGS_UPDATE_V2)
    public void refreshData_unknownDseAddedToRecents() {
        // Avoid JavaExceptionReporter misfires on bots that test official builds.
        if (VersionInfo.isOfficialBuild()) return;
        long lastVisitedTime = System.currentTimeMillis();
        TemplateUrl p1 = buildMockTemplateUrl("prepopulated1", 1, lastVisitedTime);
        TemplateUrl p2 = buildMockTemplateUrl("prepopulated2", 2, lastVisitedTime);
        TemplateUrl unknownDse = buildMockTemplateUrl("unknown", 0, lastVisitedTime);

        doReturn(true).when(mTemplateUrlService).isLoaded();
        doReturn(new PrepopulatedAndRecentlyVisitedTemplateURLs(List.of(p1, p2), List.of()))
                .when(mTemplateUrlService)
                .getPrepopulatedAndRecentlyVisitedTemplateURLs();
        doReturn(unknownDse).when(mTemplateUrlService).getDefaultSearchEngineTemplateUrl();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        var adapter =
                new SearchEngineAdapter(mContext, mProfile, /* siteSearchClickHandler= */ null);
        adapter.start();

        // The adapter will show 2 prepopulated engines, a divider, and the unknown DSE.
        assertEquals(4, adapter.getCount());
        assertEquals(p1.getKeyword(), adapter.getItem(0).getKeyword());
        assertEquals(p2.getKeyword(), adapter.getItem(1).getKeyword());
        // Item 2 is a divider.
        assertEquals(unknownDse.getKeyword(), adapter.getItem(3).getKeyword());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SEARCH_SETTINGS_UPDATE_V2)
    public void refreshData_dseSuppressedByPolicy() {
        // Avoid JavaExceptionReporter misfires on bots that test official builds.
        if (VersionInfo.isOfficialBuild()) return;
        long lastVisitedTime = System.currentTimeMillis();
        TemplateUrl p1 = buildMockTemplateUrl("prepopulated1", 1, lastVisitedTime);
        TemplateUrl p2 = buildMockTemplateUrl("prepopulated2", 2, lastVisitedTime);

        doReturn(true).when(mTemplateUrlService).isLoaded();
        doReturn(new PrepopulatedAndRecentlyVisitedTemplateURLs(List.of(p1, p2), List.of()))
                .when(mTemplateUrlService)
                .getPrepopulatedAndRecentlyVisitedTemplateURLs();
        doReturn(null).when(mTemplateUrlService).getDefaultSearchEngineTemplateUrl();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        var adapter =
                new SearchEngineAdapter(mContext, mProfile, /* siteSearchClickHandler= */ null);
        adapter.start();

        // The adapter will show 2 prepopulated engines, a divider, and the unknown DSE.
        assertEquals(2, adapter.getCount());
        assertEquals(p1.getKeyword(), adapter.getItem(0).getKeyword());
        assertEquals(p2.getKeyword(), adapter.getItem(1).getKeyword());
    }

    @Test
    public void testGetView_Legacy() {
        TemplateUrl p1 = buildMockTemplateUrl("prepopulated1", 1);
        TemplateUrl p2 = buildMockTemplateUrl("", 2);
        TemplateUrl c1 = buildMockTemplateUrl("custom1", 0);

        doReturn(true).when(mTemplateUrlService).isLoaded();
        doReturn(new ArrayList<>(List.of(p1, p2, c1))).when(mTemplateUrlService).getTemplateUrls();
        doReturn(p2).when(mTemplateUrlService).getDefaultSearchEngineTemplateUrl();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        doReturn(false).when(mRegionalCapabilities).isInEeaCountry();
        RegionalCapabilitiesServiceFactory.setInstanceForTesting(mRegionalCapabilities);

        var adapter =
                new SearchEngineAdapter(mContext, mProfile, /* siteSearchClickHandler= */ null);
        adapter.start();

        assertEquals(4, adapter.getCount());

        // Checking the data that was used to render the view.
        assertEquals(SearchEngineAdapter.ViewType.ITEM, adapter.getItemViewType(0));
        View v = adapter.getView(0, null, null);
        verify(p1, atLeastOnce()).getShortName();
        assertEquals(View.VISIBLE, v.findViewById(R.id.url).getVisibility());
        assertThat(v.findViewById(R.id.logo), notNullValue());

        assertEquals(SearchEngineAdapter.ViewType.ITEM, adapter.getItemViewType(1));
        v = adapter.getView(1, null, null);
        verify(p2, atLeastOnce()).getShortName();
        assertEquals(View.GONE, v.findViewById(R.id.url).getVisibility()); // Because no keyword.
        assertThat(v.findViewById(R.id.logo), notNullValue());

        assertEquals(SearchEngineAdapter.ViewType.DIVIDER, adapter.getItemViewType(2));
        assertNotNull(adapter.getView(2, null, null));

        assertEquals(SearchEngineAdapter.ViewType.ITEM, adapter.getItemViewType(3));
        v = adapter.getView(3, null, null);
        verify(c1, atLeastOnce()).getShortName();
        assertEquals(View.VISIBLE, v.findViewById(R.id.url).getVisibility());
        assertThat(v.findViewById(R.id.logo), notNullValue());
    }

    @Test
    public void refreshData_unknownDseAddedToRecents_Legacy() {
        // Avoid JavaExceptionReporter misfires on bots that test official builds.
        if (VersionInfo.isOfficialBuild()) return;
        long lastVisitedTime = System.currentTimeMillis();
        TemplateUrl p1 = buildMockTemplateUrl("prepopulated1", 1, lastVisitedTime);
        TemplateUrl p2 = buildMockTemplateUrl("prepopulated2", 2, lastVisitedTime);
        TemplateUrl unknownDse = buildMockTemplateUrl("unknown", 0, lastVisitedTime);

        doReturn(true).when(mTemplateUrlService).isLoaded();
        doReturn(new ArrayList<>(List.of(p1, p2))).when(mTemplateUrlService).getTemplateUrls();
        doReturn(unknownDse).when(mTemplateUrlService).getDefaultSearchEngineTemplateUrl();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        // Test for non-EEA country.
        doReturn(false).when(mRegionalCapabilities).isInEeaCountry();
        RegionalCapabilitiesServiceFactory.setInstanceForTesting(mRegionalCapabilities);

        var adapter =
                new SearchEngineAdapter(mContext, mProfile, /* siteSearchClickHandler= */ null);
        adapter.start();

        // The adapter will show 2 prepopulated engines, a divider, and the unknown DSE.
        assertEquals(4, adapter.getCount());
        assertEquals(p1.getKeyword(), adapter.getItem(0).getKeyword());
        assertEquals(p2.getKeyword(), adapter.getItem(1).getKeyword());
        // Item 2 is a divider.
        assertEquals(unknownDse.getKeyword(), adapter.getItem(3).getKeyword());

        // Test for EEA country.
        doReturn(true).when(mRegionalCapabilities).isInEeaCountry();
        RegionalCapabilitiesServiceFactory.setInstanceForTesting(mRegionalCapabilities);

        adapter = new SearchEngineAdapter(mContext, mProfile, /* siteSearchClickHandler= */ null);
        adapter.start();

        // The adapter will show 2 prepopulated engines, a divider, and the unknown DSE.
        assertEquals(4, adapter.getCount());
        assertEquals(p1.getKeyword(), adapter.getItem(0).getKeyword());
        assertEquals(p2.getKeyword(), adapter.getItem(1).getKeyword());
        // Item 2 is a divider.
        assertEquals(unknownDse.getKeyword(), adapter.getItem(3).getKeyword());
    }

    @Test
    public void refreshData_dseSuppressedByPolicy_Legacy() {
        // Avoid JavaExceptionReporter misfires on bots that test official builds.
        if (VersionInfo.isOfficialBuild()) return;
        long lastVisitedTime = System.currentTimeMillis();
        TemplateUrl p1 = buildMockTemplateUrl("prepopulated1", 1, lastVisitedTime);
        TemplateUrl p2 = buildMockTemplateUrl("prepopulated2", 2, lastVisitedTime);

        doReturn(true).when(mTemplateUrlService).isLoaded();
        doReturn(new ArrayList<>(List.of(p1, p2))).when(mTemplateUrlService).getTemplateUrls();
        doReturn(null).when(mTemplateUrlService).getDefaultSearchEngineTemplateUrl();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        // Test for non-EEA country.
        doReturn(false).when(mRegionalCapabilities).isInEeaCountry();
        RegionalCapabilitiesServiceFactory.setInstanceForTesting(mRegionalCapabilities);

        var adapter =
                new SearchEngineAdapter(mContext, mProfile, /* siteSearchClickHandler= */ null);
        adapter.start();

        // The adapter will show 2 prepopulated engines, a divider, and the unknown DSE.
        assertEquals(2, adapter.getCount());
        assertEquals(p1.getKeyword(), adapter.getItem(0).getKeyword());
        assertEquals(p2.getKeyword(), adapter.getItem(1).getKeyword());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SEARCH_SETTINGS_UPDATE_V2)
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_SITE_SEARCH)
    public void refreshData_V2_doesNotDereferenceStaleTemplateUrls() {
        TemplateUrl p1 = buildMockTemplateUrl("p1", 1);
        TemplateUrl r1 = buildMockTemplateUrl("r1", 0);

        doReturn(true).when(mTemplateUrlService).isLoaded();
        doReturn(new PrepopulatedAndRecentlyVisitedTemplateURLs(List.of(p1), List.of(r1)))
                .when(mTemplateUrlService)
                .getPrepopulatedAndRecentlyVisitedTemplateURLs();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        var adapter = new SearchEngineAdapter(mContext, mProfile, null);
        adapter.start();

        // New list: r1 is replaced by r2.
        TemplateUrl r2 = buildMockTemplateUrl("r2", 0);
        doReturn(new PrepopulatedAndRecentlyVisitedTemplateURLs(List.of(p1), List.of(r2)))
                .when(mTemplateUrlService)
                .getPrepopulatedAndRecentlyVisitedTemplateURLs();

        // Simulate r1 being freed in native.
        clearInvocations(r1);

        // This should not crash if the fix is correct.
        adapter.onTemplateURLServiceChanged();

        verify(r1, never()).getKeyword();
        verify(r1, never()).getShortName();
        verify(r1, never()).getIsPrepopulated();
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.SEARCH_SETTINGS_UPDATE_V2,
        OmniboxFeatureList.OMNIBOX_SITE_SEARCH
    })
    public void refreshData_Legacy_doesNotDereferenceStaleTemplateUrls() {
        TemplateUrl p1 = buildMockTemplateUrl("p1", 1);
        TemplateUrl r1 = buildMockTemplateUrl("r1", 0);

        doReturn(true).when(mTemplateUrlService).isLoaded();
        // In legacy mode, it uses getTemplateUrls() and sorts them.
        doReturn(new ArrayList<>(List.of(p1, r1))).when(mTemplateUrlService).getTemplateUrls();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        RegionalCapabilitiesServiceFactory.setInstanceForTesting(mRegionalCapabilities);

        var adapter = new SearchEngineAdapter(mContext, mProfile, null);
        adapter.start();

        // New list: r1 is replaced by r2.
        TemplateUrl r2 = buildMockTemplateUrl("r2", 0);
        doReturn(new ArrayList<>(List.of(p1, r2))).when(mTemplateUrlService).getTemplateUrls();

        // Simulate r1 being freed in native.
        clearInvocations(r1);

        // This should not crash if the fix is correct.
        adapter.onTemplateURLServiceChanged();

        verify(r1, never()).getKeyword();
        verify(r1, never()).getShortName();
        verify(r1, never()).getIsPrepopulated();
    }
}
