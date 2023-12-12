// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.notNullValue;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.components.search_engines.TemplateUrlTestHelpers.buildMockTemplateUrl;

import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link SearchEngineAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SearchEngineAdapterTest {
    public @Rule TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    public final @Rule JniMocker mJniMocker = new JniMocker();

    private @Mock Profile mProfile;
    private @Mock TemplateUrlService mTemplateUrlService;
    private @Mock LargeIconBridge.Natives mLargeIconBridgeNativeMock;

    @Before
    public void setUp() {
        mJniMocker.mock(LargeIconBridgeJni.TEST_HOOKS, mLargeIconBridgeNativeMock);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.SEARCH_ENGINE_CHOICE)
    public void testSortAndFilterUnnecessaryTemplateUrl_SortsByPrePopId() {
        String name = "prepopulated";
        long lastVisitedTime = System.currentTimeMillis();
        TemplateUrl p1 = buildMockTemplateUrl(name, 1, lastVisitedTime);
        TemplateUrl p2 = buildMockTemplateUrl(name, 2, lastVisitedTime);
        TemplateUrl p3 = buildMockTemplateUrl(name, 3, lastVisitedTime);
        TemplateUrl p4 = buildMockTemplateUrl(name, 4, lastVisitedTime);

        List<TemplateUrl> templateUrls = new ArrayList<>(List.of(p2, p1, p4, p3));
        checkSortAndFilterOutput(templateUrls, p3, List.of(p1, p2, p3, p4));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.SEARCH_ENGINE_CHOICE)
    public void testSortAndFilterUnnecessaryTemplateUrl_SecFeatureDoesNotSortPrePopInEEA() {
        String name = "prepopulated";
        long lastVisitedTime = System.currentTimeMillis();
        TemplateUrl p1 = buildMockTemplateUrl(name, 1, lastVisitedTime);
        TemplateUrl p2 = buildMockTemplateUrl(name, 2, lastVisitedTime);
        TemplateUrl p3 = buildMockTemplateUrl(name, 3, lastVisitedTime);
        TemplateUrl p4 = buildMockTemplateUrl(name, 4, lastVisitedTime);

        List<TemplateUrl> templateUrls = new ArrayList<>(List.of(p2, p1, p4, p3));

        // Unlike other tests, this one's outcome depends on `isInEeaChoiceCountry`.
        List<TemplateUrl> modifiedList = new ArrayList<>(templateUrls);
        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(
                modifiedList, p3, /* isInEeaChoiceCountry= */ true);
        assertThat(modifiedList, contains(p2, p1, p4, p3));

        modifiedList = new ArrayList<>(templateUrls);
        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(
                modifiedList, p3, /* isInEeaChoiceCountry= */ false);
        assertThat(modifiedList, contains(p1, p2, p3, p4));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.SEARCH_ENGINE_CHOICE)
    public void testSortAndFilterUnnecessaryTemplateUrl_PrePopBeforeCustom_WithSecFeature() {
        baseTestSortAndFilterUnnecessaryTemplateUrl_PrePopBeforeCustom();
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.SEARCH_ENGINE_CHOICE)
    public void testSortAndFilterUnnecessaryTemplateUrl_PrePopBeforeCustom_WithoutSecFeature() {
        baseTestSortAndFilterUnnecessaryTemplateUrl_PrePopBeforeCustom();
    }

    private void baseTestSortAndFilterUnnecessaryTemplateUrl_PrePopBeforeCustom() {
        // Test outcome does not depend on the SearchEngineChoice feature state, but since
        // comparisons of prepopulated engines is affected by the flag, it needs to be set to some
        // value so the test does not crash.

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
    @Features.EnableFeatures(ChromeFeatureList.SEARCH_ENGINE_CHOICE)
    public void testSortAndFilterUnnecessaryTemplateUrl_equalInstancesNotReordered() {
        // Test outcome does not depend on the SearchEngineChoice feature state, but since
        // comparisons of prepopulated engines is affected by the flag, it needs to be set to some
        // value so the test does not crash.

        String name = "prepopulated";
        long lastVisitedTime = System.currentTimeMillis();
        TemplateUrl p1 = buildMockTemplateUrl(name, 0, lastVisitedTime, 42);
        TemplateUrl p2 = buildMockTemplateUrl(name, 0, lastVisitedTime, 42);
        TemplateUrl p3 = buildMockTemplateUrl(name, 0, lastVisitedTime, 42);

        List<TemplateUrl> templateUrls = new ArrayList<>(List.of(p2, p1, p3));
        checkSortAndFilterOutput(templateUrls, p3, List.of(p2, p1, p3));

        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(
                templateUrls, p3, /* isInEeaChoiceCountry= */ false);

        // Explicitly comparing identity instead of equality here, as all instances are equal.
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
                modifiedList, defaultSearchEngine, /* isInEeaChoiceCountry= */ true);
        assertThat(modifiedList, contains(expectedOutput.toArray()));

        modifiedList = new ArrayList<>(input);
        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(
                modifiedList, defaultSearchEngine, /* isInEeaChoiceCountry= */ false);
        assertThat(modifiedList, contains(expectedOutput.toArray()));
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.SEARCH_ENGINE_CHOICE)
    public void testGetView() {
        baseTestGetView(/* expectLogos= */ false);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.SEARCH_ENGINE_CHOICE)
    public void testGetView_WithSecFeature() {
        baseTestGetView(/* expectLogos= */ true);
    }

    private void baseTestGetView(boolean expectLogos) {
        TemplateUrl p1 = buildMockTemplateUrl("prepopulated1", 1);
        TemplateUrl p2 = buildMockTemplateUrl("", 2);
        TemplateUrl c1 = buildMockTemplateUrl("custom1", 0);

        doReturn(true).when(mTemplateUrlService).isLoaded();
        doReturn(new ArrayList<>(List.of(p1, p2, c1))).when(mTemplateUrlService).getTemplateUrls();
        doReturn(p2).when(mTemplateUrlService).getDefaultSearchEngineTemplateUrl();
        doReturn(false).when(mTemplateUrlService).isEeaChoiceCountry();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        var adapter =
                new SearchEngineAdapter(ApplicationProvider.getApplicationContext(), mProfile);
        adapter.start();

        assertEquals(adapter.getCount(), 4);

        // Checking the data that was used to render the view.
        assertEquals(adapter.getItemViewType(0), SearchEngineAdapter.VIEW_TYPE_ITEM);
        verify(p1, never()).getShortName();
        View v = adapter.getView(0, null, null);
        verify(p1, atLeastOnce()).getShortName();
        assertEquals(v.findViewById(R.id.url).getVisibility(), View.VISIBLE);
        assertThat(v.findViewById(R.id.logo), is(expectLogos ? notNullValue() : nullValue()));

        assertEquals(adapter.getItemViewType(1), SearchEngineAdapter.VIEW_TYPE_ITEM);
        verify(p2, never()).getShortName();
        v = adapter.getView(1, null, null);
        verify(p2, atLeastOnce()).getShortName();
        assertEquals(v.findViewById(R.id.url).getVisibility(), View.GONE); // Because no keyword.
        assertThat(v.findViewById(R.id.logo), is(expectLogos ? notNullValue() : nullValue()));

        assertEquals(adapter.getItemViewType(2), SearchEngineAdapter.VIEW_TYPE_DIVIDER);
        assertNotNull(adapter.getView(2, null, null));

        assertEquals(adapter.getItemViewType(3), SearchEngineAdapter.VIEW_TYPE_ITEM);
        verify(c1, never()).getShortName();
        v = adapter.getView(3, null, null);
        verify(c1, atLeastOnce()).getShortName();
        assertEquals(v.findViewById(R.id.url).getVisibility(), View.VISIBLE);
        assertThat(v.findViewById(R.id.logo), is(expectLogos ? notNullValue() : nullValue()));
    }
}
