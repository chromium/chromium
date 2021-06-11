// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import android.net.Uri;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Feature;

/**
 * Tests parts of the {@link RelatedSearchesList} class.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class RelatedSearchesListTest {
    private static final String QUERY_PARAM_NAME = "q";
    private static final String SAMPLE_STAMP = "1RcldCu";
    private static final String USER_POSITION_CODE = "Up";
    private static final String URL_1 =
            "https://www.google.com/search?q=1st query&ctxsl_rs=" + SAMPLE_STAMP;
    private static final String URL_2_NO_STAMP = "https://www.google.com/search?q=2nd+query";
    private static final String SAMPLE_JSON = "{\"content\":[{\"searchUrl\":\"" + URL_1
            + "\",\"title\":\"1st query\"},"
            + "{\"searchUrl\":\"" + URL_2_NO_STAMP + "\",\"title\":\"2nd query\"}]}";
    private static final String BAD_JSON = "Bad JSON!";

    private String mWarningReceived;

    @Before
    public void setup() {
        mWarningReceived = null;
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesList"})
    public void testGetQueries() {
        RelatedSearchesList relatedSearchesList =
                new RelatedSearchesList(SAMPLE_JSON, (warning) -> mWarningReceived = warning);
        assertNull(mWarningReceived);
        assertThat(relatedSearchesList.getQueries().get(0), equalTo("1st query"));
        assertThat(relatedSearchesList.getQueries().get(1), equalTo("2nd query"));
        assertNull(mWarningReceived);
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesList"})
    public void testGetQueriesBadJson() {
        RelatedSearchesList relatedSearchesList =
                new RelatedSearchesList(BAD_JSON, (warning) -> mWarningReceived = warning);
        assertThat(relatedSearchesList.getQueries().size(), is(0));
        assertThat(mWarningReceived, notNullValue());
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesList"})
    public void testGetSearchUri() {
        RelatedSearchesList relatedSearchesList =
                new RelatedSearchesList(SAMPLE_JSON, (warning) -> mWarningReceived = warning);
        assertThat(relatedSearchesList.getSearchUri(0).getQueryParameter(QUERY_PARAM_NAME),
                containsString("1st query"));
        assertThat(relatedSearchesList.getSearchUri(1).getQueryParameter(QUERY_PARAM_NAME),
                containsString("2nd query"));
        assertNull(mWarningReceived);

        // The first URL had a stamp, so check that it's now updated.
        Uri uriWithStamp = relatedSearchesList.getSearchUri(0);
        String updatedStamp = uriWithStamp.getQueryParameter(RelatedSearchesStamp.STAMP_PARAMETER);
        assertTrue(updatedStamp.startsWith(SAMPLE_STAMP));
        assertTrue(updatedStamp.length() > SAMPLE_STAMP.length());
        assertTrue(updatedStamp.endsWith(USER_POSITION_CODE + 0));

        // The second URL had no stamp, so check that there still is none.
        assertNull(relatedSearchesList.getSearchUri(1).getQueryParameter(
                RelatedSearchesStamp.STAMP_PARAMETER));
        assertNull(mWarningReceived);

        // Now index too far. We should just get a warning.
        assertNull(relatedSearchesList.getSearchUri(2));
        assertThat(mWarningReceived, containsString("searchUrl"));
    }
}
