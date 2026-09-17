// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.test.MockCertVerifierRuleAndroid;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.RenderFrameHostTestExt;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.ContentSwitches;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Tests the {@link CurrentPageVerifier} integration with Trusted Web Activity Mode. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    // Map all hostnames to 127.0.0.1 without a hardcoded port so MappedHostResolver preserves
    // the port from each request URL. Because @Batch(PER_CLASS) reuses the Network Service across
    // tests while EmbeddedTestServerRule restarts EmbeddedTestServer on a new port for each test,
    // hardcoding a single port in HOST_RESOLVER_RULES would cause subsequent tests to connect to
    // a previous test's closed port.
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
})
@Batch(PER_CLASS)
public final class TrustedWebActivityCurrentPageVerifierTest {
    /**
     * Timeout for a triggered main frame navigation to commit. Generous because main frame
     * navigations to {@code blob:} URLs go through {@code ExternalNavigationHandler}'s
     * external-intent resolution before falling back to committing in the browser.
     */
    private static final long NAVIGATION_COMMIT_TIMEOUT_SECONDS = 10L;

    public final CustomTabActivityTestRule mActivityTestRule = new CustomTabActivityTestRule();

    public MockCertVerifierRuleAndroid mCertVerifierRule =
            new MockCertVerifierRuleAndroid(0 /* net::OK */);

    @Rule
    public RuleChain mRuleChain =
            RuleChain.emptyRuleChain().around(mActivityTestRule).around(mCertVerifierRule);

    @Before
    public void setUp() {
        mActivityTestRule.setFinishActivity(true);
        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
    }

    /**
     * Returns an HTTPS URL for {@code host} targeting the current test's {@code EmbeddedTestServer}
     * port (e.g. {@code https://foo.com:<port>/...}), paired with the port-preserving {@code
     * HOST_RESOLVER_RULES} above.
     */
    private String getUrlForHost(String host, String relativeUrl) {
        return mActivityTestRule.getTestServer().getURLWithHostName(host, relativeUrl);
    }

