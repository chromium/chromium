// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.net.Uri;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Tests parts of the {@link RelatedSearchesList} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class RelatedSearchesListTest {
    private static final String QUERY_PARAM_NAME = "q";
    private static final String SAMPLE_STAMP = "1RcldCu";
    private static final String USER_POSITION_CODE = "Up";
    private static final String URL_1 =
            "https://www.google.com/search?q=1st+query&ctxsl_rs=" + SAMPLE_STAMP;
    private static final String URL_2_NO_STAMP = "https://www.google.com/search?q=2nd+query";
    private static final String SAMPLE_JSON =
            "{\"selection\":[{\"searchUrl\":\""
                    + URL_1
                    + "\",\"title\":\"1st query\"},"
                    + "{\"searchUrl\":\""
                    + URL_2_NO_STAMP
                    + "\",\"title\":\"2nd query\"}]}";
    private static final String BAD_JSON = "Bad JSON!";

    // TODO(donnd): Add failure messages to the asserts in all these tests.
    @Test
    @Feature({"RelatedSearches", "RelatedSearchesList"})
    public void testGetQueries() {
        RelatedSearchesList relatedSearchesList = new RelatedSearchesList(SAMPLE_JSON);
        assertThat(relatedSearchesList.getQueries().get(0), equalTo("1st query"));
        assertThat(relatedSearchesList.getQueries().get(1), equalTo("2nd query"));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesList"})
    public void testGetQueriesBadJson() {
        RelatedSearchesList relatedSearchesList = new RelatedSearchesList(BAD_JSON);
        assertThat(relatedSearchesList.getQueries().size(), is(0));
    }

    /** Asserts that the given URI has an updated stamp. */
    private void assertUdpatedStamp(Uri uriWithStamp) {
        String updatedStamp = uriWithStamp.getQueryParameter(RelatedSearchesStamp.STAMP_PARAMETER);
        assertTrue(updatedStamp.startsWith(SAMPLE_STAMP));
        assertTrue(updatedStamp.length() > SAMPLE_STAMP.length());
        assertTrue(updatedStamp.endsWith(USER_POSITION_CODE + 0));
    }

    @Test
    @Feature({"RelatedSearches", "RelatedSearchesList"})
    public void testGetSearchUri() {
        RelatedSearchesList relatedSearchesList = new RelatedSearchesList(SAMPLE_JSON);
        assertThat(
                relatedSearchesList.getSearchUri(0).getQueryParameter(QUERY_PARAM_NAME),
                containsString("1st query"));
        assertThat(
                relatedSearchesList.getSearchUri(1).getQueryParameter(QUERY_PARAM_NAME),
                containsString("2nd query"));

        // The first URL had a stamp, so check that it's now updated.
        Uri uriWithStamp = relatedSearchesList.getSearchUri(0);
        assertUdpatedStamp(uriWithStamp);

        // The second URL had no stamp, so check that there still is none.
        assertNull(
                relatedSearchesList
                        .getSearchUri(1)
                        .getQueryParameter(RelatedSearchesStamp.STAMP_PARAMETER));

        // Now index too far. We should just get a warning.
        assertNull(relatedSearchesList.getSearchUri(2));
    }
}
