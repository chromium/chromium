// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.policy.PolicyAuditor.AuditEvent;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.url.GURL;

import java.util.ArrayList;

/** PolicyAuditor integration test. */
@RunWith(ChromeJUnit4ClassRunner.class)
// TODO(crbug.com/344672097): Failing when batched, batch this again.
public class PolicyAuditorBridgeTest {
    private static class Entry {
        final int mEvent;
        final String mUrl;

        public Entry(int event, String url) {
            mEvent = event;
            mUrl = url;
        }
    }

    private static class FakePolicyAuditor extends PolicyAuditor {
        final ArrayList<Entry> mEntries = new ArrayList<>();

        public Entry getEntry(int index) {
            return mEntries.get(index);
        }

        public int getEntriesSize() {
            return mEntries.size();
        }

        @Override
        public void notifyAuditEvent(
                Context context, @AuditEvent int event, String url, String message) {
            mEntries.add(new Entry(event, url));
        }
    }

    private static final FakePolicyAuditor sFakePolicyAuditor = new FakePolicyAuditor();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(mActivityTestRule, false);

    @BeforeClass
    public static void setUpClass() {
        ServiceLoaderUtil.setInstanceForTesting(PolicyAuditor.class, sFakePolicyAuditor);
    }

    @Before
    public void setUp() {
        sFakePolicyAuditor.mEntries.clear();
    }

    @Test
    @SmallTest
    public void testSuccessfulNavigation() {
        mActivityTestRule.loadUrl(UrlConstants.VERSION_URL);

        FakePolicyAuditor fakePolicyAuditor = sFakePolicyAuditor;
        Assert.assertEquals(1, fakePolicyAuditor.getEntriesSize());
        Entry entry = fakePolicyAuditor.getEntry(0);
        Assert.assertEquals(AuditEvent.OPEN_URL_SUCCESS, entry.mEvent);
        Assert.assertEquals(UrlConstants.VERSION_URL, entry.mUrl);
    }

    @Test
    @SmallTest
    public void testUnsuccessfulNavigation() throws Exception {
        String invalidUrl = "https://invalid/";

        // Can't use the activity test rule to navigate to invalid urls, the rule has an assert that
        // fails the testcase upon unsuccessful navigations. So, use the tab directly to navigate to
        // the invalid url.
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        final CallbackHelper loadFinishCallback = new CallbackHelper();
        WebContentsObserver observer =
                new WebContentsObserver() {
                    @Override
                    public void didFinishLoadInPrimaryMainFrame(
                            GlobalRenderFrameHostId rfhId,
                            GURL url,
                            boolean isKnownValid,
                            @LifecycleState int rfhLifecycleState) {
                        loadFinishCallback.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.getWebContents().addObserver(observer);
                    tab.loadUrl(new LoadUrlParams(invalidUrl));
                });

        try {
            loadFinishCallback.waitForCallback(0);
        } finally {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        tab.getWebContents().removeObserver(observer);
                    });
        }

        FakePolicyAuditor fakePolicyAuditor = (FakePolicyAuditor) PolicyAuditor.maybeCreate();

        // After a failed navigation that is not caused by the url being blocked by an
        // administrator, we expect an OPEN_URL_FAILURE entry from didFinishNavigation, followed by
        // an OPEN_URL_SUCCESS entry from didFinishLoad. Android looper keeps executing tasks which
        // keeps calling didFinishNavigation, we only care about the first two after the navigation.
        Assert.assertTrue(fakePolicyAuditor.getEntriesSize() >= 2);

        Entry entry = fakePolicyAuditor.getEntry(0);
        Assert.assertEquals(AuditEvent.OPEN_URL_FAILURE, entry.mEvent);
        Assert.assertEquals(invalidUrl, entry.mUrl);
        entry = fakePolicyAuditor.getEntry(1);
        Assert.assertEquals(AuditEvent.OPEN_URL_SUCCESS, entry.mEvent);
        Assert.assertEquals("chrome-error://chromewebdata/", entry.mUrl);
    }
}