    private void launchTwa(String url) throws TimeoutException {
        String packageName = ApplicationProvider.getApplicationContext().getPackageName();
        Intent intent = TrustedWebActivityTestUtil.createTrustedWebActivityIntent(url);
        TrustedWebActivityTestUtil.spoofVerification(packageName, url);
        TrustedWebActivityTestUtil.createSession(intent, packageName);
        mActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    private @VerificationStatus int getCurrentPageVerifierStatus() {
        CustomTabActivity customTabActivity = mActivityTestRule.getActivity();
        return customTabActivity.getCurrentPageVerifier().getState().status;
    }

    @Test
    @LargeTest
    public void testInScope() throws TimeoutException {
        String page =
                getUrlForHost("foo.com", "/chrome/test/data/android/customtabs/cct_header.html");
        String otherPageInScope =
                getUrlForHost(
                        "foo.com", "/chrome/test/data/android/customtabs/cct_header_frame.html");
        launchTwa(page);

        mActivityTestRule.loadUrl(otherPageInScope);

        TrustedWebActivityTestUtil.waitForCurrentPageVerifierToFinish(
                mActivityTestRule.getActivity());
        assertEquals(VerificationStatus.SUCCESS, getCurrentPageVerifierStatus());
    }

    @Test
    @LargeTest
    public void testOutsideScope() throws TimeoutException {
        String page = getUrlForHost("foo.com", "/chrome/test/data/android/simple.html");
        String pageDifferentOrigin =
                getUrlForHost("bar.com", "/chrome/test/data/android/simple.html");
        launchTwa(page);

        mActivityTestRule.loadUrl(pageDifferentOrigin, /* secondsToWait= */ 10);

        TrustedWebActivityTestUtil.waitForCurrentPageVerifierToFinish(
                mActivityTestRule.getActivity());
        assertEquals(VerificationStatus.FAILURE, getCurrentPageVerifierStatus());
    }

    /**
     * Tests that a renderer-initiated navigation to a same-origin blob: URL causes verification to
     * fail and shows browser controls.
     *
     * <p>Note: Because this blob: URL is created by {@code foo.com}, it has the same origin as the
     * verified TWA. Failing verification here is not the ideal desired platform behavior (ideally
     * same-origin blob: documents would remain trusted in app mode), but tests the current
     * implementation's intentional fail-safe behavior since {@code Origin#create()} cannot parse
     * non-HTTP(S) schemes from URL strings.
     */
    @Test
    @LargeTest
    public void testOutsideScope_rendererInitiatedSameOriginBlobNavigation()
            throws TimeoutException {
        String page = getUrlForHost("foo.com", "/chrome/test/data/android/simple.html");
        launchTwa(page);

        CustomTabActivity activity = mActivityTestRule.getActivity();
        assertTrue(
                "Should start off in Trusted Web Activity mode",
                TrustedWebActivityTestUtil.isTrustedWebActivity(activity));

        // This creates a same-origin blob: URL (blob:https://foo.com:<port>/...). Although
        // same-origin, current URL-based verification fails safe rather than leaving the unparsable
        // URL in app mode.
        navigateRendererInitiated(
                "const blob = new Blob(['<html><body>Same-origin blob content</body></html>'],"
                        + " {type: 'text/html'});"
                        + "location.href = URL.createObjectURL(blob);",
                "blob:https://foo.com:");

        TrustedWebActivityTestUtil.waitForCurrentPageVerifierToFinish(activity);
        assertEquals(VerificationStatus.FAILURE, getCurrentPageVerifierStatus());
        CriteriaHelper.pollInstrumentationThread(
                () -> !TrustedWebActivityTestUtil.isTrustedWebActivity(activity),
                "Browser controls should be shown for a blob: document");
    }

    /**
     * Tests that a renderer-initiated navigation to same-origin about:blank causes verification to
     * fail and shows browser controls.
     *
     * <p>Note: Because this navigation to about:blank is initiated by {@code foo.com}, the
     * committed about:blank document inherits {@code foo.com}'s origin. Failing verification here
     * is not the ideal desired platform behavior, but tests the current implementation's
     * intentional fail-safe behavior since {@code Origin#create()} cannot parse about:blank.
     */
    @Test
    @LargeTest
    public void testOutsideScope_rendererInitiatedSameOriginAboutBlankNavigation()
            throws TimeoutException {
        String page = getUrlForHost("foo.com", "/chrome/test/data/android/simple.html");
        launchTwa(page);

        CustomTabActivity activity = mActivityTestRule.getActivity();
        assertTrue(
                "Should start off in Trusted Web Activity mode",
                TrustedWebActivityTestUtil.isTrustedWebActivity(activity));

        // Initiated by foo.com, so the about:blank document is same-origin. Current URL-based
        // verification fails safe rather than leaving about:blank in app mode.
        navigateRendererInitiated("location.href = 'about:blank';", "about:blank");

        TrustedWebActivityTestUtil.waitForCurrentPageVerifierToFinish(activity);
        assertEquals(VerificationStatus.FAILURE, getCurrentPageVerifierStatus());
        CriteriaHelper.pollInstrumentationThread(
                () -> !TrustedWebActivityTestUtil.isTrustedWebActivity(activity),
                "Browser controls should be shown for an about:blank document");
    }

    /**
     * Tests that a top-level navigation to a cross-origin blob: URL initiated by an embedded
     * cross-origin iframe causes verification to fail and shows browser controls.
     *
     * <p>Unlike the same-origin blob test above, failing verification here is the desired long-term
     * behavior and must continue to fail even if follow-up work preserves app mode for same-origin
     * blob: URLs.
     */
    @Test
    @LargeTest
    public void testOutsideScope_rendererInitiatedCrossOriginBlobNavigation()
            throws TimeoutException {
        String page = getUrlForHost("foo.com", "/chrome/test/data/android/simple.html");
        launchTwa(page);

        CustomTabActivity activity = mActivityTestRule.getActivity();
        assertTrue(
                "Should start off in Trusted Web Activity mode",
                TrustedWebActivityTestUtil.isTrustedWebActivity(activity));

        navigateTopFromCrossOriginSubframe(
                "const blob = new Blob(['<html><body>Cross-origin blob content</body></html>'],"
                        + " {type: 'text/html'});"
                        + "top.location.href = URL.createObjectURL(blob);",
                "blob:https://bar.com:");

        TrustedWebActivityTestUtil.waitForCurrentPageVerifierToFinish(activity);
        assertEquals(VerificationStatus.FAILURE, getCurrentPageVerifierStatus());
        CriteriaHelper.pollInstrumentationThread(
                () -> !TrustedWebActivityTestUtil.isTrustedWebActivity(activity),
                "Browser controls should be shown for a cross-origin blob: document");
    }

    /**
     * Tests that a top-level navigation to about:blank initiated by an embedded cross-origin iframe
     * causes verification to fail and shows browser controls.
     *
     * <p>Because the navigation is initiated by {@code bar.com}, the resulting about:blank document
     * inherits {@code bar.com}'s cross-origin security context. Failing verification here is the
     * desired long-term behavior and must continue to fail even if follow-up work preserves app
     * mode for same-origin about:blank navigations.
     */
    @Test
    @LargeTest
    public void testOutsideScope_rendererInitiatedCrossOriginAboutBlankNavigation()
            throws TimeoutException {
        String page = getUrlForHost("foo.com", "/chrome/test/data/android/simple.html");
        launchTwa(page);

        CustomTabActivity activity = mActivityTestRule.getActivity();
        assertTrue(
                "Should start off in Trusted Web Activity mode",
                TrustedWebActivityTestUtil.isTrustedWebActivity(activity));

        navigateTopFromCrossOriginSubframe("top.location.href = 'about:blank';", "about:blank");

        TrustedWebActivityTestUtil.waitForCurrentPageVerifierToFinish(activity);
        assertEquals(VerificationStatus.FAILURE, getCurrentPageVerifierStatus());
        CriteriaHelper.pollInstrumentationThread(
                () -> !TrustedWebActivityTestUtil.isTrustedWebActivity(activity),
                "Browser controls should be shown for a cross-origin about:blank document");
    }

    /**
     * Runs {@code script} in the main frame and waits for the primary main frame to commit a
     * navigation to a URL starting with {@code expectedUrlPrefix}.
     */
    private void navigateRendererInitiated(String script, String expectedUrlPrefix)
            throws TimeoutException {
        WebContents webContents = mActivityTestRule.getActivity().getActivityTab().getWebContents();
        runAndWaitForMainFrameNavigationCommit(
                expectedUrlPrefix, () -> JavaScriptUtils.executeJavaScript(webContents, script));
    }

    /**
     * Injects a cross-origin iframe ({@code bar.com}) into the main frame, runs {@code script} in
     * that subframe with a user gesture, and waits for the primary main frame to commit a
     * navigation to a URL starting with {@code expectedUrlPrefix}.
     */
    private void navigateTopFromCrossOriginSubframe(String script, String expectedUrlPrefix)
            throws TimeoutException {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        String subframeUrl = getUrlForHost("bar.com", "/chrome/test/data/android/simple.html");
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(),
                "const f = document.createElement('iframe'); f.src = '"
                        + subframeUrl
                        + "'; document.body.appendChild(f);");
        CriteriaHelper.pollUiThread(
                () -> getSubframeForOrigin(tab, "https://bar.com:") != null,
                "Cross-origin subframe should commit");
        runAndWaitForMainFrameNavigationCommit(
                expectedUrlPrefix,
                () -> runScriptInCrossOriginSubframeWithUserGesture(tab, script));
    }

