// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ExploreSitesCategory} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExploreSitesCategoryUnitTest {
    @Test
    public void testAddSite() {
        final int id = 1;
        @ExploreSitesCategory.CategoryType
        final int type = ExploreSitesCategory.CategoryType.SCIENCE;
        final int ntpShownCount = 2;
        final int interactionCount = 3;
        final int siteId = 100;
        final String title = "test";
        final GURL url = JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL);
        final String categoryTitle = "Movies";

        ExploreSitesCategory category =
                new ExploreSitesCategory(id, type, categoryTitle, ntpShownCount, interactionCount);
        category.addSite(new ExploreSitesSite(siteId, title, url, true)); // blocked
        category.addSite(new ExploreSitesSite(siteId, title, url, false)); // not blocked

        assertEquals(id, category.getId());
        assertEquals(type, category.getType());
        assertEquals(ntpShownCount, category.getNtpShownCount());
        assertEquals(interactionCount, category.getInteractionCount());
        assertEquals(2, category.getSites().size());
        assertEquals(1, category.getNumDisplayed());
        assertEquals(siteId, category.getSites().get(0).getModel().get(ExploreSitesSite.ID_KEY));
        assertEquals(title, category.getSites().get(0).getModel().get(ExploreSitesSite.TITLE_KEY));
        assertEquals(url, category.getSites().get(0).getModel().get(ExploreSitesSite.URL_KEY));
        assertTrue(category.getSites().get(0).getModel().get(ExploreSitesSite.BLOCKED_KEY));
        assertFalse(category.getSites().get(1).getModel().get(ExploreSitesSite.BLOCKED_KEY));
    }

    @Test
    public void testRemoveSite() {
        final int id = 1;
        @ExploreSitesCategory.CategoryType
        final int type = ExploreSitesCategory.CategoryType.SCIENCE;
        final int siteId1 = 100;
        final int siteId2 = 101;
        final int siteId3 = 102;
        final int siteId4 = 103;
        final String title1 = "One";
        final String title2 = "Two";
        final String title3 = "Three";
        final String title4 = "Example";
        final GURL url1 = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
        final GURL url2 = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_2);
        final GURL url3 = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_3);
        final GURL url4 = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
        final String categoryTitle = "Science";

        ExploreSitesCategory category = new ExploreSitesCategory(
                id, type, categoryTitle, /* ntpShownCount = */ 2, /* interactionCount = */ 3);
        category.addSite(new ExploreSitesSite(siteId1, title1, url1, false)); // not blocked
        category.addSite(new ExploreSitesSite(siteId2, title2, url2, true)); // blocked
        category.addSite(new ExploreSitesSite(siteId3, title3, url3, false)); // not blocked
        category.addSite(new ExploreSitesSite(siteId4, title4, url4, false)); // not blocked

        // Now remove site3, which currently has tileIndex of 1.  We should be left with site 1 and
        // 4 still visible, and site 2 and 3 should be gone.  removeSite(1) means remove the second
        // displayed tile (which is site 3, since site 2 is blocked). Site 4 should occupy
        // tileIndex 1 after the removeSite operation completes.
        category.removeSite(1);

        assertEquals(4, category.getSites().size());
        assertEquals(2, category.getNumDisplayed());
        assertTrue(category.getSites().get(1).getModel().get(ExploreSitesSite.BLOCKED_KEY));
        assertTrue(category.getSites().get(2).getModel().get(ExploreSitesSite.BLOCKED_KEY));
        // The fourth site should now be at tile index '1' (the second tile).
        assertEquals(1, category.getSites().get(3).getModel().get(ExploreSitesSite.TILE_INDEX_KEY));
    }
}
