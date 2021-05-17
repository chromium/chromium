// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Feature;

/**
 * Tests parts of the {@link RelatedSearchesList} class.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class RelatedSearchesListTest {
    private static final String SAMPLE_JSON =
            "{\"content\":[{\"searchUrl\":\"https://www.google.com/search?q=1st+query\",\"title\":\"1st query\"},"
            + "{\"searchUrl\":\"https://www.google.com/search?q=2nd+query\",\"title\":\"2nd query\"}]}";
    private static final String BAD_JSON = "Bad JSON!";

    @Test
    @Feature({"RelatedSearchesList", "RelatedSearchesList"})
    public void testGetQueries() {
        RelatedSearchesList relatedSearchesList = new RelatedSearchesList(SAMPLE_JSON);
        assertThat(relatedSearchesList.getQueries().get(0), equalTo("1st query"));
        assertThat(relatedSearchesList.getQueries().get(1), equalTo("2nd query"));
    }

    @Test
    @Feature({"RelatedSearchesList", "RelatedSearchesList"})
    public void testGetQueriesBadJson() {
        RelatedSearchesList relatedSearchesList = new RelatedSearchesList(BAD_JSON);
        assertThat(relatedSearchesList.getQueries().size(), is(0));
    }
}