    /** Runs {@code script} with a user gesture in the {@code bar.com} subframe of {@code tab}. */
    private void runScriptInCrossOriginSubframeWithUserGesture(Tab tab, String script) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RenderFrameHost subframe = getSubframeForOrigin(tab, "https://bar.com:");
                    assertNotNull("Cross-origin subframe should still be live", subframe);
                    new RenderFrameHostTestExt(subframe).executeJavaScriptWithUserGesture(script);
                });
    }

    /**
     * Runs {@code trigger}, waits for the next cross-document primary main frame navigation to
     * commit, and asserts that the committed URL starts with {@code expectedUrlPrefix}.
     *
     * <p>Waiting on the navigation commit rather than on page load events is deliberate: this is
     * exactly the event {@link CurrentPageVerifier} keys off, so the test waits on the same signal
     * it asserts about. Page load events are unsuitable because the subframe loads used by the
     * cross-origin tests also notify tab load observers, which would let the wait finish before the
     * main frame had navigated.
     *
     * <p>This also relies on {@link CurrentPageVerifier}'s observer running before the one
     * registered here: WebContents observers are notified in registration order, and the tab's
     * observer is registered when the WebContents is created. Verification has therefore already
     * been kicked off for the new URL by the time this method returns, so a subsequent {@link
     * TrustedWebActivityTestUtil#waitForCurrentPageVerifierToFinish} cannot observe the previous
     * page's terminal state.
     */
    private void runAndWaitForMainFrameNavigationCommit(String expectedUrlPrefix, Runnable trigger)
            throws TimeoutException {
        WebContents webContents = mActivityTestRule.getActivity().getActivityTab().getWebContents();
        CallbackHelper navigationCommitted = new CallbackHelper();
        AtomicReference<String> committedUrl = new AtomicReference<>();
        WebContentsObserver observer =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                new WebContentsObserver(webContents) {
                                    @Override
                                    public void didFinishNavigationInPrimaryMainFrame(
                                            NavigationHandle navigation) {
                                        if (!navigation.hasCommitted()
                                                || navigation.isSameDocument()) {
                                            return;
                                        }
                                        committedUrl.set(navigation.getUrl().getSpec());
                                        navigationCommitted.notifyCalled();
                                    }
                                });
        try {
            trigger.run();
            navigationCommitted.waitForOnly(
                    "Primary main frame should commit a navigation to " + expectedUrlPrefix,
                    NAVIGATION_COMMIT_TIMEOUT_SECONDS,
                    TimeUnit.SECONDS);
        } finally {
            ThreadUtils.runOnUiThreadBlocking(() -> observer.observe(null));
        }
        assertTrue(
                "Committed URL " + committedUrl.get() + " should start with " + expectedUrlPrefix,
                committedUrl.get().startsWith(expectedUrlPrefix));
    }

    private RenderFrameHost getSubframeForOrigin(Tab tab, String originPrefix) {
        ThreadUtils.assertOnUiThread();
        RenderFrameHost mainFrame = tab.getWebContents().getMainFrame();
        if (mainFrame == null) return null;
        for (RenderFrameHost frame : mainFrame.getAllRenderFrameHosts()) {
            if (frame != mainFrame
                    && frame.getLastCommittedURL() != null
                    && frame.getLastCommittedURL().getSpec().startsWith(originPrefix)) {
                return frame;
            }
        }
        return null;
    }
}
