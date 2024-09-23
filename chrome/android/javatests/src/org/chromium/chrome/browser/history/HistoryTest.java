// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertNull;

import android.graphics.Bitmap;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** Tests for history feature. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class HistoryTest {
    private static class FaviconWaiter extends CallbackHelper
            implements FaviconHelper.FaviconImageCallback {
        private Bitmap mFavicon;

        @Override
        public void onFaviconAvailable(Bitmap image, GURL iconUrl) {
            mFavicon = image;
            notifyCalled();
        }

        public Bitmap waitForFavicon() throws TimeoutException {
            waitForOnly();
            return mFavicon;
        }
    }

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    /**
     * Check that the favicons for {@link UrlConstants#HISTORY_URL} and for {@link
     * UrlConstants#NATIVE_HISTORY_URL} are identical.
     */
    @Test
    @SmallTest
    public void testFavicon() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        FaviconHelper helper = ThreadUtils.runOnUiThreadBlocking(FaviconHelper::new);
        // If the returned favicons are non-null Bitmap#sameAs() should be used.
        assertNull(getFavicon(helper, new GURL(UrlConstants.HISTORY_URL)));
        assertNull(getFavicon(helper, new GURL(UrlConstants.NATIVE_HISTORY_URL)));
    }

    public Bitmap getFavicon(FaviconHelper helper, GURL pageUrl) throws TimeoutException {
        FaviconWaiter waiter = new FaviconWaiter();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    helper.getLocalFaviconImageForURL(
                            ProfileManager.getLastUsedRegularProfile(), pageUrl, 0, waiter);
                });
        return waiter.waitForFavicon();
    }
}
