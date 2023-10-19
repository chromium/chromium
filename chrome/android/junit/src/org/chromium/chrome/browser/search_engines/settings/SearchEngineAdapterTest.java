// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;

import android.text.format.DateUtils;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.search_engines.TemplateUrl;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link SearchEngineAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SearchEngineAdapterTest {
    @Test
    public void testSortandGetCustomSearchEngine() {
        long currentTime = System.currentTimeMillis();
        TemplateUrl dse = new MockTemplateUrl(0, "default", currentTime);

        MockTemplateUrl prepopulated1 = new MockTemplateUrl(11, "prepopulated1", currentTime);
        prepopulated1.isPrepopulated = true;
        prepopulated1.prepopulatedId = 0;

        MockTemplateUrl prepopulated2 = new MockTemplateUrl(12, "prepopulated2", currentTime - 1);
        prepopulated2.isPrepopulated = true;
        prepopulated2.prepopulatedId = 1;

        MockTemplateUrl prepopulated3 = new MockTemplateUrl(13, "prepopulated3", currentTime - 2);
        prepopulated3.isPrepopulated = true;
        prepopulated3.prepopulatedId = 2;

        MockTemplateUrl custom1 = new MockTemplateUrl(101, "custom_keyword1", currentTime);
        MockTemplateUrl custom2 = new MockTemplateUrl(102, "custom_keyword2", currentTime - 1);
        MockTemplateUrl custom3 = new MockTemplateUrl(103, "custom_keyword3", currentTime - 2);
        MockTemplateUrl custom4 = new MockTemplateUrl(104, "custom_keyword4", currentTime - 3);
        MockTemplateUrl custom5 = new MockTemplateUrl(105, "custom_keyword5", currentTime - 4);

        ArrayList<TemplateUrl> templateUrls =
                new ArrayList<>(
                        Arrays.asList(
                                dse,
                                prepopulated1,
                                prepopulated2,
                                prepopulated3,
                                custom1,
                                custom2,
                                custom3));

        List<TemplateUrl> output = new ArrayList<>(templateUrls);
        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(output, dse);
        assertThat(
                output,
                contains(
                        prepopulated1,
                        prepopulated2,
                        prepopulated3,
                        dse,
                        custom1,
                        custom2,
                        custom3));

        // Mark one of the custom engines as older than the visible threshold.
        custom2.updateAgeInDays(3);
        output = new ArrayList<>(templateUrls);
        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(output, dse);
        assertThat(
                output,
                contains(prepopulated1, prepopulated2, prepopulated3, dse, custom1, custom3));

        // Mark one of the custom engines as older than the other.
        custom1.updateAgeInDays(1);
        output = new ArrayList<>(templateUrls);
        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(output, dse);
        assertThat(
                output,
                contains(prepopulated1, prepopulated2, prepopulated3, dse, custom3, custom1));

        // Include more than 3 custom serach engines and ensure they're filtered accordingly.
        templateUrls.add(custom4);
        templateUrls.add(custom5);
        output = new ArrayList<>(templateUrls);
        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(output, dse);
        assertThat(
                output,
                contains(
                        prepopulated1,
                        prepopulated2,
                        prepopulated3,
                        dse,
                        custom3,
                        custom4,
                        custom5));

        // Specify an older custom search engine as default, and ensure it is included as well as
        // the 3 most recent custom search engines.
        output =
                new ArrayList<>(
                        Arrays.asList(
                                prepopulated1,
                                prepopulated2,
                                custom1,
                                custom2,
                                custom3,
                                custom4,
                                custom5));
        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(output, custom2);
        assertThat(
                output, contains(prepopulated1, prepopulated2, custom2, custom3, custom4, custom5));
    }

    @Test
    public void testSortandGetCustomSearchEngine_PrepopulateIdOrdering() {
        long currentTime = System.currentTimeMillis();
        MockTemplateUrl prepopulated1 = new MockTemplateUrl(11, "prepopulated1", currentTime);
        prepopulated1.isPrepopulated = true;
        prepopulated1.prepopulatedId = 3;

        MockTemplateUrl prepopulated2 = new MockTemplateUrl(12, "prepopulated2", currentTime - 1);
        prepopulated2.isPrepopulated = true;
        prepopulated2.prepopulatedId = 1;

        MockTemplateUrl prepopulated3 = new MockTemplateUrl(13, "prepopulated3", currentTime - 2);
        prepopulated3.isPrepopulated = true;
        prepopulated3.prepopulatedId = 4;

        List<TemplateUrl> templateUrls = Arrays.asList(prepopulated1, prepopulated2, prepopulated3);

        List<TemplateUrl> output = new ArrayList<>(templateUrls);
        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(output, prepopulated1);
        assertThat(output, contains(prepopulated2, prepopulated1, prepopulated3));

        prepopulated1.prepopulatedId = 0;
        output = new ArrayList<>(templateUrls);
        SearchEngineAdapter.sortAndFilterUnnecessaryTemplateUrl(output, prepopulated1);
        assertThat(output, contains(prepopulated1, prepopulated2, prepopulated3));
    }

    private static class MockTemplateUrl extends TemplateUrl {
        public String shortName = "";
        public int prepopulatedId;
        public boolean isPrepopulated;
        public String keyword = "";
        public long lastVisitedTime;
        public String url = "https://testurl.com/?searchstuff={searchTerms}";

        public MockTemplateUrl(long fakeNativePtr, String keyword, long lastVisitedTime) {
            super(fakeNativePtr);
            this.keyword = keyword;
            this.shortName = keyword;
            this.lastVisitedTime = lastVisitedTime;
        }

        @Override
        public String getShortName() {
            return shortName;
        }

        @Override
        public int getPrepopulatedId() {
            return prepopulatedId;
        }

        @Override
        public boolean getIsPrepopulated() {
            return isPrepopulated;
        }

        @Override
        public String getKeyword() {
            return keyword;
        }

        @Override
        public long getLastVisitedTime() {
            return lastVisitedTime;
        }

        @Override
        public String getURL() {
            return url;
        }

        void updateAgeInDays(int days) {
            lastVisitedTime = System.currentTimeMillis() - DateUtils.DAY_IN_MILLIS * days;
        }
    }
}
