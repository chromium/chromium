// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContentsOriginMatcher;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;

import java.util.Arrays;

/** AwContentsOriginMatcher tests. */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS) // These are unit tests
@DoNotBatch(reason = "Shared dependencies among the tests cause conflicts during batch testing.")
public class AwContentsOriginMatcherTest {
    @Rule public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private AwContentsOriginMatcher mMatcher;

    @Before
    public void setup() {
        mMatcher = new AwContentsOriginMatcher();
    }

    @After
    public void tearDown() {
        mMatcher.destroy();
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testMatchesOrigin_returnsFalse_whenEmpty() {
        Assert.assertFalse(mMatcher.matchesOrigin(Uri.parse("http://webview.com")));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testMatchesOrigin_returnsFalse_whenNoMatching() {
        mMatcher.updateRuleList(Arrays.asList("http://somesite.com", "http://webviewalmost.com"));
        Assert.assertFalse(mMatcher.matchesOrigin(Uri.parse("http://webview.com")));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testMatchesOrigin_returnsTrue_whenMatches() {
        mMatcher.updateRuleList(Arrays.asList("http://somesite.com", "http://webview.com"));
        Assert.assertTrue(mMatcher.matchesOrigin(Uri.parse("http://webview.com")));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testMatchesOrigin_returnsTrue_whenSubDomainMatches() {
        mMatcher.updateRuleList(Arrays.asList("http://somesite.com", "http://*.webview.com"));
        Assert.assertTrue(mMatcher.matchesOrigin(Uri.parse("http://sub.webview.com")));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testUpdateRuleList() {
        String[] badRules =
                mMatcher.updateRuleList(
                        Arrays.asList("http://somesite.com", "oh no", "http://*.webview.com"));
        // Still doesn't match even though that list had a rule that should match.
        // Because updateRuleList won't update the rules if there was a bad one.
        Assert.assertFalse(mMatcher.matchesOrigin(Uri.parse("http://sub.webview.com")));
        Assert.assertEquals(new String[] {"oh no"}, badRules);

        // After removing the bad rule, we should no longer get any rules returned and things should
        // match
        badRules =
                mMatcher.updateRuleList(
                        Arrays.asList("http://somesite.com", "http://*.webview.com"));

        Assert.assertTrue(mMatcher.matchesOrigin(Uri.parse("http://sub.webview.com")));
        Assert.assertEquals(new String[] {}, badRules);
    }

    @Test(expected = IllegalStateException.class)
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testThrowsAfterDestroy() {
        AwContentsOriginMatcher matcher = new AwContentsOriginMatcher();
        matcher.destroy();
        matcher.matchesOrigin(Uri.parse("http://sub.webview.com"));
    }
}
