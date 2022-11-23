// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.PatternMatcher;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.lifecycle.Stage;
import android.text.TextUtils;
import android.util.Base64;
import android.util.Pair;
import android.widget.TextView;

import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.PackageManagerWrapper;
import org.chromium.base.test.util.Restriction;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.InterceptNavigationDelegateTabHelper;
import org.chromium.chrome.browser.tab.RedirectHandlerTabHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResultType;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;
import org.chromium.components.external_intents.RedirectHandler;
import org.chromium.components.external_intents.TestChildFrameNavigationObserver;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageStateHandler;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Test suite for verifying the behavior of various URL overriding actions.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class UrlOverridingTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityRule = new CustomTabActivityTestRule();

    private static final String BASE_PATH = "/chrome/test/data/android/url_overriding/";
    private static final String HELLO_PAGE = BASE_PATH + "hello.html";
    private static final String NAVIGATION_FROM_TIMEOUT_PAGE =
            BASE_PATH + "navigation_from_timer.html";
    private static final String NAVIGATION_FROM_TIMEOUT_WITH_FALLBACK_PAGE =
            BASE_PATH + "navigation_from_timer_with_fallback.html";
    private static final String NAVIGATION_FROM_TIMEOUT_PARENT_FRAME_PAGE =
            BASE_PATH + "navigation_from_timer_parent_frame.html";
    private static final String NAVIGATION_FROM_USER_GESTURE_PAGE =
            BASE_PATH + "navigation_from_user_gesture.html";
    private static final String NAVIGATION_FROM_USER_GESTURE_PARENT_FRAME_PAGE =
            BASE_PATH + "navigation_from_user_gesture_parent_frame.html";
    private static final String NAVIGATION_FROM_XHR_CALLBACK_PAGE =
            BASE_PATH + "navigation_from_xhr_callback.html";
    private static final String NAVIGATION_FROM_XHR_CALLBACK_PARENT_FRAME_PAGE =
            BASE_PATH + "navigation_from_xhr_callback_parent_frame.html";
    private static final String NAVIGATION_FROM_XHR_CALLBACK_AND_SHORT_TIMEOUT_PAGE =
            BASE_PATH + "navigation_from_xhr_callback_and_short_timeout.html";
    private static final String NAVIGATION_WITH_FALLBACK_URL_PAGE =
            BASE_PATH + "navigation_with_fallback_url.html";
    private static final String NAVIGATION_WITH_FALLBACK_URL_PARENT_FRAME_PAGE =
            BASE_PATH + "navigation_with_fallback_url_parent_frame.html";
    private static final String FALLBACK_LANDING_PATH = BASE_PATH + "hello.html";
    private static final String OPEN_WINDOW_FROM_USER_GESTURE_PAGE =
            BASE_PATH + "open_window_from_user_gesture.html";
    private static final String OPEN_WINDOW_FROM_LINK_USER_GESTURE_PAGE =
            BASE_PATH + "open_window_from_link_user_gesture.html";
    private static final String OPEN_WINDOW_FROM_SVG_USER_GESTURE_PAGE =
            BASE_PATH + "open_window_from_svg_user_gesture.html";
    private static final String NAVIGATION_FROM_JAVA_REDIRECTION_PAGE =
            BASE_PATH + "navigation_from_java_redirection.html";
    private static final String NAVIGATION_TO_CCT_FROM_INTENT_URI =
            BASE_PATH + "navigation_to_cct_via_intent_uri.html";
    private static final String NAVIGATION_TO_FILE_SCHEME_FROM_INTENT_URI =
            BASE_PATH + "navigation_to_file_scheme_via_intent_uri.html";
    private static final String SUBFRAME_REDIRECT_WITH_PLAY_FALLBACK =
            BASE_PATH + "subframe_navigation_with_play_fallback.html";
    private static final String REDIRECT_TO_OTHER_BROWSER =
            BASE_PATH + "redirect_to_other_browser.html";
    private static final String NAVIGATION_FROM_BFCACHE =
            BASE_PATH + "navigation_from_bfcache-1.html";
    private static final String NAVIGATION_FROM_PRERENDER =
            BASE_PATH + "navigation_from_prerender.html";
    private static final String NAVIGATION_FROM_FENCED_FRAME =
            BASE_PATH + "navigation_from_fenced_frame.html";
    private static final String NAVIGATION_FROM_LONG_TIMEOUT =
            BASE_PATH + "navigation_from_long_timeout.html";
    private static final String NAVIGATION_FROM_PAGE_SHOW =
            BASE_PATH + "navigation_from_page_show.html";

    private static final String OTHER_BROWSER_PACKAGE = "com.other.browser";
    private static final String NON_BROWSER_PACKAGE = "not.a.browser";

    @Mock
    private RedirectHandler mRedirectHandler;

    @Spy
    private RedirectHandler mSpyRedirectHandler;

    private static class TestTabObserver extends EmptyTabObserver {
        private final CallbackHelper mFinishCallback;
        private final CallbackHelper mFailCallback;
        private final CallbackHelper mDestroyedCallback;

        TestTabObserver(final CallbackHelper finishCallback, final CallbackHelper failCallback,
                final CallbackHelper destroyedCallback) {
            mFinishCallback = finishCallback;
            mFailCallback = failCallback;
            mDestroyedCallback = destroyedCallback;
        }

        @Override
        public void onPageLoadFinished(Tab tab, GURL url) {
            mFinishCallback.notifyCalled();
        }

        @Override
        public void onDestroyed(Tab tab) {
            // A new tab is destroyed when loading is overridden while opening it.
            mDestroyedCallback.notifyCalled();
        }
    }

    private static ResolveInfo newResolveInfo(String packageName) {
        ActivityInfo ai = new ActivityInfo();
        ai.packageName = packageName;
        ai.name = "Name: " + packageName;
        ai.applicationInfo = new ApplicationInfo();
        ai.exported = true;
        ResolveInfo ri = new ResolveInfo();
        ri.activityInfo = ai;
        return ri;
    }

    private static class TestContext extends ContextWrapper {
        private boolean mResolveToNonBrowserPackage;
        private String mHostToMatch;
        private IntentFilter mFilterForHostMatch;

        public TestContext(Context baseContext) {
            super(baseContext);
        }

        public void setResolveBrowserIntentToNonBrowserPackage(boolean toNonBrowser) {
            mResolveToNonBrowserPackage = toNonBrowser;
        }

        private boolean targetsPlay(Intent intent) {
            if (intent.getPackage() != null
                    && intent.getPackage().equals(ExternalNavigationHandler.PLAY_APP_PACKAGE)) {
                return true;
            }
            if (intent.getScheme() != null && intent.getScheme().equals("market")) return true;
            return false;
        }

        private void setIntentFilterForHost(String host, IntentFilter filter) {
            mHostToMatch = host;
            mFilterForHostMatch = filter;
        }

        @Override
        public PackageManager getPackageManager() {
            return new PackageManagerWrapper(super.getPackageManager()) {
                @Override
                public List<ResolveInfo> queryIntentActivities(Intent intent, int flags) {
                    if ((intent.getPackage() != null
                                && intent.getPackage().equals(OTHER_BROWSER_PACKAGE))
                            || intent.filterEquals(PackageManagerUtils.BROWSER_INTENT)) {
                        return Arrays.asList(newResolveInfo(OTHER_BROWSER_PACKAGE));
                    }

                    // Behave as though play store is not installed - this matches bot emulator
                    // images.
                    if (targetsPlay(intent)) return null;

                    if (mHostToMatch != null && intent.getData() != null
                            && intent.getData().getHost().equals(mHostToMatch)) {
                        ResolveInfo info = newResolveInfo(NON_BROWSER_PACKAGE);
                        info.filter = mFilterForHostMatch;
                        return Arrays.asList(info);
                    }

                    return TestContext.super.getPackageManager().queryIntentActivities(
                            intent, flags);
                }

                @Override
                public ResolveInfo resolveActivity(Intent intent, int flags) {
                    if (intent.getPackage() != null
                            && intent.getPackage().equals(OTHER_BROWSER_PACKAGE)) {
                        if (mResolveToNonBrowserPackage) {
                            return newResolveInfo(NON_BROWSER_PACKAGE);
                        }
                        return newResolveInfo(OTHER_BROWSER_PACKAGE);
                    }

                    // Behave as though play store is not installed - this matches bot emulator
                    // images.
                    if (targetsPlay(intent)) return null;

                    return TestContext.super.getPackageManager().resolveActivity(intent, flags);
                }
            };
        }
    }

    private ActivityMonitor mActivityMonitor;
    private EmbeddedTestServer mTestServer;
    private Context mContextToRestore;
    private TestContext mTestContext;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        mContextToRestore = ContextUtils.getApplicationContext();
        mTestContext = new TestContext(mContextToRestore);
        ContextUtils.initApplicationContextForTests(mTestContext);
        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        filter.addDataScheme("externalappscheme");
        mActivityMonitor = InstrumentationRegistry.getInstrumentation().addMonitor(
                filter, new Instrumentation.ActivityResult(Activity.RESULT_OK, null), true);
        mTestServer = mActivityTestRule.getTestServer();
    }

    @After
    public void tearDown() {
        if (mContextToRestore != null) {
            ContextUtils.initApplicationContextForTests(mContextToRestore);
        }
    }

    private Origin createExampleOrigin() {
        org.chromium.url.internal.mojom.Origin origin = new org.chromium.url.internal.mojom.Origin();
        origin.scheme = "https";
        origin.host = "example.com";
        origin.port = 80;
        return new Origin(origin);
    }

    private Intent getCustomTabFromChromeIntent(final String url, final boolean markFromChrome) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            intent = LaunchIntentDispatcher.createCustomTabActivityIntent(
                    InstrumentationRegistry.getTargetContext(), intent);
            IntentUtils.addTrustedIntentExtras(intent);
            return intent;
        });
    }

    private @OverrideUrlLoadingResultType int loadUrlAndWaitForIntentUrl(
            final String url, boolean needClick, boolean shouldLaunchExternalIntent) {
        return loadUrlAndWaitForIntentUrl(
                url, needClick, false, shouldLaunchExternalIntent, url, true);
    }

    private @OverrideUrlLoadingResultType int loadUrlAndWaitForIntentUrl(final String url,
            boolean shouldLaunchExternalIntent, String expectedFinalUrl,
            @PageTransition int transition) {
        return loadUrlAndWaitForIntentUrl(url, false, false, shouldLaunchExternalIntent,
                expectedFinalUrl, true, null, transition);
    }

    private @OverrideUrlLoadingResultType int loadUrlAndWaitForIntentUrl(final String url,
            boolean needClick, boolean createsNewTab, final boolean shouldLaunchExternalIntent,
            final String expectedFinalUrl, final boolean shouldFailNavigation) {
        return loadUrlAndWaitForIntentUrl(url, needClick, createsNewTab, shouldLaunchExternalIntent,
                expectedFinalUrl, shouldFailNavigation, null, PageTransition.LINK);
    }

    private @OverrideUrlLoadingResultType int loadUrlAndWaitForIntentUrl(final String url,
            boolean needClick, boolean createsNewTab, final boolean shouldLaunchExternalIntent,
            final String expectedFinalUrl, final boolean shouldFailNavigation, String clickTargetId,
            @PageTransition int transition) {
        final CallbackHelper finishCallback = new CallbackHelper();
        final CallbackHelper failCallback = new CallbackHelper();
        final CallbackHelper destroyedCallback = new CallbackHelper();
        final CallbackHelper newTabCallback = new CallbackHelper();

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        final Tab[] latestTabHolder = new Tab[1];
        final InterceptNavigationDelegateImpl[] latestDelegateHolder =
                new InterceptNavigationDelegateImpl[1];

        AtomicInteger lastResultValue = new AtomicInteger();

        latestTabHolder[0] = tab;
        latestDelegateHolder[0] = getInterceptNavigationDelegate(tab);

        Callback<Pair<GURL, OverrideUrlLoadingResult>> resultCallback =
                (Pair<GURL, OverrideUrlLoadingResult> result) -> {
            if (result.first.getSpec().equals(url)) return;
            // Ignore the NO_OVERRIDE that comes asynchronously after clobbering the tab.
            if (lastResultValue.get() == OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB
                    && result.second.getResultType() == OverrideUrlLoadingResultType.NO_OVERRIDE) {
                return;
            }
            lastResultValue.set(result.second.getResultType());
        };

        latestDelegateHolder[0].setResultCallbackForTesting(resultCallback);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.addObserver(new TestTabObserver(finishCallback, failCallback, destroyedCallback));

            TabModelSelectorObserver selectorObserver = new TabModelSelectorObserver() {
                @Override
                public void onNewTabCreated(Tab newTab, @TabCreationState int creationState) {
                    Assert.assertTrue(createsNewTab);
                    newTabCallback.notifyCalled();
                    newTab.addObserver(
                            new TestTabObserver(finishCallback, failCallback, destroyedCallback));
                    latestTabHolder[0] = newTab;
                    latestDelegateHolder[0].setResultCallbackForTesting(null);
                    latestDelegateHolder[0] = getInterceptNavigationDelegate(newTab);
                    latestDelegateHolder[0].setResultCallbackForTesting(resultCallback);

                    TestChildFrameNavigationObserver.createAndAttachToNativeWebContents(
                            newTab.getWebContents(), failCallback);
                }
            };
            mActivityTestRule.getActivity().getTabModelSelector().addObserver(selectorObserver);

            TestChildFrameNavigationObserver.createAndAttachToNativeWebContents(
                    tab.getWebContents(), failCallback);
        });

        LoadUrlParams params = new LoadUrlParams(url, transition);
        if (transition == PageTransition.LINK || transition == PageTransition.FORM_SUBMIT) {
            params.setIsRendererInitiated(true);
            params.setInitiatorOrigin(createExampleOrigin());
        }
        TestThreadUtils.runOnUiThreadBlocking(() -> { tab.loadUrl(params); });

        if (finishCallback.getCallCount() == 0) {
            try {
                finishCallback.waitForCallback(0, 1, 20, TimeUnit.SECONDS);
            } catch (TimeoutException ex) {
                Assert.fail();
                return OverrideUrlLoadingResultType.NO_OVERRIDE;
            }
        }

        if (needClick) {
            if (clickTargetId == null) {
                TouchCommon.singleClickView(tab.getView());
            } else {
                try {
                    DOMUtils.clickNode(mActivityTestRule.getWebContents(), clickTargetId);
                } catch (TimeoutException e) {
                    Assert.fail("Failed to click on the target node.");
                    return OverrideUrlLoadingResultType.NO_OVERRIDE;
                }
            }
        }

        if (shouldFailNavigation) {
            try {
                failCallback.waitForCallback(0, 1, 20, TimeUnit.SECONDS);
            } catch (TimeoutException ex) {
                Assert.fail("Haven't received navigation failure of intents.");
                return OverrideUrlLoadingResultType.NO_OVERRIDE;
            }
        }

        if (createsNewTab) {
            try {
                newTabCallback.waitForCallback(0, 1, 20, TimeUnit.SECONDS);
            } catch (TimeoutException ex) {
                Assert.fail("New Tab was not created.");
            }

            if (shouldLaunchExternalIntent) {
                try {
                    destroyedCallback.waitForCallback(0, 1, 20, TimeUnit.SECONDS);
                } catch (TimeoutException ex) {
                    Assert.fail("Intercepted new tab wasn't destroyed.");
                    return OverrideUrlLoadingResultType.NO_OVERRIDE;
                }
            }
        }

        boolean hasFallbackUrl =
                expectedFinalUrl != null && !TextUtils.equals(url, expectedFinalUrl);

        if (hasFallbackUrl) {
            if (finishCallback.getCallCount() == 1) {
                try {
                    finishCallback.waitForCallback(1, 1, 20, TimeUnit.SECONDS);
                } catch (TimeoutException ex) {
                    Assert.fail("Fallback URL is not loaded");
                    return OverrideUrlLoadingResultType.NO_OVERRIDE;
                }
            }
        }

        // For sub frames, the |loadFailCallback| run through different threads
        // from the ExternalNavigationHandler. As a result, there is no guarantee
        // when url override result would come.
        CriteriaHelper.pollUiThread(() -> {
            // Note that we do not distinguish between OVERRIDE_WITH_CLOBBERING_TAB
            // and NO_OVERRIDE since tab clobbering will eventually lead to NO_OVERRIDE.
            // in the tab. Rather, we check the final URL to distinguish between
            // fallback and normal navigation. See crbug.com/487364 for more.
            Tab latestTab = latestTabHolder[0];
            InterceptNavigationDelegateImpl delegate = latestDelegateHolder[0];
            if (shouldLaunchExternalIntent) {
                Criteria.checkThat(lastResultValue.get(),
                        Matchers.is(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT));
            } else {
                Criteria.checkThat(lastResultValue.get(),
                        Matchers.not(OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT));
            }
            if (expectedFinalUrl == null) return;
            Criteria.checkThat(latestTab.getUrl().getSpec(), Matchers.is(expectedFinalUrl));
        });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    mActivityMonitor.getHits(), Matchers.is(shouldLaunchExternalIntent ? 1 : 0));
            boolean finishesTwice = hasFallbackUrl || (shouldLaunchExternalIntent && !needClick);
            Criteria.checkThat(finishCallback.getCallCount(), Matchers.is(finishesTwice ? 2 : 1));
        });
        Assert.assertEquals(shouldFailNavigation ? 1 : 0, failCallback.getCallCount());

        return lastResultValue.get();
    }

    private static InterceptNavigationDelegateImpl getInterceptNavigationDelegate(Tab tab) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> InterceptNavigationDelegateTabHelper.get(tab));
    }

    private PropertyModel getCurrentExternalNavigationMessage() throws Exception {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            MessageDispatcher messageDispatcher = MessageDispatcherProvider.from(
                    mActivityTestRule.getActivity().getWindowAndroid());
            List<MessageStateHandler> messages = MessagesTestHelper.getEnqueuedMessages(
                    messageDispatcher, MessageIdentifier.EXTERNAL_NAVIGATION);
            if (messages.isEmpty()) return null;
            Assert.assertEquals(1, messages.size());
            return MessagesTestHelper.getCurrentMessage(messages.get(0));
        });
    }

    private void assertMessagePresent() throws Exception {
        PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
        ApplicationInfo selfInfo = ContextUtils.getApplicationContext().getApplicationInfo();
        CharSequence selfLabel = pm.getApplicationLabel(selfInfo);

        PropertyModel message = getCurrentExternalNavigationMessage();
        Assert.assertNotNull(message);
        Assert.assertThat(message.get(MessageBannerProperties.TITLE),
                Matchers.containsString(selfLabel.toString()));
        Assert.assertThat(message.get(MessageBannerProperties.DESCRIPTION).toString(),
                Matchers.containsString(selfLabel.toString()));
        Assert.assertNotNull(message.get(MessageBannerProperties.ICON));
    }

    @Test
    @SmallTest
    public void testNavigationFromTimer() {
        mActivityTestRule.startMainActivityOnBlankPage();
        loadUrlAndWaitForIntentUrl(mTestServer.getURL(NAVIGATION_FROM_TIMEOUT_PAGE), false, false);
    }

    @Test
    @SmallTest
    public void testNavigationFromTimerInSubFrame() {
        mActivityTestRule.startMainActivityOnBlankPage();
        loadUrlAndWaitForIntentUrl(
                mTestServer.getURL(NAVIGATION_FROM_TIMEOUT_PARENT_FRAME_PAGE), false, false);
    }

    @Test
    @SmallTest
    public void testNavigationFromUserGesture() {
        mActivityTestRule.startMainActivityOnBlankPage();
        loadUrlAndWaitForIntentUrl(
                mTestServer.getURL(NAVIGATION_FROM_USER_GESTURE_PAGE), true, true);
    }

    @Test
    @SmallTest
    public void testNavigationFromUserGestureInSubFrame() {
        mActivityTestRule.startMainActivityOnBlankPage();
        loadUrlAndWaitForIntentUrl(
                mTestServer.getURL(NAVIGATION_FROM_USER_GESTURE_PARENT_FRAME_PAGE), true, true);
    }

    @Test
    @SmallTest
    public void testNavigationFromXHRCallback() {
        mActivityTestRule.startMainActivityOnBlankPage();
        loadUrlAndWaitForIntentUrl(
                mTestServer.getURL(NAVIGATION_FROM_XHR_CALLBACK_PAGE), true, true);
    }

    @Test
    @SmallTest
    public void testNavigationFromXHRCallbackInSubFrame() {
        mActivityTestRule.startMainActivityOnBlankPage();
        loadUrlAndWaitForIntentUrl(
                mTestServer.getURL(NAVIGATION_FROM_XHR_CALLBACK_PARENT_FRAME_PAGE), true, true);
    }

    @Test
    @SmallTest
    public void testNavigationFromXHRCallbackAndShortTimeout() {
        mActivityTestRule.startMainActivityOnBlankPage();
        loadUrlAndWaitForIntentUrl(
                mTestServer.getURL(NAVIGATION_FROM_XHR_CALLBACK_AND_SHORT_TIMEOUT_PAGE), true,
                true);
    }

    @Test
    @SmallTest
    public void testNavigationFromXHRCallbackAndLongTimeout() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> RedirectHandlerTabHelper.swapHandlerFor(tab, mSpyRedirectHandler));
        // This is a little fragile to code changes, but better than waiting 15 real seconds.
        Mockito.doReturn(SystemClock.elapsedRealtime()) // Initial Navigation create
                .doReturn(SystemClock.elapsedRealtime()) // Initial Navigation shouldOverride
                .doReturn(SystemClock.elapsedRealtime()) // XHR Navigation create
                .doReturn(SystemClock.elapsedRealtime()) // XHR callback navigation create
                .doReturn(SystemClock.elapsedRealtime()
                        + RedirectHandler.NAVIGATION_CHAIN_TIMEOUT_MILLIS + 1) // xhr callback
                .when(mSpyRedirectHandler)
                .currentRealtime();

        @OverrideUrlLoadingResultType
        int result = loadUrlAndWaitForIntentUrl(
                mTestServer.getURL(NAVIGATION_FROM_XHR_CALLBACK_AND_SHORT_TIMEOUT_PAGE), true,
                false);

        Assert.assertEquals(OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION, result);

        assertMessagePresent();
    }

    @Test
    @SmallTest
    public void testNavigationWithFallbackURL() {
        mActivityTestRule.startMainActivityOnBlankPage();
        String fallbackUrl = mTestServer.getURL(FALLBACK_LANDING_PATH);
        String originalUrl = mTestServer.getURL(NAVIGATION_WITH_FALLBACK_URL_PAGE + "?replace_text="
                + Base64.encodeToString(
                        ApiCompatibilityUtils.getBytesUtf8("PARAM_FALLBACK_URL"), Base64.URL_SAFE)
                + ":"
                + Base64.encodeToString(
                        ApiCompatibilityUtils.getBytesUtf8(fallbackUrl), Base64.URL_SAFE));
        loadUrlAndWaitForIntentUrl(originalUrl, true, false, false, fallbackUrl, true);
    }

    @Test
    @SmallTest
    public void testNavigationWithFallbackURLInSubFrame() {
        mActivityTestRule.startMainActivityOnBlankPage();
        // The replace_text parameters for NAVIGATION_WITH_FALLBACK_URL_PAGE, which is loaded in
        // the iframe in NAVIGATION_WITH_FALLBACK_URL_PARENT_FRAME_PAGE, have to go through the
        // embedded test server twice and, as such, have to be base64-encoded twice.
        String fallbackUrl = mTestServer.getURL(FALLBACK_LANDING_PATH);
        byte[] paramBase64Name = ApiCompatibilityUtils.getBytesUtf8("PARAM_BASE64_NAME");
        byte[] base64ParamFallbackUrl = Base64.encode(
                ApiCompatibilityUtils.getBytesUtf8("PARAM_FALLBACK_URL"), Base64.URL_SAFE);
        byte[] paramBase64Value = ApiCompatibilityUtils.getBytesUtf8("PARAM_BASE64_VALUE");
        byte[] base64FallbackUrl =
                Base64.encode(ApiCompatibilityUtils.getBytesUtf8(fallbackUrl), Base64.URL_SAFE);

        String originalUrl = mTestServer.getURL(NAVIGATION_WITH_FALLBACK_URL_PARENT_FRAME_PAGE
                + "?replace_text=" + Base64.encodeToString(paramBase64Name, Base64.URL_SAFE) + ":"
                + Base64.encodeToString(base64ParamFallbackUrl, Base64.URL_SAFE)
                + "&replace_text=" + Base64.encodeToString(paramBase64Value, Base64.URL_SAFE) + ":"
                + Base64.encodeToString(base64FallbackUrl, Base64.URL_SAFE));

        // Fallback URL from a subframe will not trigger main or sub frame navigation.
        @OverrideUrlLoadingResultType
        int result = loadUrlAndWaitForIntentUrl(originalUrl, true, false);
        Assert.assertEquals(OverrideUrlLoadingResultType.NO_OVERRIDE, result);
    }

    @Test
    @SmallTest
    public void testOpenWindowFromUserGesture() {
        mActivityTestRule.startMainActivityOnBlankPage();
        loadUrlAndWaitForIntentUrl(mTestServer.getURL(OPEN_WINDOW_FROM_USER_GESTURE_PAGE), true,
                true, true, null, true);
    }

    @Test
    @SmallTest
    public void testOpenWindowFromLinkUserGesture() {
        mActivityTestRule.startMainActivityOnBlankPage();
        loadUrlAndWaitForIntentUrl(mTestServer.getURL(OPEN_WINDOW_FROM_LINK_USER_GESTURE_PAGE),
                true, true, true, null, true, "link", PageTransition.LINK);
    }

    @Test
    @SmallTest
    public void testOpenWindowFromSvgUserGesture() {
        mActivityTestRule.startMainActivityOnBlankPage();
        loadUrlAndWaitForIntentUrl(mTestServer.getURL(OPEN_WINDOW_FROM_SVG_USER_GESTURE_PAGE), true,
                true, true, null, true, "link", PageTransition.LINK);
    }

    @Test
    @SmallTest
    public void testRedirectionFromIntentColdNoTask() throws Exception {
        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent(Intent.ACTION_VIEW,
                Uri.parse(mTestServer.getURL(NAVIGATION_FROM_JAVA_REDIRECTION_PAGE)));
        intent.setClassName(context, ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        ChromeTabbedActivity activity = ApplicationTestUtils.waitForActivityWithClass(
                ChromeTabbedActivity.class, Stage.CREATED, () -> context.startActivity(intent));
        mActivityTestRule.setActivity(activity);

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1));
        }, 10000L, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        ApplicationTestUtils.waitForActivityState(activity, Stage.DESTROYED);
    }

    @Test
    @SmallTest
    public void testRedirectionFromIntentColdWithTask() throws Exception {
        // Set up task with finished ChromeActivity.
        Context context = ContextUtils.getApplicationContext();
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.getActivity().finish();
        ApplicationTestUtils.waitForActivityState(mActivityTestRule.getActivity(), Stage.DESTROYED);

        // Fire intent into existing task.
        Intent intent = new Intent(Intent.ACTION_VIEW,
                Uri.parse(mTestServer.getURL(NAVIGATION_FROM_JAVA_REDIRECTION_PAGE)));
        intent.setClassName(context, ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        AsyncInitializationActivity.interceptMoveTaskToBackForTesting();
        ChromeTabbedActivity activity = ApplicationTestUtils.waitForActivityWithClass(
                ChromeTabbedActivity.class, Stage.CREATED, () -> context.startActivity(intent));
        mActivityTestRule.setActivity(activity);

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1));
        }, 10000L, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        CriteriaHelper.pollUiThread(
                () -> AsyncInitializationActivity.wasMoveTaskToBackInterceptedForTesting());
    }

    @Test
    @SmallTest
    public void testRedirectionFromIntentWarm() throws Exception {
        Context context = ContextUtils.getApplicationContext();
        mActivityTestRule.startMainActivityOnBlankPage();

        Intent intent = new Intent(Intent.ACTION_VIEW,
                Uri.parse(mTestServer.getURL(NAVIGATION_FROM_JAVA_REDIRECTION_PAGE)));
        intent.setClassName(context, ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        AsyncInitializationActivity.interceptMoveTaskToBackForTesting();
        context.startActivity(intent);

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1));
        }, 10000L, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        CriteriaHelper.pollUiThread(
                () -> AsyncInitializationActivity.wasMoveTaskToBackInterceptedForTesting());
    }

    @Test
    @LargeTest
    public void testCCTRedirectFromIntentUriStaysInChrome_InIncognito() throws TimeoutException {
        mActivityTestRule.startMainActivityOnBlankPage();
        // This will make the mActivityTestRule.getActivity().getActivityTab() used in the method
        // loadUrlAndWaitForIntentUrl to return an incognito tab instead.
        mActivityTestRule.loadUrlInNewTab("chrome://about/", /**incognito**/ true);

        String fallbackUrl = mTestServer.getURL(FALLBACK_LANDING_PATH);
        String fallbackUrlWithoutScheme = fallbackUrl.replace("https://", "");
        String originalUrl = mTestServer.getURL(NAVIGATION_TO_CCT_FROM_INTENT_URI + "?replace_text="
                + Base64.encodeToString(
                        ApiCompatibilityUtils.getBytesUtf8("PARAM_FALLBACK_URL"), Base64.URL_SAFE)
                + ":"
                + Base64.encodeToString(
                        ApiCompatibilityUtils.getBytesUtf8(fallbackUrlWithoutScheme),
                        Base64.URL_SAFE));

        loadUrlAndWaitForIntentUrl(originalUrl, true, false, false, fallbackUrl, true);
    }

    @Test
    @LargeTest
    public void testIntentURIWithFileSchemeDoesNothing() throws TimeoutException {
        mActivityTestRule.startMainActivityOnBlankPage();
        String originalUrl = mTestServer.getURL(NAVIGATION_TO_FILE_SCHEME_FROM_INTENT_URI);
        loadUrlAndWaitForIntentUrl(
                originalUrl, true, true, false, null, true, "scheme_file", PageTransition.LINK);
    }

    @Test
    @LargeTest
    public void testIntentURIWithMixedCaseFileSchemeDoesNothing() throws TimeoutException {
        mActivityTestRule.startMainActivityOnBlankPage();
        String originalUrl = mTestServer.getURL(NAVIGATION_TO_FILE_SCHEME_FROM_INTENT_URI);
        loadUrlAndWaitForIntentUrl(originalUrl, true, true, false, null, true,
                "scheme_mixed_case_file", PageTransition.LINK);
    }

    @Test
    @LargeTest
    public void testIntentURIWithNoSchemeDoesNothing() throws TimeoutException {
        mActivityTestRule.startMainActivityOnBlankPage();
        String originalUrl = mTestServer.getURL(NAVIGATION_TO_FILE_SCHEME_FROM_INTENT_URI);
        loadUrlAndWaitForIntentUrl(
                originalUrl, true, true, false, null, true, "null_scheme", PageTransition.LINK);
    }

    @Test
    @LargeTest
    public void testIntentURIWithEmptySchemeDoesNothing() throws TimeoutException {
        mActivityTestRule.startMainActivityOnBlankPage();
        String originalUrl = mTestServer.getURL(NAVIGATION_TO_FILE_SCHEME_FROM_INTENT_URI);
        loadUrlAndWaitForIntentUrl(
                originalUrl, true, true, false, null, true, "empty_scheme", PageTransition.LINK);
    }

    @Test
    @LargeTest
    public void testSubframeLoadCannotLaunchPlayApp() throws TimeoutException {
        mActivityTestRule.startMainActivityOnBlankPage();
        @OverrideUrlLoadingResultType
        int result = loadUrlAndWaitForIntentUrl(
                mTestServer.getURL(SUBFRAME_REDIRECT_WITH_PLAY_FALLBACK), false, false);
        Assert.assertEquals(OverrideUrlLoadingResultType.NO_OVERRIDE, result);
    }

    private void runRedirectToOtherBrowserTest(Instrumentation.ActivityResult chooserResult) {
        Context context = ContextUtils.getApplicationContext();
        String targetUrl = getRedirectToOtherBrowserUrl();
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(targetUrl));
        intent.setClassName(context, ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        IntentFilter filter = new IntentFilter(Intent.ACTION_PICK_ACTIVITY);
        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(
                        filter, chooserResult, true);

        ChromeTabbedActivity activity = ApplicationTestUtils.waitForActivityWithClass(
                ChromeTabbedActivity.class, Stage.CREATED, () -> context.startActivity(intent));
        mActivityTestRule.setActivity(activity);

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(monitor.getHits(), Matchers.is(1));
        }, 10000L, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);
    }

    private String getRedirectToOtherBrowserUrl() {
        // Strip off the "https:" for intent scheme formatting.
        String redirectUrl = mTestServer.getURL(HELLO_PAGE).substring(6);
        byte[] param = ApiCompatibilityUtils.getBytesUtf8("PARAM_URL");
        byte[] value = ApiCompatibilityUtils.getBytesUtf8(redirectUrl);
        return mTestServer.getURL(REDIRECT_TO_OTHER_BROWSER)
                + "?replace_text=" + Base64.encodeToString(param, Base64.URL_SAFE) + ":"
                + Base64.encodeToString(value, Base64.URL_SAFE);
    }

    private IntentFilter createHelloIntentFilter() {
        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addDataScheme(UrlConstants.HTTPS_SCHEME);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        filter.addDataAuthority("*", null);
        filter.addDataPath(HELLO_PAGE, PatternMatcher.PATTERN_LITERAL);
        return filter;
    }

    @Test
    @LargeTest
    public void testRedirectToOtherBrowser_ChooseSelf() throws TimeoutException {
        mTestContext.setResolveBrowserIntentToNonBrowserPackage(false);
        Intent result = new Intent(Intent.ACTION_CREATE_SHORTCUT);

        runRedirectToOtherBrowserTest(
                new Instrumentation.ActivityResult(Activity.RESULT_OK, result));

        // Wait for the target (data) URL to load in the tab.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mActivityTestRule.getActivity().getActivityTab().getUrl().getSpec(),
                    Matchers.is(mTestServer.getURL(HELLO_PAGE)));
        });
    }

    @Test
    @LargeTest
    public void testRedirectToOtherBrowser_ChooseOther() throws TimeoutException {
        mTestContext.setResolveBrowserIntentToNonBrowserPackage(false);
        IntentFilter filter = createHelloIntentFilter();
        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(filter, null, true);

        Intent result = new Intent(Intent.ACTION_VIEW);
        result.setComponent(new ComponentName(OTHER_BROWSER_PACKAGE, "activity"));

        runRedirectToOtherBrowserTest(
                new Instrumentation.ActivityResult(Activity.RESULT_OK, result));

        CriteriaHelper.pollUiThread(
                () -> { Criteria.checkThat(monitor.getHits(), Matchers.is(1)); });

        InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);
    }

    @Test
    @LargeTest
    public void testRedirectToOtherBrowser_DefaultNonBrowserPackage() throws TimeoutException {
        mTestContext.setResolveBrowserIntentToNonBrowserPackage(true);
        IntentFilter filter = createHelloIntentFilter();
        Instrumentation.ActivityMonitor viewMonitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(filter, null, true);

        Context context = ContextUtils.getApplicationContext();
        String targetUrl = getRedirectToOtherBrowserUrl();
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(targetUrl));
        intent.setClassName(context, ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        IntentFilter filter2 = new IntentFilter(Intent.ACTION_PICK_ACTIVITY);
        Instrumentation.ActivityMonitor pickActivityMonitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(filter2, null, true);

        ChromeTabbedActivity activity = ApplicationTestUtils.waitForActivityWithClass(
                ChromeTabbedActivity.class, Stage.CREATED, () -> context.startActivity(intent));
        mActivityTestRule.setActivity(activity);

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(viewMonitor.getHits(), Matchers.is(1));
        }, 10000L, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        Assert.assertEquals(0, pickActivityMonitor.getHits());
        InstrumentationRegistry.getInstrumentation().removeMonitor(pickActivityMonitor);
        InstrumentationRegistry.getInstrumentation().removeMonitor(viewMonitor);
    }

    @Test
    @LargeTest
    @Features.EnableFeatures({"BackForwardCache<Study", "BackForwardCacheNoTimeEviction"})
    @Features.DisableFeatures({"BackForwardCacheMemoryControls"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group"})
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testNoRedirectWithBFCache() throws Exception {
        final CallbackHelper finishCallback = new CallbackHelper();
        final CallbackHelper syncHelper = new CallbackHelper();
        AtomicReference<NavigationHandle> mLastNavigationHandle = new AtomicReference<>(null);
        EmptyTabObserver observer = new EmptyTabObserver() {
            @Override
            public void onDidFinishNavigationInPrimaryMainFrame(
                    Tab tab, NavigationHandle navigation) {
                int callCount = syncHelper.getCallCount();
                mLastNavigationHandle.set(navigation);
                finishCallback.notifyCalled();
                try {
                    syncHelper.waitForCallback(callCount);
                } catch (Exception e) {
                }
            }
        };
        String url = mTestServer.getURL(NAVIGATION_FROM_BFCACHE);
        mActivityTestRule.startMainActivityWithURL(url);

        // This test uses the back/forward cache, so return early if it's not enabled.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.BACK_FORWARD_CACHE)) return;

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        final RedirectHandler spyHandler = Mockito.spy(TestThreadUtils.runOnUiThreadBlocking(
                () -> RedirectHandlerTabHelper.getHandlerFor(tab)));

        InterceptNavigationDelegateImpl delegate = getInterceptNavigationDelegate(tab);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.addObserver(observer);
            RedirectHandlerTabHelper.swapHandlerFor(tab, spyHandler);
        });

        // Click link to go to second page.
        DOMUtils.clickNode(mActivityTestRule.getWebContents(), "link");
        finishCallback.waitForCallback(0);
        syncHelper.notifyCalled();

        AtomicInteger lastResultValue = new AtomicInteger();
        delegate.setResultCallbackForTesting((Pair<GURL, OverrideUrlLoadingResult> result) -> {
            if (result.first.getSpec().equals(url)) return;
            lastResultValue.set(result.second.getResultType());
        });

        // Press back to go back to first page with BFCache.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().onBackPressed());
        finishCallback.waitForCallback(1);
        Assert.assertTrue(mLastNavigationHandle.get().isPageActivation());
        // Page activations should clear the RedirectHandler so future navigations aren't part of
        // the same navigation chain.
        Mockito.verify(spyHandler, Mockito.times(1)).clear();
        syncHelper.notifyCalled();

        // Page redirects to intent: URL.
        finishCallback.waitForCallback(2);

        // With RedirectHandler state cleared, this should be treated as a navigation without a
        // user gesture, which will use a Message to ask the user if they would like to follow the
        // external navigation.
        Assert.assertEquals(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION, lastResultValue.get());
        Assert.assertTrue(mLastNavigationHandle.get().getUrl().getSpec().startsWith("intent://"));
        syncHelper.notifyCalled();

        Assert.assertNotNull(getCurrentExternalNavigationMessage());
    }

    @Test
    @LargeTest
    @Features.EnableFeatures({BlinkFeatures.PRERENDER2})
    @Features.DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testClearRedirectHandlerOnPageActivation() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        final CallbackHelper prerenderFinishCallback = new CallbackHelper();
        WebContentsObserver observer = new WebContentsObserver() {
            @Override
            public void didStopLoading(GURL url, boolean isKnownValid) {
                prerenderFinishCallback.notifyCalled();
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.getWebContents().addObserver(observer); });

        mActivityTestRule.loadUrl(mTestServer.getURL(NAVIGATION_FROM_PRERENDER));

        prerenderFinishCallback.waitForCallback(0);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            RedirectHandlerTabHelper.swapHandlerFor(tab, mRedirectHandler);
            tab.getWebContents().removeObserver(observer);
        });

        // Click page to load prerender.
        TouchCommon.singleClickView(tab.getView());

        // Page activations should clear the RedirectHandler so future navigations aren't part of
        // the same navigation chain.
        Mockito.verify(mRedirectHandler,
                       Mockito.timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .clear();
    }

    @Test
    @LargeTest
    public void testServerRedirectionFromIntent() throws Exception {
        TestWebServer webServer = TestWebServer.start();
        final String redirectTargetUrl = "intent://test/#Intent;scheme=externalappscheme;end";
        final String redirectUrl = webServer.setRedirect("/302.html", redirectTargetUrl);

        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(redirectUrl));
        intent.setClassName(context, ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        ChromeTabbedActivity activity = ApplicationTestUtils.waitForActivityWithClass(
                ChromeTabbedActivity.class, Stage.CREATED, () -> context.startActivity(intent));
        mActivityTestRule.setActivity(activity);

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1));
        }, 10000L, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        ApplicationTestUtils.waitForActivityState(activity, Stage.DESTROYED);
    }

    @Test
    @LargeTest
    @Features.EnableFeatures({"FencedFrames<Study,PrivacySandboxAdsAPIsOverride"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:implementation_type/mparch"})
    public void
    testNavigationFromFencedFrame() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        final CallbackHelper frameFinishCallback = new CallbackHelper();
        WebContentsObserver observer = new WebContentsObserver() {
            @Override
            public void didStopLoading(GURL url, boolean isKnownValid) {
                frameFinishCallback.notifyCalled();
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.getWebContents().addObserver(observer); });

        try {
            // Note for posterity: This depends on
            // navigation_from_user_gesture.html.mock-http-headers to work.
            mActivityTestRule.loadUrl(mTestServer.getURL(NAVIGATION_FROM_FENCED_FRAME));

            frameFinishCallback.waitForCallback(0);
        } finally {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> { tab.getWebContents().removeObserver(observer); });
        }

        // Click page to launch app. There's no easy way to know when an out of process subframe is
        // ready to receive input, even if the document is loaded and javascript runs. If the click
        // fails the first time, try a second time.
        try {
            TouchCommon.singleClickView(tab.getView());

            CriteriaHelper.pollUiThread(
                    () -> { Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1)); });
        } catch (Throwable e) {
            TouchCommon.singleClickView(tab.getView());

            CriteriaHelper.pollUiThread(
                    () -> { Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1)); });
        }
    }

    @Test
    @Feature("CustomTabFromChrome")
    @LargeTest
    public void testIntentWithRedirectToApp() {
        final String redirectUrl = "https://example.com/path";
        final String initialUrl =
                mTestServer.getURL("/chrome/test/data/android/redirect/js_redirect.html"
                        + "?replace_text="
                        + Base64.encodeToString(
                                ApiCompatibilityUtils.getBytesUtf8("PARAM_URL"), Base64.URL_SAFE)
                        + ":"
                        + Base64.encodeToString(
                                ApiCompatibilityUtils.getBytesUtf8(redirectUrl), Base64.URL_SAFE));

        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        filter.addDataAuthority("example.com", null);
        filter.addDataScheme("https");
        ActivityMonitor monitor = InstrumentationRegistry.getInstrumentation().addMonitor(
                filter, new Instrumentation.ActivityResult(Activity.RESULT_OK, null), true);
        mTestContext.setIntentFilterForHost("example.com", filter);

        mCustomTabActivityRule.launchActivity(getCustomTabFromChromeIntent(initialUrl, true));

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(monitor.getHits(), Matchers.is(1));
        }, 10000L, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @LargeTest
    public void testExternalNavigationMessage() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        GURL url = new GURL(mTestServer.getURL(NAVIGATION_FROM_LONG_TIMEOUT));
        @OverrideUrlLoadingResultType
        int result = loadUrlAndWaitForIntentUrl(url.getSpec(), true, false);

        Assert.assertEquals(OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION, result);

        assertMessagePresent();
    }

    @Test
    @LargeTest
    public void testRedirectFromBookmark() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        String url = mTestServer.getURL(NAVIGATION_FROM_TIMEOUT_PAGE);
        @OverrideUrlLoadingResultType
        int result = loadUrlAndWaitForIntentUrl(url, false, null, PageTransition.AUTO_BOOKMARK);
        Assert.assertEquals(OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION, result);
        assertMessagePresent();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TextView button = mActivityTestRule.getActivity().findViewById(
                    org.chromium.components.messages.R.id.message_primary_button);
            button.performClick();
        });
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1));
            Criteria.checkThat(mActivityTestRule.getActivity().getActivityTab().getUrl().getSpec(),
                    Matchers.is("about:blank"));
        });
    }

    @Test
    @LargeTest
    public void testRedirectFromBookmarkWithFallback() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        String fallbackUrl = mTestServer.getURL(FALLBACK_LANDING_PATH);
        String originalUrl = mTestServer.getURL(NAVIGATION_FROM_TIMEOUT_WITH_FALLBACK_PAGE
                + "?replace_text="
                + Base64.encodeToString(
                        ApiCompatibilityUtils.getBytesUtf8("PARAM_FALLBACK_URL"), Base64.URL_SAFE)
                + ":"
                + Base64.encodeToString(
                        ApiCompatibilityUtils.getBytesUtf8(fallbackUrl), Base64.URL_SAFE));

        @OverrideUrlLoadingResultType
        int result = loadUrlAndWaitForIntentUrl(
                originalUrl, false, fallbackUrl, PageTransition.AUTO_BOOKMARK);
        Assert.assertEquals(OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB, result);
        Assert.assertNull(getCurrentExternalNavigationMessage());
    }

    @Test
    @LargeTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_PREFETCH_DELAY_SHOW_ON_START})
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testRedirectFromCCTSpeculation() throws Exception {
        final String url = mTestServer.getURL(NAVIGATION_FROM_PAGE_SHOW);
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = ContextUtils.getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, url);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));

        connection.setCanUseHiddenTabForSession(token, true);
        Assert.assertTrue(connection.mayLaunchUrl(token, Uri.parse(url), null, null));
        CustomTabsTestUtils.ensureCompletedSpeculationForUrl(connection, url);

        // Can't wait for Activity startup as we close so fast the polling is flaky.
        mCustomTabActivityRule.launchActivity(intent);
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1));
        }, 10000L, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }
}
