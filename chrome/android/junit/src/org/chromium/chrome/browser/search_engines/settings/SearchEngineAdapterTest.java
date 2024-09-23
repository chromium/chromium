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
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.TestActivity;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link SearchEngineAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SearchEngineAdapterTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    public final @Rule JniMocker mJniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private @Mock Profile mProfile;
    private @Mock TemplateUrlService mTemplateUrlService;
    private @Mock LargeIconBridge.Natives mLargeIconBridgeNativeMock;
    private Context mContext;

    @Before
    public void setUp() {
        mJniMocker.mock(LargeIconBridgeJni.TEST_HOOKS, mLargeIconBridgeNativeMock);
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
                templateUrls, p3, /* isInEeaChoiceCountry= */ true);

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
    public void testGetView() {
        TemplateUrl p1 = buildMockTemplateUrl("prepopulated1", 1);
        TemplateUrl p2 = buildMockTemplateUrl("", 2);
        TemplateUrl c1 = buildMockTemplateUrl("custom1", 0);

        doReturn(true).when(mTemplateUrlService).isLoaded();
        doReturn(new ArrayList<>(List.of(p1, p2, c1))).when(mTemplateUrlService).getTemplateUrls();
        doReturn(p2).when(mTemplateUrlService).getDefaultSearchEngineTemplateUrl();
        doReturn(false).when(mTemplateUrlService).isEeaChoiceCountry();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        var adapter = new SearchEngineAdapter(mContext, mProfile);
        adapter.start();

        assertEquals(adapter.getCount(), 4);

        // Checking the data that was used to render the view.
        assertEquals(adapter.getItemViewType(0), SearchEngineAdapter.VIEW_TYPE_ITEM);
        verify(p1, never()).getShortName();
        View v = adapter.getView(0, null, null);
        verify(p1, atLeastOnce()).getShortName();
        assertEquals(v.findViewById(R.id.url).getVisibility(), View.VISIBLE);
        assertThat(v.findViewById(R.id.logo), notNullValue());

        assertEquals(adapter.getItemViewType(1), SearchEngineAdapter.VIEW_TYPE_ITEM);
        verify(p2, never()).getShortName();
        v = adapter.getView(1, null, null);
        verify(p2, atLeastOnce()).getShortName();
        assertEquals(v.findViewById(R.id.url).getVisibility(), View.GONE); // Because no keyword.
        assertThat(v.findViewById(R.id.logo), notNullValue());

        assertEquals(adapter.getItemViewType(2), SearchEngineAdapter.VIEW_TYPE_DIVIDER);
        assertNotNull(adapter.getView(2, null, null));

        assertEquals(adapter.getItemViewType(3), SearchEngineAdapter.VIEW_TYPE_ITEM);
        verify(c1, never()).getShortName();
        v = adapter.getView(3, null, null);
        verify(c1, atLeastOnce()).getShortName();
        assertEquals(v.findViewById(R.id.url).getVisibility(), View.VISIBLE);
        assertThat(v.findViewById(R.id.logo), notNullValue());
    }
}
