// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user.website_approval;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.url.GURL;

/** Unit tests for {@link WebsiteApprovalSheetContent}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class WebsiteApprovalSheetContentTest {
    private static final String PREFIX = "https://www.";

    private static final String BASE_URL = "somehost.com";

    private static final String PATH_WITHIN_LIMIT =
            "/path0/path1/path2/path3/path4/path5/path6/"
                    + "path7/path8/path9/path10/path11/path12/path13/path14/path15";

    @Test
    @SmallTest
    public void testTruncateLongUrlNoTruncation() {
        // Url within MAX_HOST_SIZE test case.
        String expectedUri = BASE_URL + PATH_WITHIN_LIMIT;
        GURL url = new GURL(PREFIX + expectedUri);

        Assert.assertTrue(url.getSpec().length() < WebsiteApprovalSheetContent.MAX_HOST_SIZE);
        WebsiteApprovalSheetContent.StringSpecs specs =
                WebsiteApprovalSheetContent.truncateLongUrl(url);

        assertEquals(expectedUri, specs.mFormattedString);
    }

    @Test
    @SmallTest
    public void testTruncateLongUrlNoTruncationNoEllipsis() {
        String urlWithPathWithElipsisLimit = BASE_URL;
        int elipsisSize = 3;
        String charPad = "a";
        while (urlWithPathWithElipsisLimit.length() + elipsisSize
                < WebsiteApprovalSheetContent.MAX_FULL_URL_SIZE) {
            if (urlWithPathWithElipsisLimit.length() + elipsisSize + PATH_WITHIN_LIMIT.length()
                    < WebsiteApprovalSheetContent.MAX_FULL_URL_SIZE) {
                urlWithPathWithElipsisLimit = urlWithPathWithElipsisLimit + PATH_WITHIN_LIMIT;
            } else {
                urlWithPathWithElipsisLimit = urlWithPathWithElipsisLimit + charPad;
            }
        }

        GURL url = new GURL(PREFIX + urlWithPathWithElipsisLimit);
        Assert.assertTrue(url.getSpec().length() > WebsiteApprovalSheetContent.MAX_FULL_URL_SIZE);

        WebsiteApprovalSheetContent.StringSpecs specs =
                WebsiteApprovalSheetContent.truncateLongUrl(url);

        assertEquals(urlWithPathWithElipsisLimit, specs.mFormattedString);
    }

    @SmallTest
    @Test
    public void testTruncateLongUrlWithPathTruncationWithEllipsis() {
        // Truncate url with long path.
        String pathAboveLimit = "";
        String subpath = "";
        while (pathAboveLimit.length() < WebsiteApprovalSheetContent.MAX_FULL_URL_SIZE) {
            pathAboveLimit = pathAboveLimit + PATH_WITHIN_LIMIT;

            if (subpath.length() + PATH_WITHIN_LIMIT.length()
                    <= WebsiteApprovalSheetContent.SUBSTRING_LIMIT) {
                subpath = subpath + PATH_WITHIN_LIMIT;
            } else if (subpath.length() < WebsiteApprovalSheetContent.SUBSTRING_LIMIT) {
                for (int i = 0;
                        i < PATH_WITHIN_LIMIT.length()
                                && subpath.length() < WebsiteApprovalSheetContent.SUBSTRING_LIMIT;
                        i++) {
                    subpath = subpath + PATH_WITHIN_LIMIT.charAt(i);
                }
            }
        }
        String uri = BASE_URL + pathAboveLimit;
        GURL url = new GURL(PREFIX + uri);

        Assert.assertTrue(url.getSpec().length() > WebsiteApprovalSheetContent.MAX_FULL_URL_SIZE);
        WebsiteApprovalSheetContent.StringSpecs specs =
                WebsiteApprovalSheetContent.truncateLongUrl(url);

        String expected = BASE_URL + subpath + "...";
        assertEquals(expected, specs.mFormattedString);
    }
}
