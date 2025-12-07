// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static com.google.common.truth.Truth.assertThat;

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
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.PatternMatcher;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Base64;
import android.util.Pair;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.Espresso;
import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matchers;
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

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Holder;
import org.chromium.base.IntentUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.PackageManagerWrapper;
import org.chromium.base.test.util.Restriction;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.InterceptNavigationDelegateClientImpl;
import org.chromium.chrome.browser.tab.InterceptNavigationDelegateTabHelper;
import org.chromium.chrome.browser.tab.RedirectHandlerTabHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tabmodel.TabModelJniBridge;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.modaldialog.ModalDialogView;
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
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.FencedFrameUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.NetError;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/** Test suite for verifying the behavior of various URL overriding actions. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(crbug.com/423465927): Explore a better approach to make the
// existing tests run with the prewarm feature enabled.
@DisableFeatures({ChromeFeatureList.CCT_DESTROY_TAB_WHEN_MODEL_IS_EMPTY, "Prewarm"})
public class UrlOverridingTest {
    @Rule
    public FreshCtaTransitTestRule mTabbedActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule public CustomTabActivityTestRule mCustomTabActivityRule = new CustomTabActivityTestRule();

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
    private static final String NAVIGATION_FROM_XHR_CALLBACK_AND_LOST_ACTIVATION_PAGE =
            BASE_PATH + "navigation_from_xhr_callback_lost_activation.html";
    private static final String NAVIGATION_WITH_FALLBACK_URL_PAGE =
            BASE_PATH + "navigation_with_fallback_url.html";
    private static final String FALLBACK_LANDING_PATH = BASE_PATH + "hello.html";
    private static final String OPEN_WINDOW_FROM_USER_GESTURE_PAGE =
            BASE_PATH + "open_window_from_user_gesture.html";
    private static final String NAVIGATION_FROM_TARGET_SELF_LINK =
            BASE_PATH + "navigation_from_target_self.html";
    private static final String NAVIGATION_FROM_TARGET_BLANK_LINK =
            BASE_PATH + "navigation_from_target_blank.html";
    private static final String NAVIGATION_FROM_TARGET_BLANK_REL_OPENER_LINK =
            BASE_PATH + "navigation_from_target_blank_rel_opener.html";
    private static final String OPEN_WINDOW_FROM_LINK_USER_GESTURE_PAGE =
            BASE_PATH + "open_window_from_link_user_gesture.html";
    private static final String OPEN_WINDOW_FROM_SVG_USER_GESTURE_PAGE =
            BASE_PATH + "open_window_from_svg_user_gesture.html";
    private static final String NAVIGATION_FROM_JAVA_REDIRECTION_PAGE =
            BASE_PATH + "navigation_from_java_redirection.html";
    private static final String NAVIGATION_TO_CCT_FROM_INTENT_URI =
            BASE_PATH + "navigation_to_cct_via_intent_uri.html";
    private static final String FALLBACK_URL =
            "https://play.google.com/store/apps/details?id=com.android.chrome";
    private static final String SUBFRAME_REDIRECT_WITH_PLAY_FALLBACK =
            BASE_PATH + "subframe_navigation_with_play_fallback_parent.html";
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
    private static final String SUBFRAME_NAVIGATION_PARENT =
            BASE_PATH + "subframe_navigation_parent.html";
    private static final String SUBFRAME_NAVIGATION_PARENT_SANDBOX =
            BASE_PATH + "subframe_navigation_parent_sandbox.html";
    private static final String SUBFRAME_NAVIGATION_PARENT_CSP_SANDBOX =
            BASE_PATH + "subframe_navigation_parent_csp_sandbox.html";
    private static final String SUBFRAME_NAVIGATION_CHILD =
            BASE_PATH + "subframe_navigation_child.html";
    private static final String NAVIGATION_FROM_RENAVIGATE_FRAME =
            BASE_PATH + "renavigate_frame.html";
    private static final String NAVIGATION_FROM_RENAVIGATE_FRAME_WITH_REDIRECT =
            BASE_PATH + "renavigate_frame_with_redirect.html";
    private static final String NAVIGATION_FROM_WINDOW_REDIRECT =
            BASE_PATH + "navigation_from_window_redirect.html";

    private static final String EXTERNAL_APP_URL =
            "intent://test/#Intent;scheme=externalappscheme;end;";

    private static final String OTHER_BROWSER_PACKAGE = "com.other.browser";
    private static final String TRUSTED_CCT_PACKAGE = "com.trusted.cct";

    private static final String EXTERNAL_APP_SCHEME = "externalappscheme";

    private static final String INTENT_LAUNCH_FROM_TAB_CREATION =
            "Android.Intent.IntentLaunchFromTabCreation";

    @IntDef({NavigationType.SELF, NavigationType.BLANK, NavigationType.TOP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface NavigationType {
        int SELF = 0;
        int BLANK = 1;
        int TOP = 2;
    }

    @IntDef({SandboxType.NONE, SandboxType.FRAME, SandboxType.CSP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SandboxType {
        int NONE = 0;
        int FRAME = 1;
        int CSP = 2;
    }

    @Mock private RedirectHandler mRedirectHandler;

    @Spy private RedirectHandler mSpyRedirectHandler;

    private static class TestTabObserver extends EmptyTabObserver {
        private final CallbackHelper mFinishCallback;
        private final CallbackHelper mDestroyedCallback;
        private final CallbackHelper mFailCallback;
        private final CallbackHelper mLoadCallback;

        TestTabObserver(
                CallbackHelper finishCallback,
                CallbackHelper destroyedCallback,
                CallbackHelper failCallback,
                CallbackHelper loadCallback) {
            mFinishCallback = finishCallback;
            mDestroyedCallback = destroyedCallback;
            mFailCallback = failCallback;
            mLoadCallback = loadCallback;
        }

        @Override
        public void onPageLoadStarted(Tab tab, GURL url) {
            mLoadCallback.notifyCalled();
        }

        @Override
        public void onPageLoadFinished(Tab tab, GURL url) {
            mFinishCallback.notifyCalled();
        }

        @Override
        public void onPageLoadFailed(Tab tab, @NetError int errorCode) {
            mFailCallback.notifyCalled();
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
        private boolean mResolveToTrustedCaller;
        private final String mNonBrowserPackageName;
        private String mHostToMatch;
        private String mSchemeToMatch;
        private IntentFilter mFilterForHostMatch;
        private IntentFilter mFilterForSchemeMatch;

        TestContext(Context baseContext, String nonBrowserPackageName) {
            super(baseContext);
            mNonBrowserPackageName = nonBrowserPackageName;
        }

        public void setResolveBrowserIntentToNonBrowserPackage(boolean toNonBrowser) {
            mResolveToNonBrowserPackage = toNonBrowser;
        }

        public void setResolveToTrustedCaller(boolean toTrustedCaller) {
            mResolveToTrustedCaller = toTrustedCaller;
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

        private void setIntentFilterForScheme(String scheme, IntentFilter filter) {
            mSchemeToMatch = scheme;
            mFilterForSchemeMatch = filter;
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

                    String targetPackage =
                            mResolveToTrustedCaller ? TRUSTED_CCT_PACKAGE : mNonBrowserPackageName;

                    // Behave as though play store is not installed - this matches bot emulator
                    // images.
                    if (targetsPlay(intent)) return null;

                    if (mHostToMatch != null
                            && intent.getData() != null
                            && intent.getData().getHost().equals(mHostToMatch)) {
                        ResolveInfo info = newResolveInfo(targetPackage);
                        info.filter = mFilterForHostMatch;
                        return Arrays.asList(info);
                    }

                    if (mSchemeToMatch != null
                            && intent.getScheme() != null
                            && intent.getScheme().equals(mSchemeToMatch)) {
                        ResolveInfo info = newResolveInfo(targetPackage);
                        info.filter = mFilterForSchemeMatch;
                        return Arrays.asList(info);
                    }

                    return TestContext.super
                            .getPackageManager()
                            .queryIntentActivities(intent, flags);
                }

                @Override
                public ResolveInfo resolveActivity(Intent intent, int flags) {
                    if (intent.getPackage() != null
                            && intent.getPackage().equals(OTHER_BROWSER_PACKAGE)) {
                        if (mResolveToNonBrowserPackage) {
                            return newResolveInfo(mNonBrowserPackageName);
                        }
                        return newResolveInfo(OTHER_BROWSER_PACKAGE);
                    }

                    String targetPackage =
                            mResolveToTrustedCaller ? TRUSTED_CCT_PACKAGE : mNonBrowserPackageName;

                    if (mSchemeToMatch != null
                            && intent.getScheme() != null
                            && intent.getScheme().equals(mSchemeToMatch)) {
                        ResolveInfo info = newResolveInfo(targetPackage);
                        info.filter = mFilterForSchemeMatch;
                        return info;
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
    private TestContext mTestContext;
    private String mNonBrowserPackageName;

    @Before
    public void setUp() throws Exception {
        mTabbedActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        mNonBrowserPackageName = getNonBrowserPackageName();
        mTestContext =
                new TestContext(ContextUtils.getApplicationContext(), mNonBrowserPackageName);
        ContextUtils.initApplicationContextForTests(mTestContext);
        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        filter.addDataScheme(EXTERNAL_APP_SCHEME);
        mActivityMonitor =
                InstrumentationRegistry.getInstrumentation()
                        .addMonitor(
                                filter,
                                new Instrumentation.ActivityResult(Activity.RESULT_OK, null),
                                true);
        mTestServer = mTabbedActivityTestRule.getTestServer();
        mTestContext.setIntentFilterForScheme(EXTERNAL_APP_SCHEME, filter);
        ModalDialogView.disableButtonTapProtectionForTesting();
    }

    private Origin createExampleOrigin() {
        org.chromium.url.internal.mojom.Origin origin =
                new org.chromium.url.internal.mojom.Origin();
        origin.scheme = "https";
        origin.host = "example.com";
        origin.port = 80;
        return new Origin(origin);
    }

    private Intent getCustomTabFromChromeIntent(final String url, final boolean targetChrome) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    if (targetChrome) {
                        intent.setPackage(ContextUtils.getApplicationContext().getPackageName());
                    }
                    intent =
                            LaunchIntentDispatcher.createCustomTabActivityIntent(
                                    ApplicationProvider.getApplicationContext(), intent);
                    IntentUtils.addTrustedIntentExtras(intent);
                    return intent;
                });
    }

    private static class TestParams {
        public final String url;
        public final boolean needClick;
        public final boolean shouldLaunchExternalIntent;
        public boolean createsNewTab;
        public String expectedFinalUrl;
        public boolean shouldFailNavigation = true;
        public String clickTargetId;
        public @PageTransition int transition = PageTransition.LINK;
        public boolean willNavigateTwice;
        public boolean willLoadSubframe;

        TestParams(String url, boolean needClick, boolean shouldLaunchExternalIntent) {
            this.url = url;
            this.needClick = needClick;
            this.shouldLaunchExternalIntent = shouldLaunchExternalIntent;
            expectedFinalUrl = url;
        }
    }

    private OverrideUrlLoadingResult loadUrlAndWaitForIntentUrl(
            TestParams params, CtaPageStation sourcePage) throws Exception {
        final CallbackHelper finishCallback = new CallbackHelper();
        final CallbackHelper failCallback = new CallbackHelper();
        final CallbackHelper destroyedCallback = new CallbackHelper();
        final CallbackHelper newTabCallback = new CallbackHelper();
        final CallbackHelper loadCallback = new CallbackHelper();

        final Tab tab = sourcePage.getTab();
        final Holder<@Nullable Tab> latestTabHolder = new Holder<>(null);

        AtomicReference<OverrideUrlLoadingResult> lastResultValue = new AtomicReference<>();

        latestTabHolder.value = tab;

        Callback<Pair<GURL, OverrideUrlLoadingResult>> resultCallback =
                (Pair<GURL, OverrideUrlLoadingResult> result) -> {
                    if (result.first.getSpec().equals(params.url)) return;
                    // Ignore the NO_OVERRIDE that comes asynchronously after clobbering the tab.
                    if (lastResultValue.get() != null
                            && lastResultValue.get().getResultType()
                                    == OverrideUrlLoadingResultType.OVERRIDE_WITH_NAVIGATE_TAB
                            && result.second.getResultType()
                                    == OverrideUrlLoadingResultType.NO_OVERRIDE) {
                        return;
                    }
                    lastResultValue.set(result.second);
                };

        InterceptNavigationDelegateImpl.setResultCallbackForTesting(resultCallback);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.addObserver(
                            new TestTabObserver(
                                    finishCallback, destroyedCallback, failCallback, loadCallback));

                    TabModelSelectorObserver selectorObserver =
                            new TabModelSelectorObserver() {
                                @Override
                                public void onNewTabCreated(
                                        Tab newTab, @TabCreationState int creationState) {
                                    Assert.assertTrue(params.createsNewTab);
                                    latestTabHolder.value = newTab;
                                    newTabCallback.notifyCalled();
                                    loadCallback.notifyCalled();
                                    newTab.addObserver(
                                            new TestTabObserver(
                                                    finishCallback,
                                                    destroyedCallback,
                                                    failCallback,
                                                    loadCallback));
                                    TestChildFrameNavigationObserver
                                            .createAndAttachToNativeWebContents(
                                                    newTab.getWebContents(),
                                                    failCallback,
                                                    finishCallback,
                                                    loadCallback);
                                }
                            };
                    sourcePage.getActivity().getTabModelSelector().addObserver(selectorObserver);

                    TestChildFrameNavigationObserver.createAndAttachToNativeWebContents(
                            tab.getWebContents(), failCallback, finishCallback, loadCallback);
                });

        LoadUrlParams loadParams = new LoadUrlParams(params.url, params.transition);
        if (params.transition == PageTransition.LINK
                || params.transition == PageTransition.FORM_SUBMIT) {
            loadParams.setIsRendererInitiated(true);
            loadParams.setInitiatorOrigin(createExampleOrigin());
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.loadUrl(loadParams);
                });
        ChromeTabUtils.waitForInteractable(tab);

        int preClickFinishTarget = params.willLoadSubframe ? 2 : 1;
        if (finishCallback.getCallCount() < preClickFinishTarget) {
            finishCallback.waitForCallback(0, preClickFinishTarget, 20, TimeUnit.SECONDS);
        }

        if (params.needClick) {
            int loadCount = loadCallback.getCallCount();
            doClick(params.clickTargetId, tab);
            try {
                // Some tests have a long delay before starting the load.
                loadCallback.waitForCallback(loadCount, 1, 20, TimeUnit.SECONDS);
            } catch (TimeoutException ex) {
                // Non-subframe clicks shouldn't be flaky.
                if (!params.willLoadSubframe) {
                    throw ex;
                }
                // Subframe clicks are flaky so re-try them if nothing started loading.
                doClick(params.clickTargetId, tab);
            }
        }

        if (params.willNavigateTwice && finishCallback.getCallCount() < preClickFinishTarget + 1) {
            finishCallback.waitForCallback(preClickFinishTarget, 1, 20, TimeUnit.SECONDS);
        }

        if (params.createsNewTab) {
            newTabCallback.waitForCallback("New Tab was not created.", 0, 1, 20, TimeUnit.SECONDS);
        }

        if (params.createsNewTab && !params.shouldLaunchExternalIntent) {
            ChromeTabUtils.waitForInteractable(tab);
            // The new tab URL is sometimes empty and not strictly linked to the interactable state.
            CriteriaHelper.pollUiThread(
                    () -> {
                        Criteria.checkThat(
                                GURL.isEmptyOrInvalid(latestTabHolder.value.getUrl()),
                                Matchers.is(false));
                    });
        }

        if (params.shouldFailNavigation) {
            failCallback.waitForCallback("Navigation didn't fail.", 0, 1, 20, TimeUnit.SECONDS);
        }

        boolean hasFallbackUrl =
                params.expectedFinalUrl != null
                        && !TextUtils.equals(params.url, params.expectedFinalUrl);

        int finalFinishTarget =
                preClickFinishTarget + (params.willNavigateTwice || hasFallbackUrl ? 1 : 0);
        if (hasFallbackUrl && finishCallback.getCallCount() < finalFinishTarget) {
            finishCallback.waitForCallback(
                    "Fallback URL is not loaded", finalFinishTarget - 1, 1, 20, TimeUnit.SECONDS);
        }

        // For sub frames, the |loadFailCallback| run through different threads
        // from the ExternalNavigationHandler. As a result, there is no guarantee
        // when url override result would come.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(lastResultValue.get(), Matchers.notNullValue());
                    // Note that we do not distinguish between OVERRIDE_WITH_NAVIGATE_TAB
                    // and NO_OVERRIDE since tab clobbering will eventually lead to NO_OVERRIDE.
                    // in the tab. Rather, we check the final URL to distinguish between
                    // fallback and normal navigation. See crbug.com/487364 for more.
                    Tab latestTab = latestTabHolder.value;
                    if (params.shouldLaunchExternalIntent) {
                        Criteria.checkThat(
                                lastResultValue.get().getResultType(),
                                Matchers.is(
                                        OverrideUrlLoadingResultType
                                                .OVERRIDE_WITH_EXTERNAL_INTENT));
                    } else {
                        Criteria.checkThat(
                                lastResultValue.get().getResultType(),
                                Matchers.not(
                                        OverrideUrlLoadingResultType
                                                .OVERRIDE_WITH_EXTERNAL_INTENT));
                    }
                    if (params.expectedFinalUrl == null) return;
                    Criteria.checkThat(
                            latestTab.getUrl().getSpec(), Matchers.is(params.expectedFinalUrl));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        if (params.createsNewTab && params.shouldLaunchExternalIntent) {
            destroyedCallback.waitForCallback(
                    "Intercepted new tab wasn't destroyed.", 0, 1, 20, TimeUnit.SECONDS);
        }

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivityMonitor.getHits(),
                            Matchers.is(params.shouldLaunchExternalIntent ? 1 : 0));
                    Criteria.checkThat(
                            finishCallback.getCallCount(), Matchers.is(finalFinishTarget));
                });
        Assert.assertEquals(params.shouldFailNavigation ? 1 : 0, failCallback.getCallCount());

        return lastResultValue.get();
    }

    private void doClick(String targetId, Tab tab) throws Exception {
        if (targetId == null) {
            TouchCommon.singleClickView(tab.getView());
        } else {
            DOMUtils.clickNode(mTabbedActivityTestRule.getWebContents(), targetId);
        }
    }

    private PropertyModel getCurrentExternalNavigationMessage() throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeActivity activity = mTabbedActivityTestRule.getActivity();
                    if (activity == null) activity = mCustomTabActivityRule.getActivity();
                    MessageDispatcher messageDispatcher =
                            MessageDispatcherProvider.from(activity.getWindowAndroid());
                    List<MessageStateHandler> messages =
                            MessagesTestHelper.getEnqueuedMessages(
                                    messageDispatcher, MessageIdentifier.EXTERNAL_NAVIGATION);
                    if (messages.isEmpty()) return null;
                    Assert.assertEquals(1, messages.size());
                    return MessagesTestHelper.getCurrentMessage(messages.get(0));
                });
    }

    private void assertMessagePresent() throws Exception {
        PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
        ApplicationInfo applicationInfo = pm.getApplicationInfo(mNonBrowserPackageName, 0);
        CharSequence label = pm.getApplicationLabel(applicationInfo);

        PropertyModel message = getCurrentExternalNavigationMessage();
        Assert.assertNotNull(message);
        assertThat(
                message.get(MessageBannerProperties.TITLE),
                Matchers.containsString(label.toString()));
        assertThat(
                message.get(MessageBannerProperties.DESCRIPTION).toString(),
                Matchers.containsString(label.toString()));
        Assert.assertNotNull(message.get(MessageBannerProperties.ICON));
    }

    private String getSubframeNavigationUrl(
            String subframeTargetUrl,
            @NavigationType int navigationType,
            @SandboxType int sandboxType) {
        // The replace_text parameters for SUBFRAME_NAVIGATION_CHILD, which is loaded in
        // the iframe in SUBFRAME_NAVIGATION_PARENT, have to go through the
        // embedded test server twice and, as such, have to be base64-encoded twice.
        byte[] paramBase64Name = ApiCompatibilityUtils.getBytesUtf8("PARAM_BASE64_NAME");
        byte[] base64ParamSubframeUrl =
                Base64.encode(
                        ApiCompatibilityUtils.getBytesUtf8("PARAM_SUBFRAME_URL"), Base64.URL_SAFE);
        byte[] paramBase64Value = ApiCompatibilityUtils.getBytesUtf8("PARAM_BASE64_VALUE");
        byte[] base64SubframeUrl =
                Base64.encode(
                        ApiCompatibilityUtils.getBytesUtf8(subframeTargetUrl), Base64.URL_SAFE);

        byte[] paramNavType = ApiCompatibilityUtils.getBytesUtf8("PARAM_BLANK");
        byte[] valBlank = ApiCompatibilityUtils.getBytesUtf8("_blank");
        byte[] valTop = ApiCompatibilityUtils.getBytesUtf8("_top");

        String url = SUBFRAME_NAVIGATION_PARENT;
        if (sandboxType == SandboxType.FRAME) {
            url = SUBFRAME_NAVIGATION_PARENT_SANDBOX;
        } else if (sandboxType == SandboxType.CSP) {
            url = SUBFRAME_NAVIGATION_PARENT_CSP_SANDBOX;
        }

        String navType = "";
        if (navigationType == NavigationType.BLANK) {
            navType = Base64.encodeToString(valBlank, Base64.URL_SAFE);
        } else if (navigationType == NavigationType.TOP) {
            navType = Base64.encodeToString(valTop, Base64.URL_SAFE);
        }

        return mTestServer.getURL(
                url
                        + "?replace_text="
                        + Base64.encodeToString(paramBase64Name, Base64.URL_SAFE)
                        + ":"
                        + Base64.encodeToString(base64ParamSubframeUrl, Base64.URL_SAFE)
                        + "&replace_text="
                        + Base64.encodeToString(paramBase64Value, Base64.URL_SAFE)
                        + ":"
                        + Base64.encodeToString(base64SubframeUrl, Base64.URL_SAFE)
                        + "&replace_text="
                        + Base64.encodeToString(paramNavType, Base64.URL_SAFE)
                        + ":"
                        + navType);
    }

    private String getUrlWithParam(String url, String paramNewUrl) {
        byte[] paranName = ApiCompatibilityUtils.getBytesUtf8("PARAM_URL");
        byte[] value = ApiCompatibilityUtils.getBytesUtf8(paramNewUrl);
        return mTestServer.getURL(url)
                + "?replace_text="
                + Base64.encodeToString(paranName, Base64.URL_SAFE)
                + ":"
                + Base64.encodeToString(value, Base64.URL_SAFE);
    }

    private String getNonBrowserPackageName() {
        List<PackageInfo> packages =
                ContextUtils.getApplicationContext().getPackageManager().getInstalledPackages(0);
        if (packages == null || packages.size() == 0) {
            return "";
        }

        return packages.get(0).packageName;
    }

    private void maybeCleanupIncognitoWindow(CtaPageStation page) {
        if (page.getActivity().isIncognitoWindow()) {
            ApplicationTestUtils.finishActivity(page.getActivity());
        }
    }

    @Test
    @SmallTest
    public void testNavigationFromTimer() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        loadUrlAndWaitForIntentUrl(
                new TestParams(mTestServer.getURL(NAVIGATION_FROM_TIMEOUT_PAGE), false, false),
                ctaPage);
    }

    @Test
    @SmallTest
    public void testNavigationFromTimerInSubFrame() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        TestParams params =
                new TestParams(
                        mTestServer.getURL(NAVIGATION_FROM_TIMEOUT_PARENT_FRAME_PAGE),
                        false,
                        false);
        params.willLoadSubframe = true;
        loadUrlAndWaitForIntentUrl(params, ctaPage);
    }

    @Test
    @SmallTest
    public void testNavigationFromUserGesture() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        loadUrlAndWaitForIntentUrl(
                new TestParams(mTestServer.getURL(NAVIGATION_FROM_USER_GESTURE_PAGE), true, true),
                ctaPage);
    }

    @Test
    @SmallTest
    public void testNavigationFromUserGestureInSubFrame() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        TestParams params =
                new TestParams(
                        mTestServer.getURL(NAVIGATION_FROM_USER_GESTURE_PARENT_FRAME_PAGE),
                        true,
                        true);
        params.willLoadSubframe = true;
        loadUrlAndWaitForIntentUrl(params, ctaPage);
    }

    @Test
    @SmallTest
    public void testNavigationFromXHRCallback() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        loadUrlAndWaitForIntentUrl(
                new TestParams(mTestServer.getURL(NAVIGATION_FROM_XHR_CALLBACK_PAGE), true, true),
                ctaPage);
    }

    @Test
    @SmallTest
    public void testNavigationFromXHRCallbackInSubFrame() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        TestParams params =
                new TestParams(
                        mTestServer.getURL(NAVIGATION_FROM_XHR_CALLBACK_PARENT_FRAME_PAGE),
                        true,
                        true);
        params.willLoadSubframe = true;
        loadUrlAndWaitForIntentUrl(params, ctaPage);
    }

    @Test
    @SmallTest
    public void testNavigationFromXHRCallbackAndShortTimeout() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        loadUrlAndWaitForIntentUrl(
                new TestParams(
                        mTestServer.getURL(NAVIGATION_FROM_XHR_CALLBACK_AND_SHORT_TIMEOUT_PAGE),
                        true,
                        true),
                ctaPage);
    }

    @Test
    @SmallTest
    public void testNavigationFromXHRCallbackAndLostActivation() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        loadUrlAndWaitForIntentUrl(
                new TestParams(
                        mTestServer.getURL(NAVIGATION_FROM_XHR_CALLBACK_AND_LOST_ACTIVATION_PAGE),
                        true,
                        true),
                ctaPage);
    }

    @Test
    @SmallTest
    public void testNavigationFromXHRCallbackAndLostActivationLongTimeout() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();

        final Tab tab = mTabbedActivityTestRule.getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> RedirectHandlerTabHelper.swapHandlerForTesting(tab, mSpyRedirectHandler));
        // This is a little fragile to code changes, but better than waiting 15 real seconds.
        Mockito.doReturn(SystemClock.elapsedRealtime()) // Initial Navigation create
                .doReturn(SystemClock.elapsedRealtime()) // Initial Navigation shouldOverride
                .doReturn(SystemClock.elapsedRealtime()) // XHR Navigation create
                .doReturn(
                        SystemClock.elapsedRealtime()
                                + RedirectHandler.NAVIGATION_CHAIN_TIMEOUT_MILLIS
                                + 1) // xhr callback
                .when(mSpyRedirectHandler)
                .currentRealtime();

        OverrideUrlLoadingResult result =
                loadUrlAndWaitForIntentUrl(
                        new TestParams(
                                mTestServer.getURL(
                                        NAVIGATION_FROM_XHR_CALLBACK_AND_LOST_ACTIVATION_PAGE),
                                true,
                                false),
                        ctaPage);

        Assert.assertEquals(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION, result.getResultType());

        assertMessagePresent();
    }

    @Test
    @SmallTest
    public void testNavigationWithFallbackURL() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        String fallbackUrl = mTestServer.getURL(FALLBACK_LANDING_PATH);
        String originalUrl =
                mTestServer.getURL(
                        NAVIGATION_WITH_FALLBACK_URL_PAGE
                                + "?replace_text="
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8("PARAM_FALLBACK_URL"),
                                        Base64.URL_SAFE)
                                + ":"
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8(fallbackUrl),
                                        Base64.URL_SAFE));
        TestParams params = new TestParams(originalUrl, true, false);
        params.expectedFinalUrl = fallbackUrl;
        loadUrlAndWaitForIntentUrl(params, ctaPage);
    }

    @Test
    @SmallTest
    public void testNavigationWithFallbackURLInSubFrame() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        String fallbackUrl = mTestServer.getURL(FALLBACK_LANDING_PATH);
        String subframeUrl =
                "intent://test/#Intent;scheme=badscheme;S.browser_fallback_url="
                        + fallbackUrl
                        + ";end";
        String originalUrl =
                getSubframeNavigationUrl(subframeUrl, NavigationType.SELF, SandboxType.NONE);

        final Tab tab = mTabbedActivityTestRule.getActivityTab();

        final CallbackHelper subframeRedirect = new CallbackHelper();
        EmptyTabObserver observer =
                new EmptyTabObserver() {
                    @Override
                    public void onDidStartNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        Assert.assertEquals(originalUrl, navigation.getUrl().getSpec());
                    }

                    @Override
                    public void onDidRedirectNavigation(Tab tab, NavigationHandle navigation) {
                        Assert.assertFalse(navigation.isInPrimaryMainFrame());
                        Assert.assertEquals(fallbackUrl, navigation.getUrl().getSpec());
                        subframeRedirect.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.addObserver(observer);
                });

        // Fallback URL from a subframe will not trigger main navigation.
        TestParams params = new TestParams(originalUrl, true, false);
        params.willLoadSubframe = true;
        params.shouldFailNavigation = false;
        params.willNavigateTwice = true;
        OverrideUrlLoadingResult result = loadUrlAndWaitForIntentUrl(params, ctaPage);

        Assert.assertEquals(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_NAVIGATE_TAB, result.getResultType());
        subframeRedirect.waitForOnly();
    }

    @Test
    @SmallTest
    public void testOpenWindowFromUserGesture() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        TestParams params =
                new TestParams(mTestServer.getURL(OPEN_WINDOW_FROM_USER_GESTURE_PAGE), true, true);
        params.createsNewTab = true;
        params.expectedFinalUrl = null;
        loadUrlAndWaitForIntentUrl(params, ctaPage);
    }

    @Test
    @SmallTest
    public void testOpenWindowFromLinkUserGesture() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        TestParams params =
                new TestParams(
                        getUrlWithParam(OPEN_WINDOW_FROM_LINK_USER_GESTURE_PAGE, EXTERNAL_APP_URL),
                        true,
                        true);
        params.createsNewTab = true;
        params.expectedFinalUrl = null;
        loadUrlAndWaitForIntentUrl(params, ctaPage);
    }

    @Test
    @SmallTest
    public void testOpenWindowFromSvgUserGesture() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        TestParams params =
                new TestParams(
                        mTestServer.getURL(OPEN_WINDOW_FROM_SVG_USER_GESTURE_PAGE), true, true);
        params.createsNewTab = true;
        params.clickTargetId = "link";
        params.expectedFinalUrl = null;
        loadUrlAndWaitForIntentUrl(params, ctaPage);
    }

    @Test
    @SmallTest
    public void testRedirectionFromIntentColdNoTask() throws Exception {
        Context context = ContextUtils.getApplicationContext();
        Intent intent =
                new Intent(
                        Intent.ACTION_VIEW,
                        Uri.parse(mTestServer.getURL(NAVIGATION_FROM_JAVA_REDIRECTION_PAGE)));
        intent.setClassName(context, ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        ChromeTabbedActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.CREATED,
                        () -> context.startActivity(intent));
        mTabbedActivityTestRule.getActivityTestRule().setActivity(activity);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        ApplicationTestUtils.waitForActivityState(activity, Stage.DESTROYED);
    }

    @Test
    @SmallTest
    public void testRedirectionFromIntentColdWithTask() throws Exception {
        // Set up task with finished ChromeActivity.
        Context context = ContextUtils.getApplicationContext();
        mTabbedActivityTestRule.startOnBlankPage();
        mTabbedActivityTestRule.getActivity().finish();
        ApplicationTestUtils.waitForActivityState(
                mTabbedActivityTestRule.getActivity(), Stage.DESTROYED);

        // Fire intent into existing task.
        Intent intent =
                new Intent(
                        Intent.ACTION_VIEW,
                        Uri.parse(mTestServer.getURL(NAVIGATION_FROM_JAVA_REDIRECTION_PAGE)));
        intent.setClassName(context, ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        AsyncInitializationActivity.interceptMoveTaskToBackForTesting();
        ChromeTabbedActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.CREATED,
                        () -> context.startActivity(intent));
        mTabbedActivityTestRule.getActivityTestRule().setActivity(activity);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        CriteriaHelper.pollUiThread(
                () -> AsyncInitializationActivity.wasMoveTaskToBackInterceptedForTesting());
    }

    @Test
    @SmallTest
    public void testRedirectionFromIntentWarm() throws Exception {
        HistogramWatcher redirectWatcher =
                HistogramWatcher.newSingleRecordWatcher(INTENT_LAUNCH_FROM_TAB_CREATION, true);
        Context context = ContextUtils.getApplicationContext();
        mTabbedActivityTestRule.startOnBlankPage();

        Intent intent =
                new Intent(
                        Intent.ACTION_VIEW,
                        Uri.parse(mTestServer.getURL(NAVIGATION_FROM_JAVA_REDIRECTION_PAGE)));
        intent.setClassName(context, ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        AsyncInitializationActivity.interceptMoveTaskToBackForTesting();
        context.startActivity(intent);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        redirectWatcher.assertExpected();
        CriteriaHelper.pollUiThread(
                () -> AsyncInitializationActivity.wasMoveTaskToBackInterceptedForTesting());
    }

    @Test
    @LargeTest
    public void testCctRedirectFromIntentUriStaysInChrome_InIncognito() throws Exception {
        var initialCtaPage = mTabbedActivityTestRule.startOnBlankPage();
        // This will cause getActivityTab() in loadUrlAndWaitForIntentUrl to return an incognito tab
        // instead.
        IncognitoNewTabPageStation incognitoPage = initialCtaPage.openNewIncognitoTabOrWindowFast();

        String fallbackUrl = mTestServer.getURL(FALLBACK_LANDING_PATH);
        String fallbackUrlWithoutScheme = fallbackUrl.replace("https://", "");
        String originalUrl =
                mTestServer.getURL(
                        NAVIGATION_TO_CCT_FROM_INTENT_URI
                                + "?replace_text="
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8("PARAM_FALLBACK_URL"),
                                        Base64.URL_SAFE)
                                + ":"
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8(
                                                fallbackUrlWithoutScheme),
                                        Base64.URL_SAFE));
        TestParams params = new TestParams(originalUrl, true, false);
        params.expectedFinalUrl = fallbackUrl;
        loadUrlAndWaitForIntentUrl(params, incognitoPage);

        // Cleanup newly created incognito window when applicable.
        maybeCleanupIncognitoWindow(incognitoPage);
    }

    @Test
    @LargeTest
    public void testIntentURIWithFileSchemeDoesNothing() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        String targetUrl =
                "intent:///x.mhtml#Intent;package=org.chromium.chrome.tests;"
                        + "action=android.intent.action.VIEW;scheme=file;end;";
        String url = getUrlWithParam(OPEN_WINDOW_FROM_LINK_USER_GESTURE_PAGE, targetUrl);
        TestParams params = new TestParams(url, true, false);
        params.createsNewTab = true;
        params.expectedFinalUrl = null;
        loadUrlAndWaitForIntentUrl(params, ctaPage);
    }

    @Test
    @LargeTest
    public void testIntentURIWithMixedCaseFileSchemeDoesNothing() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        String targetUrl =
                "intent:///x.mhtml#Intent;package=org.chromium.chrome.tests;"
                        + "action=android.intent.action.VIEW;scheme=FiLe;end;";
        String url = getUrlWithParam(OPEN_WINDOW_FROM_LINK_USER_GESTURE_PAGE, targetUrl);
        TestParams params = new TestParams(url, true, false);
        params.createsNewTab = true;
        params.expectedFinalUrl = null;
        loadUrlAndWaitForIntentUrl(params, ctaPage);
    }

    @Test
    @LargeTest
    public void testIntentURIWithNoSchemeDoesNothing() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        String targetUrl =
                "intent:///x.mhtml#Intent;package=org.chromium.chrome.tests;"
                        + "action=android.intent.action.VIEW;end;";
        String url = getUrlWithParam(OPEN_WINDOW_FROM_LINK_USER_GESTURE_PAGE, targetUrl);
        TestParams params = new TestParams(url, true, false);
        params.createsNewTab = true;
        params.expectedFinalUrl = null;
        loadUrlAndWaitForIntentUrl(params, ctaPage);
    }

    @Test
    @LargeTest
    public void testIntentURIWithEmptySchemeDoesNothing() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        String targetUrl =
                "intent:///x.mhtml#Intent;package=org.chromium.chrome.tests;"
                        + "action=android.intent.action.VIEW;scheme=;end;";
        String url = getUrlWithParam(OPEN_WINDOW_FROM_LINK_USER_GESTURE_PAGE, targetUrl);
        TestParams params = new TestParams(url, true, false);
        params.createsNewTab = true;
        params.expectedFinalUrl = null;
        loadUrlAndWaitForIntentUrl(params, ctaPage);
    }

    @Test
    @LargeTest
    public void testSubframeLoadCannotLaunchPlayApp() throws Exception {
        String fallbackUrl = "https://play.google.com/store/apps/details?id=com.android.chrome";
        String mainUrl = mTestServer.getURL(SUBFRAME_REDIRECT_WITH_PLAY_FALLBACK);
        String redirectUrl = mTestServer.getURL(HELLO_PAGE);
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();

        final Tab tab = mTabbedActivityTestRule.getActivityTab();

        final CallbackHelper subframeExternalProtocol = new CallbackHelper();
        final CallbackHelper subframeRedirect = new CallbackHelper();
        EmptyTabObserver observer =
                new EmptyTabObserver() {
                    @Override
                    public void onDidStartNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        Assert.assertEquals(mainUrl, navigation.getUrl().getSpec());
                    }

                    @Override
                    public void onDidRedirectNavigation(Tab tab, NavigationHandle navigation) {
                        Assert.assertFalse(navigation.isInPrimaryMainFrame());
                        Assert.assertEquals(redirectUrl, navigation.getUrl().getSpec());
                        subframeRedirect.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.addObserver(observer);

                    InterceptNavigationDelegateClientImpl client =
                            InterceptNavigationDelegateClientImpl.createForTesting(tab);
                    InterceptNavigationDelegateImpl delegate =
                            new InterceptNavigationDelegateImpl(client) {
                                @Override
                                public GURL handleSubframeExternalProtocol(
                                        GURL escapedUrl,
                                        @PageTransition int transition,
                                        boolean hasUserGesture,
                                        Origin initiatorOrigin) {
                                    GURL target =
                                            super.handleSubframeExternalProtocol(
                                                    escapedUrl,
                                                    transition,
                                                    hasUserGesture,
                                                    initiatorOrigin);
                                    Assert.assertEquals(fallbackUrl, target.getSpec());
                                    subframeExternalProtocol.notifyCalled();
                                    // We can't actually load the play store URL in tests.
                                    return new GURL(redirectUrl);
                                }
                            };
                    client.initializeWithDelegate(delegate);
                    delegate.setExternalNavigationHandler(
                            new ExternalNavigationHandler(new ExternalNavigationDelegateImpl(tab)));
                    delegate.associateWithWebContents(tab.getWebContents());
                    InterceptNavigationDelegateTabHelper.setDelegateForTesting(tab, delegate);
                });

        TestParams params = new TestParams(mainUrl, false, false);
        params.willNavigateTwice = true;
        params.willLoadSubframe = true;
        params.shouldFailNavigation = false;
        OverrideUrlLoadingResult result = loadUrlAndWaitForIntentUrl(params, ctaPage);

        Assert.assertEquals(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_NAVIGATE_TAB, result.getResultType());
        subframeExternalProtocol.waitForOnly();
        subframeRedirect.waitForOnly();
    }

    private void runRedirectToOtherBrowserTest(Instrumentation.ActivityResult chooserResult) {
        Context context = ContextUtils.getApplicationContext();
        String targetUrl = getRedirectToOtherBrowserUrl();
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(targetUrl));
        intent.setClassName(context, ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        IntentFilter filter = new IntentFilter(Intent.ACTION_PICK_ACTIVITY);
        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation()
                        .addMonitor(filter, chooserResult, true);

        ChromeTabbedActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.CREATED,
                        () -> context.startActivity(intent));
        mTabbedActivityTestRule.getActivityTestRule().setActivity(activity);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(monitor.getHits(), Matchers.is(1));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);
    }

    private String getRedirectToOtherBrowserUrl() {
        // Strip off the "https:" for intent scheme formatting.
        String redirectUrl = mTestServer.getURL(HELLO_PAGE).substring(6);
        byte[] param = ApiCompatibilityUtils.getBytesUtf8("PARAM_URL");
        byte[] value = ApiCompatibilityUtils.getBytesUtf8(redirectUrl);
        return mTestServer.getURL(REDIRECT_TO_OTHER_BROWSER)
                + "?replace_text="
                + Base64.encodeToString(param, Base64.URL_SAFE)
                + ":"
                + Base64.encodeToString(value, Base64.URL_SAFE);
    }

    private IntentFilter createHelloIntentFilter() {
        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addDataScheme(UrlConstants.HTTPS_SCHEME);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        filter.addDataAuthority("127.0.0.1", null);
        filter.addDataPath(HELLO_PAGE, PatternMatcher.PATTERN_LITERAL);
        return filter;
    }

    @Test
    @LargeTest
    public void testRedirectToOtherBrowser_ChooseSelf() throws Exception {
        mTestContext.setResolveBrowserIntentToNonBrowserPackage(false);
        Intent result = new Intent(Intent.ACTION_CREATE_SHORTCUT);

        runRedirectToOtherBrowserTest(
                new Instrumentation.ActivityResult(Activity.RESULT_OK, result));

        // Wait for the target (data) URL to load in the tab.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mTabbedActivityTestRule.getActivityTab().getUrl().getSpec(),
                            Matchers.is(mTestServer.getURL(HELLO_PAGE)));
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            RedirectHandlerTabHelper.getOrCreateHandlerFor(
                                            mTabbedActivityTestRule.getActivityTab())
                                    .shouldNotOverrideUrlLoading());
                });
    }

    @Test
    @LargeTest
    public void testRedirectToOtherBrowser_ChooseOther() throws Exception {
        mTestContext.setResolveBrowserIntentToNonBrowserPackage(false);
        IntentFilter filter = createHelloIntentFilter();
        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(filter, null, true);

        Intent result = new Intent(Intent.ACTION_VIEW);
        result.setComponent(new ComponentName(OTHER_BROWSER_PACKAGE, "activity"));

        runRedirectToOtherBrowserTest(
                new Instrumentation.ActivityResult(Activity.RESULT_OK, result));

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(monitor.getHits(), Matchers.is(1));
                });

        InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);
    }

    @Test
    @LargeTest
    public void testRedirectToOtherBrowser_DefaultNonBrowserPackage() throws Exception {
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

        ChromeTabbedActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.CREATED,
                        () -> context.startActivity(intent));
        mTabbedActivityTestRule.getActivityTestRule().setActivity(activity);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(viewMonitor.getHits(), Matchers.is(1));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        Assert.assertEquals(0, pickActivityMonitor.getHits());
        InstrumentationRegistry.getInstrumentation().removeMonitor(pickActivityMonitor);
        InstrumentationRegistry.getInstrumentation().removeMonitor(viewMonitor);
    }

    @Test
    @LargeTest
    @EnableFeatures({"BackForwardCache", "BackForwardCacheNoTimeEviction"})
    @DisableFeatures({"BackForwardCacheMemoryControls"})
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testNoRedirectWithBFCache() throws Exception {
        final CallbackHelper finishCallback = new CallbackHelper();
        final CallbackHelper syncHelper = new CallbackHelper();
        AtomicReference<NavigationHandle> lastNavigationHandle = new AtomicReference<>(null);
        EmptyTabObserver observer =
                new EmptyTabObserver() {
                    @Override
                    public void onDidFinishNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        int callCount = syncHelper.getCallCount();
                        lastNavigationHandle.set(navigation);
                        finishCallback.notifyCalled();
                        try {
                            syncHelper.waitForCallback(callCount);
                        } catch (Exception e) {
                        }
                    }
                };
        String url = mTestServer.getURL(NAVIGATION_FROM_BFCACHE);
        mTabbedActivityTestRule.startOnUrl(url);

        // This test uses the back/forward cache, so return early if it's not enabled.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.BACK_FORWARD_CACHE)) return;

        final Tab tab = mTabbedActivityTestRule.getActivityTab();

        final RedirectHandler spyHandler =
                Mockito.spy(
                        ThreadUtils.runOnUiThreadBlocking(
                                () -> RedirectHandlerTabHelper.getHandlerFor(tab)));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.addObserver(observer);
                    RedirectHandlerTabHelper.swapHandlerForTesting(tab, spyHandler);
                });

        // Click link to go to second page.
        TouchCommon.singleClickView(tab.getView());
        finishCallback.waitForCallback(0);
        syncHelper.notifyCalled();

        AtomicInteger lastResultValue = new AtomicInteger();
        InterceptNavigationDelegateImpl.setResultCallbackForTesting(
                (Pair<GURL, OverrideUrlLoadingResult> result) -> {
                    if (result.first.getSpec().equals(url)) return;
                    lastResultValue.set(result.second.getResultType());
                });

        // Press back to go back to first page with BFCache.
        ThreadUtils.runOnUiThreadBlocking(
                mTabbedActivityTestRule.getActivity().getOnBackPressedDispatcher()::onBackPressed);
        finishCallback.waitForCallback(1);
        Assert.assertTrue(lastNavigationHandle.get().isPageActivation());
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
        Assert.assertTrue(lastNavigationHandle.get().getUrl().getSpec().startsWith("intent://"));
        syncHelper.notifyCalled();

        Assert.assertNotNull(getCurrentExternalNavigationMessage());
    }

    @Test
    @LargeTest
    @EnableFeatures({BlinkFeatures.PRERENDER2})
    @DisableFeatures({BlinkFeatures.PRERENDER2_MEMORY_CONTROLS})
    public void testClearRedirectHandlerOnPageActivation() throws Exception {
        mTabbedActivityTestRule.startOnBlankPage();

        final Tab tab = mTabbedActivityTestRule.getActivityTab();

        final CallbackHelper prerenderFinishCallback = new CallbackHelper();
        WebContentsObserver observer =
                new WebContentsObserver() {
                    @Override
                    public void didStopLoading(GURL url, boolean isKnownValid) {
                        prerenderFinishCallback.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    observer.observe(tab.getWebContents());
                });

        mTabbedActivityTestRule.loadUrl(mTestServer.getURL(NAVIGATION_FROM_PRERENDER));

        prerenderFinishCallback.waitForCallback(0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RedirectHandlerTabHelper.swapHandlerForTesting(tab, mRedirectHandler);
                    observer.observe(null);
                });

        // Click page to load prerender.
        TouchCommon.singleClickView(tab.getView());

        // Page activations should clear the RedirectHandler so future navigations aren't part of
        // the same navigation chain.
        Mockito.verify(
                        mRedirectHandler,
                        Mockito.timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .clear();
    }

    @Test
    @LargeTest
    public void testServerRedirectionFromIntent() throws Exception {
        TestWebServer webServer = TestWebServer.start();
        final String redirectTargetUrl =
                "intent://test/#Intent;scheme=" + EXTERNAL_APP_SCHEME + ";end";
        final String redirectUrl = webServer.setRedirect("/302.html", redirectTargetUrl);

        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(redirectUrl));
        intent.setClassName(context, ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        ChromeTabbedActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.CREATED,
                        () -> context.startActivity(intent));
        mTabbedActivityTestRule.getActivityTestRule().setActivity(activity);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        ApplicationTestUtils.waitForActivityState(activity, Stage.DESTROYED);
    }

    @Test
    @LargeTest
    @EnableFeatures({
        "FencedFrames:implementation_type/mparch",
        "PrivacySandboxAdsAPIsOverride",
        "FencedFramesAPIChanges",
        "FencedFramesDefaultMode"
    })
    public void testNavigationFromFencedFrame() throws Exception {
        mTabbedActivityTestRule.startOnBlankPage();

        final Tab tab = mTabbedActivityTestRule.getActivityTab();

        final CallbackHelper frameFinishCallback = new CallbackHelper();
        WebContentsObserver observer =
                new WebContentsObserver() {
                    @Override
                    public void didStopLoading(GURL url, boolean isKnownValid) {
                        frameFinishCallback.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    observer.observe(tab.getWebContents());
                });

        try {
            // Note for posterity: This depends on
            // navigation_from_user_gesture.html.mock-http-headers to work.
            mTabbedActivityTestRule.loadUrl(mTestServer.getURL(NAVIGATION_FROM_FENCED_FRAME));

            frameFinishCallback.waitForCallback(0);
        } finally {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        observer.observe(null);
                    });
        }

        // Because fenced frames are now being loaded with a config object, it
        // needs extra time to load the page outside of what the
        // WebContentsObserver is waiting for. Wait for the the fenced frame's
        // navigation to commit before continuing.
        final String fencedFrameUrl = mTestServer.getURL(NAVIGATION_FROM_USER_GESTURE_PAGE);
        RenderFrameHost mainFrame =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mTabbedActivityTestRule.getWebContents().getMainFrame());
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            FencedFrameUtils.getLastFencedFrame(mainFrame, fencedFrameUrl),
                            Matchers.notNullValue());
                });

        // Click page to launch app. There's no easy way to know when an out of process subframe is
        // ready to receive input, even if the document is loaded and javascript runs. If the click
        // fails the first time, try a second time.
        try {
            TouchCommon.singleClickView(tab.getView());

            CriteriaHelper.pollUiThread(
                    () -> {
                        Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1));
                    });
        } catch (Throwable e) {
            TouchCommon.singleClickView(tab.getView());

            CriteriaHelper.pollUiThread(
                    () -> {
                        Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1));
                    });
        }
    }

    private void doTestIntentWithRedirectToApp(boolean targetsChrome, boolean addAllowLeaveExtra)
            throws Exception {
        final String redirectUrl = "https://example.com/path";
        final String initialUrl =
                mTestServer.getURL(
                        "/chrome/test/data/android/redirect/js_redirect.html"
                                + "?replace_text="
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8("PARAM_URL"),
                                        Base64.URL_SAFE)
                                + ":"
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8(redirectUrl),
                                        Base64.URL_SAFE));

        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        filter.addDataAuthority("example.com", null);
        filter.addDataScheme("https");
        ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation()
                        .addMonitor(
                                filter,
                                new Instrumentation.ActivityResult(Activity.RESULT_OK, null),
                                true);
        mTestContext.setIntentFilterForHost("example.com", filter);

        AsyncInitializationActivity.interceptMoveTaskToBackForTesting();
        AtomicInteger lastResultValue = new AtomicInteger();
        final CallbackHelper redirectTaken = new CallbackHelper();
        InterceptNavigationDelegateImpl.setResultCallbackForTesting(
                (Pair<GURL, OverrideUrlLoadingResult> result) -> {
                    if (!result.first.getSpec().equals(redirectUrl)) return;
                    lastResultValue.set(result.second.getResultType());
                    redirectTaken.notifyCalled();
                });

        Intent intent = getCustomTabFromChromeIntent(initialUrl, targetsChrome);

        if (addAllowLeaveExtra) {
            intent.putExtra(CustomTabsIntent.EXTRA_SEND_TO_EXTERNAL_DEFAULT_HANDLER, true);
        }

        mCustomTabActivityRule.launchActivity(intent);

        if (!targetsChrome || addAllowLeaveExtra) {
            CriteriaHelper.pollUiThread(
                    () -> {
                        Criteria.checkThat(monitor.getHits(), Matchers.is(1));
                    },
                    10000L,
                    CriteriaHelper.DEFAULT_POLLING_INTERVAL);
            CriteriaHelper.pollUiThread(
                    () -> AsyncInitializationActivity.wasMoveTaskToBackInterceptedForTesting());
        } else {
            redirectTaken.waitForOnly(10, TimeUnit.SECONDS);
            Assert.assertEquals(OverrideUrlLoadingResultType.NO_OVERRIDE, lastResultValue.get());
            Assert.assertFalse(
                    AsyncInitializationActivity.wasMoveTaskToBackInterceptedForTesting());
            Assert.assertEquals(0, monitor.getHits());
        }
    }

    @Test
    @Feature("CustomTabFromChrome")
    @LargeTest
    public void testIntentWithRedirectToApp_TargetsChrome() throws Exception {
        doTestIntentWithRedirectToApp(true, false);
    }

    @Test
    @Feature("CustomTabFromChrome")
    @LargeTest
    public void testIntentWithRedirectToApp_TargetsChrome_AllowedToLeave() throws Exception {
        doTestIntentWithRedirectToApp(true, true);
    }

    @Test
    @Feature("CustomTabFromChrome")
    @LargeTest
    public void testIntentWithRedirectToApp() throws Exception {
        doTestIntentWithRedirectToApp(false, false);
    }

    @Test
    @LargeTest
    public void testExternalNavigationMessage() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();

        TestParams params =
                new TestParams(mTestServer.getURL(NAVIGATION_FROM_LONG_TIMEOUT), true, false);
        OverrideUrlLoadingResult result = loadUrlAndWaitForIntentUrl(params, ctaPage);

        Assert.assertEquals(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION, result.getResultType());

        assertMessagePresent();
    }

    @Test
    @LargeTest
    public void testRedirectFromBookmark() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();

        String url = mTestServer.getURL(NAVIGATION_FROM_TIMEOUT_PAGE);
        TestParams params = new TestParams(url, false, false);
        params.transition = PageTransition.AUTO_BOOKMARK;
        OverrideUrlLoadingResult result = loadUrlAndWaitForIntentUrl(params, ctaPage);

        Assert.assertEquals(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION, result.getResultType());
        assertMessagePresent();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TextView button =
                            mTabbedActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.message_primary_button);
                    button.performClick();
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1));
                    Criteria.checkThat(
                            mTabbedActivityTestRule
                                    .getActivityTab()
                                    .getUrl()
                                    .getSpec(),
                            Matchers.is("about:blank"));
                });
    }

    @Test
    @LargeTest
    public void testRedirectFromBookmarkWithFallback() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();

        String fallbackUrl = mTestServer.getURL(FALLBACK_LANDING_PATH);
        String originalUrl =
                mTestServer.getURL(
                        NAVIGATION_FROM_TIMEOUT_WITH_FALLBACK_PAGE
                                + "?replace_text="
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8("PARAM_FALLBACK_URL"),
                                        Base64.URL_SAFE)
                                + ":"
                                + Base64.encodeToString(
                                        ApiCompatibilityUtils.getBytesUtf8(fallbackUrl),
                                        Base64.URL_SAFE));

        TestParams params = new TestParams(originalUrl, false, false);
        params.transition = PageTransition.AUTO_BOOKMARK;
        params.expectedFinalUrl = fallbackUrl;
        OverrideUrlLoadingResult result = loadUrlAndWaitForIntentUrl(params, ctaPage);

        Assert.assertEquals(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_NAVIGATE_TAB, result.getResultType());
        Assert.assertNull(getCurrentExternalNavigationMessage());
    }

    @Test
    @LargeTest
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testRedirectFromCctSpeculation() throws Exception {
        final String url = mTestServer.getURL(NAVIGATION_FROM_PAGE_SHOW);
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = ContextUtils.getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, url);
        final var sessionHolder = SessionHolder.getSessionHolderFromIntent(intent);
        Assert.assertTrue(connection.newSession(sessionHolder.getSessionAsCustomTab()));

        connection.setCanUseHiddenTabForSession(sessionHolder, true);
        Assert.assertTrue(
                connection.mayLaunchUrl(
                        sessionHolder.getSessionAsCustomTab(), Uri.parse(url), null, null));
        CustomTabsTestUtils.ensureCompletedSpeculationForUrl(url);

        // Can't wait for Activity startup as we close so fast the polling is flaky.
        mCustomTabActivityRule.launchActivity(intent);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @LargeTest
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testRedirectFromCctEarlyNav() throws Exception {
        final String url = mTestServer.getURL(NAVIGATION_FROM_JAVA_REDIRECTION_PAGE);
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = ContextUtils.getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, url);

        final var sessionHolder = SessionHolder.getSessionHolderFromIntent(intent);
        Assert.assertTrue(connection.newSession(sessionHolder.getSessionAsCustomTab()));

        AsyncInitializationActivity.interceptMoveTaskToBackForTesting();
        // Can't wait for Activity startup as we close so fast the polling is flaky.
        mCustomTabActivityRule.launchActivity(intent);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        CriteriaHelper.pollUiThread(
                () -> AsyncInitializationActivity.wasMoveTaskToBackInterceptedForTesting());
    }

    @Test
    @LargeTest
    public void testRedirectToTrustedCaller() throws Exception {
        final String url = mTestServer.getURL(HELLO_PAGE);
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = ContextUtils.getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, url);
        final var sessionHolder = SessionHolder.getSessionHolderFromIntent(intent);
        Assert.assertTrue(connection.newSession(sessionHolder.getSessionAsCustomTab()));
        connection.overridePackageNameForSessionForTesting(sessionHolder, TRUSTED_CCT_PACKAGE);

        mCustomTabActivityRule.startCustomTabActivityWithIntent(intent);

        final Tab tab = mCustomTabActivityRule.getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> RedirectHandlerTabHelper.swapHandlerForTesting(tab, mSpyRedirectHandler));

        mCustomTabActivityRule.loadUrl(
                mTestServer.getURL(NAVIGATION_FROM_XHR_CALLBACK_AND_SHORT_TIMEOUT_PAGE));

        // This is a little fragile to code changes, but better than waiting 15 real seconds.
        Mockito.doReturn(SystemClock.elapsedRealtime()) // XHR Navigation create
                .doReturn(SystemClock.elapsedRealtime()) // XHR callback navigation create
                .doReturn(
                        SystemClock.elapsedRealtime()
                                + RedirectHandler.NAVIGATION_CHAIN_TIMEOUT_MILLIS
                                + 1) // xhr callback
                .doReturn(SystemClock.elapsedRealtime()) // XHR Navigation create
                .doReturn(SystemClock.elapsedRealtime()) // XHR callback navigation create
                .doReturn(
                        SystemClock.elapsedRealtime()
                                + RedirectHandler.NAVIGATION_CHAIN_TIMEOUT_MILLIS
                                + 1) // xhr callback
                .when(mSpyRedirectHandler)
                .currentRealtime();

        TouchCommon.singleClickView(tab.getView());
        // Wait for blocked Message to show.
        CriteriaHelper.pollInstrumentationThread(
                () -> getCurrentExternalNavigationMessage() != null);
        Assert.assertEquals(0, mActivityMonitor.getHits());

        mTestContext.setResolveToTrustedCaller(true);
        TouchCommon.singleClickView(tab.getView());

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @LargeTest
    public void testSubframeNavigationToSelf() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();

        String targetUrl = mTestServer.getURL(HELLO_PAGE);
        // Strip off the https: from the URL.
        String strippedTargetUrl = targetUrl.substring(6);
        String subframeTarget =
                "intent:"
                        + strippedTargetUrl
                        + "#Intent;scheme=https;package="
                        + ContextUtils.getApplicationContext().getPackageName()
                        + ";S.browser_fallback_url="
                        + "https%3A%2F%2Fplay.google.com%2Fstore%2Fapps%2Fdetails%3Fid%3Dcom.android.chrome"
                        + ";end";

        String originalUrl =
                getSubframeNavigationUrl(subframeTarget, NavigationType.SELF, SandboxType.NONE);

        final Tab tab = mTabbedActivityTestRule.getActivityTab();

        final CallbackHelper subframeRedirect = new CallbackHelper();
        EmptyTabObserver observer =
                new EmptyTabObserver() {
                    @Override
                    public void onDidStartNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        Assert.assertEquals(originalUrl, navigation.getUrl().getSpec());
                    }

                    @Override
                    public void onDidRedirectNavigation(Tab tab, NavigationHandle navigation) {
                        Assert.assertFalse(navigation.isInPrimaryMainFrame());
                        if (targetUrl.equals(navigation.getUrl().getSpec())) {
                            subframeRedirect.notifyCalled();
                        }
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.addObserver(observer);
                });

        // Fallback URL from a subframe will not trigger main navigation.
        TestParams params = new TestParams(originalUrl, true, false);
        params.willLoadSubframe = true;
        params.willNavigateTwice = true;
        params.shouldFailNavigation = false;
        OverrideUrlLoadingResult result = loadUrlAndWaitForIntentUrl(params, ctaPage);

        Assert.assertEquals(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_NAVIGATE_TAB, result.getResultType());
        subframeRedirect.waitForOnly();
    }

    void doTestIncognitoSubframeExternalNavigation(boolean acceptPrompt) throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();

        // This will cause getActivityTab() in loadUrlAndWaitForIntentUrl to return an incognito tab
        // instead.
        IncognitoNewTabPageStation incognitoPage = ctaPage.openNewIncognitoTabOrWindowFast();

        String fallbackUrl = mTestServer.getURL(FALLBACK_LANDING_PATH);
        String subframeUrl =
                "intent://test/#Intent;scheme=externalappscheme;S.browser_fallback_url="
                        + fallbackUrl
                        + ";end";
        String originalUrl =
                getSubframeNavigationUrl(subframeUrl, NavigationType.SELF, SandboxType.NONE);

        final Tab tab = incognitoPage.getTab();

        final CallbackHelper subframeRedirect = new CallbackHelper();
        EmptyTabObserver observer =
                new EmptyTabObserver() {
                    @Override
                    public void onDidStartNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        Assert.assertEquals(originalUrl, navigation.getUrl().getSpec());
                    }

                    @Override
                    public void onDidRedirectNavigation(Tab tab, NavigationHandle navigation) {
                        if (acceptPrompt) Assert.fail();
                        Assert.assertFalse(navigation.isInPrimaryMainFrame());
                        if (fallbackUrl.equals(navigation.getUrl().getSpec())) {
                            subframeRedirect.notifyCalled();
                        }
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.addObserver(observer);
                });

        TestParams params = new TestParams(originalUrl, true, false);
        params.willLoadSubframe = true;
        params.shouldFailNavigation = false;
        OverrideUrlLoadingResult result = loadUrlAndWaitForIntentUrl(params, incognitoPage);

        Assert.assertEquals(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION, result.getResultType());

        if (acceptPrompt) {
            Espresso.onView(withId(R.id.positive_button)).perform(click());
            CriteriaHelper.pollUiThread(
                    () -> {
                        Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1));
                        Criteria.checkThat(tab.getUrl().getSpec(), Matchers.is(originalUrl));
                    });
        } else {
            Espresso.onView(withId(R.id.negative_button)).perform(click());
            subframeRedirect.waitForOnly();
            Assert.assertEquals(0, mActivityMonitor.getHits());
        }

        // Cleanup newly created incognito window when applicable.
        maybeCleanupIncognitoWindow(incognitoPage);
    }

    @Test
    @LargeTest
    public void testIncognitoSubframeExternalNavigation_Rejected() throws Exception {
        doTestIncognitoSubframeExternalNavigation(false);
    }

    @Test
    @LargeTest
    public void testIncognitoSubframeExternalNavigation_Accepted() throws Exception {
        doTestIncognitoSubframeExternalNavigation(true);
    }

    @Test
    @LargeTest
    public void testWindowOpenRedirect() throws Exception {
        mTabbedActivityTestRule.startOnBlankPage();
        ChromeActivity activity = mTabbedActivityTestRule.getActivity();
        TabModelJniBridge tabModel =
                (TabModelJniBridge) activity.getTabModelSelector().getModel(false);
        GURL url = new GURL(EXTERNAL_APP_URL);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Called when a popup window is allowed.
                    tabModel.openNewTab(
                            activity.getActivityTab(),
                            url,
                            createExampleOrigin(),
                            null,
                            null,
                            WindowOpenDisposition.NEW_FOREGROUND_TAB,
                            true,
                            true);
                });

        assertMessagePresent();
    }

    @Test
    @LargeTest
    public void testWindowRenavigation() throws Exception {
        String finalUrl = mTestServer.getURL(HELLO_PAGE);
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();

        TestParams params =
                new TestParams(mTestServer.getURL(NAVIGATION_FROM_RENAVIGATE_FRAME), true, false);
        params.createsNewTab = true;
        params.expectedFinalUrl = finalUrl;
        OverrideUrlLoadingResult result = loadUrlAndWaitForIntentUrl(params, ctaPage);

        Assert.assertEquals(OverrideUrlLoadingResultType.NO_OVERRIDE, result.getResultType());
        Assert.assertNull(getCurrentExternalNavigationMessage());
    }

    @Test
    @LargeTest
    public void testWindowRenavigationServerRedirect() throws Exception {
        String finalUrl = mTestServer.getURL(HELLO_PAGE);
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();

        TestParams params =
                new TestParams(
                        mTestServer.getURL(NAVIGATION_FROM_RENAVIGATE_FRAME_WITH_REDIRECT),
                        true,
                        false);
        params.createsNewTab = true;
        params.expectedFinalUrl = finalUrl;
        OverrideUrlLoadingResult result = loadUrlAndWaitForIntentUrl(params, ctaPage);

        Assert.assertEquals(OverrideUrlLoadingResultType.NO_OVERRIDE, result.getResultType());
        Assert.assertNull(getCurrentExternalNavigationMessage());
    }

    @Test
    @LargeTest
    public void testWindowServerRedirect() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        TestParams params =
                new TestParams(mTestServer.getURL(NAVIGATION_FROM_WINDOW_REDIRECT), true, true);
        params.createsNewTab = true;
        params.expectedFinalUrl = null;
        loadUrlAndWaitForIntentUrl(params, ctaPage);
    }

    @Test
    @LargeTest
    public void testNavigateTopFrame() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();

        String subframeUrl = "intent://test/#Intent;scheme=externalappscheme;end";
        String originalUrl =
                getSubframeNavigationUrl(subframeUrl, NavigationType.TOP, SandboxType.NONE);

        TestParams params = new TestParams(originalUrl, true, true);
        params.willLoadSubframe = true;
        loadUrlAndWaitForIntentUrl(params, ctaPage);
    }

    @Test
    @LargeTest
    public void testIntentToSelf() throws Exception {
        String targetUrl = mTestServer.getURL(HELLO_PAGE);
        // Strip off the https: from the URL.
        String strippedTargetUrl = targetUrl.substring(6);
        String link =
                "intent:"
                        + strippedTargetUrl
                        + "#Intent;scheme=https;package="
                        + ContextUtils.getApplicationContext().getPackageName()
                        + ";end";

        byte[] paramName = ApiCompatibilityUtils.getBytesUtf8("PARAM_SUBFRAME_URL");
        byte[] paramValue = ApiCompatibilityUtils.getBytesUtf8(link);

        String url =
                mTestServer.getURL(
                        SUBFRAME_NAVIGATION_CHILD
                                + "?replace_text="
                                + Base64.encodeToString(paramName, Base64.URL_SAFE)
                                + ":"
                                + Base64.encodeToString(paramValue, Base64.URL_SAFE));

        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();
        TestParams params = new TestParams(url, true, false);
        params.willNavigateTwice = true;
        params.expectedFinalUrl = null;
        loadUrlAndWaitForIntentUrl(params, ctaPage);
    }

    @Test
    @LargeTest
    public void testIntentToSelfWithFallback() throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();

        String targetUrl = mTestServer.getURL(HELLO_PAGE);
        // Strip off the https: from the URL.
        String strippedTargetUrl = targetUrl.substring(6);
        String subframeTarget =
                "intent:"
                        + strippedTargetUrl
                        + "#Intent;scheme=https;package="
                        + ContextUtils.getApplicationContext().getPackageName()
                        + ";S.browser_fallback_url="
                        + "https%3A%2F%2Fplay.google.com%2Fstore%2Fapps%2Fdetails%3Fid%3Dcom.android.chrome"
                        + ";end";

        String originalUrl =
                getSubframeNavigationUrl(subframeTarget, NavigationType.BLANK, SandboxType.NONE);

        final Tab tab = mTabbedActivityTestRule.getActivityTab();

        final AtomicInteger navCount = new AtomicInteger(0);
        EmptyTabObserver observer =
                new EmptyTabObserver() {
                    @Override
                    public void onDidStartNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        int count = navCount.getAndIncrement();
                        if (count == 0) {
                            Assert.assertEquals(originalUrl, navigation.getUrl().getSpec());
                        } else if (count == 1) {
                            Assert.assertEquals(subframeTarget, navigation.getUrl().getSpec());
                        } else if (count == 2) {
                            Assert.assertEquals(targetUrl, navigation.getUrl().getSpec());
                        } else {
                            Assert.fail();
                        }
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.addObserver(observer);
                });

        TestParams params = new TestParams(originalUrl, true, false);
        params.createsNewTab = true;
        params.expectedFinalUrl = targetUrl;
        params.willLoadSubframe = true;
        OverrideUrlLoadingResult result = loadUrlAndWaitForIntentUrl(params, ctaPage);
        Assert.assertEquals(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_NAVIGATE_TAB, result.getResultType());
    }

    private void doTestIntentToSelfWithFallback_Sandboxed(boolean useCSP) throws Exception {
        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();

        String targetUrl = mTestServer.getURL(HELLO_PAGE);
        // Strip off the https: from the URL.
        String strippedTargetUrl = targetUrl.substring(6);
        String subframeTarget =
                "intent:"
                        + strippedTargetUrl
                        + "#Intent;scheme=https;package="
                        + ContextUtils.getApplicationContext().getPackageName()
                        + ";S.browser_fallback_url="
                        + "https%3A%2F%2Fplay.google.com%2Fstore%2Fapps%2Fdetails%3Fid%3Dcom.android.chrome"
                        + ";end";

        @SandboxType int sandboxType = useCSP ? SandboxType.CSP : SandboxType.FRAME;
        String originalUrl =
                getSubframeNavigationUrl(subframeTarget, NavigationType.BLANK, sandboxType);

        final Tab tab = mTabbedActivityTestRule.getActivityTab();

        final AtomicInteger navCount = new AtomicInteger(0);
        EmptyTabObserver observer =
                new EmptyTabObserver() {
                    @Override
                    public void onDidStartNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        int count = navCount.getAndIncrement();
                        if (count == 0) {
                            Assert.assertEquals(originalUrl, navigation.getUrl().getSpec());
                        } else if (count == 1) {
                            Assert.assertEquals(subframeTarget, navigation.getUrl().getSpec());
                        } else {
                            Assert.fail();
                        }
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.addObserver(observer);
                });

        TestParams params = new TestParams(originalUrl, true, false);
        params.createsNewTab = true;
        params.willLoadSubframe = true;
        params.expectedFinalUrl = null;
        OverrideUrlLoadingResult result = loadUrlAndWaitForIntentUrl(params, ctaPage);
        // Navigation to self is blocked, ExternalNavigationHandler asks to navigate to the
        // fallback URL.
        Assert.assertEquals(
                OverrideUrlLoadingResultType.OVERRIDE_WITH_NAVIGATE_TAB, result.getResultType());
        // Fallback URL is blocked by InterceptNavigationDelegateImpl, no URL is loading and the
        // final URL is the subframe's target.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab newTab = mTabbedActivityTestRule.getActivityTab();
                    Assert.assertEquals(subframeTarget, newTab.getUrl().getSpec());
                    Assert.assertFalse(newTab.getWebContents().isLoading());
                });
    }

    // Ensures that for a sandboxed main frame, we block both intents to ourself, and fallback URLs
    // that would escape the sandbox by clobbering the main frame.
    @Test
    @LargeTest
    public void testIntentToSelfWithFallback_Sandboxed() throws Exception {
        doTestIntentToSelfWithFallback_Sandboxed(false);
    }

    // Same as testIntentToSelfWithFallback_Sandboxed but with CSP sandbox.
    @Test
    @LargeTest
    public void testIntentToSelfWithFallback_CSPSandboxed() throws Exception {
        doTestIntentToSelfWithFallback_Sandboxed(true);
    }

    @Test
    @LargeTest
    public void testAuxiliaryNavigationShouldStayInBrowser() throws Exception {
        InterceptNavigationDelegateClientImpl.setIsDesktopWindowingModeForTesting(true);

        IntentFilter filter = createHelloIntentFilter();
        mActivityMonitor =
                InstrumentationRegistry.getInstrumentation()
                        .addMonitor(
                                filter,
                                new Instrumentation.ActivityResult(Activity.RESULT_OK, null),
                                true);
        mTestContext.setIntentFilterForHost("127.0.0.1", filter);

        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();

        String urlExternal = mTestServer.getURL(HELLO_PAGE);
        String url = getUrlWithParam(NAVIGATION_FROM_TARGET_BLANK_REL_OPENER_LINK, urlExternal);
        TestParams testParams = new TestParams(url, true, false);
        testParams.createsNewTab = true;
        testParams.expectedFinalUrl = null;
        testParams.shouldFailNavigation = false;
        testParams.willNavigateTwice = true;
        loadUrlAndWaitForIntentUrl(testParams, ctaPage);

        ChromeTabbedActivity activity = mTabbedActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(0));
                    Criteria.checkThat(
                            ChromeTabUtils.getNumOpenTabs(mTabbedActivityTestRule.getActivity()),
                            Matchers.is(2));
                    Criteria.checkThat(
                            activity.getActivityTab().getUrl().getSpec(),
                            Matchers.equalTo(new GURL(urlExternal).getSpec()));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @LargeTest
    @DisabledTest(message = "https://crbug.com/446837985")
    public void testTopLevelNavigationShouldBeIntercepted() throws Exception {
        InterceptNavigationDelegateClientImpl.setIsDesktopWindowingModeForTesting(true);

        IntentFilter filter = createHelloIntentFilter();
        mActivityMonitor =
                InstrumentationRegistry.getInstrumentation()
                        .addMonitor(
                                filter,
                                new Instrumentation.ActivityResult(Activity.RESULT_OK, null),
                                true);
        mTestContext.setIntentFilterForHost("127.0.0.1", filter);

        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();

        String urlExternal = mTestServer.getURL(HELLO_PAGE);
        String url = getUrlWithParam(NAVIGATION_FROM_TARGET_BLANK_LINK, urlExternal);
        TestParams testParams = new TestParams(url, true, true);
        testParams.createsNewTab = true;
        testParams.expectedFinalUrl = null;
        testParams.shouldFailNavigation = true;
        loadUrlAndWaitForIntentUrl(testParams, ctaPage);

        ChromeTabbedActivity activity = mTabbedActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(1));
                    Criteria.checkThat(
                            ChromeTabUtils.getNumOpenTabs(mTabbedActivityTestRule.getActivity()),
                            Matchers.is(1));
                    Criteria.checkThat(
                            activity.getActivityTab().getUrl().getSpec(),
                            Matchers.equalTo(new GURL(url).getSpec()));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @LargeTest
    public void testSelfNavigationInAuxiliaryPage() throws Exception {
        InterceptNavigationDelegateClientImpl.setIsDesktopWindowingModeForTesting(true);

        IntentFilter filter = createHelloIntentFilter();
        mActivityMonitor =
                InstrumentationRegistry.getInstrumentation()
                        .addMonitor(
                                filter,
                                new Instrumentation.ActivityResult(Activity.RESULT_OK, null),
                                true);
        mTestContext.setIntentFilterForHost("127.0.0.1", filter);

        WebPageStation ctaPage = mTabbedActivityTestRule.startOnBlankPage();

        String pageWithSelfLink =
                getUrlWithParam(NAVIGATION_FROM_TARGET_SELF_LINK, mTestServer.getURL(HELLO_PAGE));
        String pageWithBlankOpenerLink =
                getUrlWithParam(NAVIGATION_FROM_TARGET_BLANK_REL_OPENER_LINK, pageWithSelfLink);

        // open first tab and new auxiliary tab
        TestParams testParams = new TestParams(pageWithBlankOpenerLink, true, false);
        testParams.createsNewTab = true;
        testParams.expectedFinalUrl = new GURL(pageWithSelfLink).getSpec();
        testParams.shouldFailNavigation = false;
        testParams.willNavigateTwice = true;
        loadUrlAndWaitForIntentUrl(testParams, ctaPage);

        Tab tab = mTabbedActivityTestRule.getActivityTab();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ChromeTabUtils.getNumOpenTabs(mTabbedActivityTestRule.getActivity()),
                            Matchers.is(2));
                    Criteria.checkThat(tab.getWebContents().hasOpener(), Matchers.is(true));
                });

        TouchCommon.singleClickView(tab.getView());

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ChromeTabUtils.getNumOpenTabs(mTabbedActivityTestRule.getActivity()),
                            Matchers.is(2));
                    Criteria.checkThat(mActivityMonitor.getHits(), Matchers.is(0));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void launchTwa(String twaPackageName, String url) throws TimeoutException {
        Intent intent = TrustedWebActivityTestUtil.createTrustedWebActivityIntent(url);
        TrustedWebActivityTestUtil.spoofVerification(twaPackageName, url);
        TrustedWebActivityTestUtil.createSession(intent, twaPackageName);
        mCustomTabActivityRule.startCustomTabActivityWithIntent(intent);
    }

    private ChromeActivity launchTwaAndClick(String url) throws TimeoutException {
        launchTwa("com.foo.bar", url);
        ChromeActivity activity = mCustomTabActivityRule.getActivity();
        Tab tab = activity.getActivityTab();

        Assert.assertTrue(tab.isTabInPWA());
        Assert.assertFalse(tab.getWebContents().hasOpener());

        ChromeTabbedActivity newActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.STARTED,
                        () -> TouchCommon.singleClickView(tab.getView()));

        ApplicationTestUtils.waitForActivityState(newActivity, Stage.RESUMED);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(newActivity.getActivityTab(), Matchers.notNullValue());
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        return newActivity;
    }

    @Test
    @LargeTest
    public void testAuxiliaryNavigationWasReparented() throws TimeoutException {
        InterceptNavigationDelegateClientImpl.setIsDesktopWindowingModeForTesting(true);

        ChromeActivity newActivity =
                launchTwaAndClick(
                        getUrlWithParam(
                                NAVIGATION_FROM_TARGET_BLANK_REL_OPENER_LINK,
                                "https://example.com"));

        Tab tab = ThreadUtils.runOnUiThreadBlocking(newActivity::getActivityTab);
        Assert.assertFalse(tab.isTabInPWA());
        Assert.assertTrue(tab.getWebContents().hasOpener());
    }

    @Test
    @LargeTest
    public void testTopLevelNavigationWasReparented() throws TimeoutException {
        InterceptNavigationDelegateClientImpl.setIsDesktopWindowingModeForTesting(true);

        ChromeActivity newActivity =
                launchTwaAndClick(
                        getUrlWithParam(NAVIGATION_FROM_TARGET_BLANK_LINK, "https://example.com"));

        Tab tab = ThreadUtils.runOnUiThreadBlocking(newActivity::getActivityTab);
        Assert.assertFalse(tab.isTabInPWA());
        Assert.assertFalse(tab.getWebContents().hasOpener());
    }

    @Test
    @LargeTest
    public void testNavigationsToSelfPWALaunchHandler() throws Exception {
        InterceptNavigationDelegateClientImpl.setIsDesktopWindowingModeForTesting(true);

        IntentFilter filter = createHelloIntentFilter();
        mActivityMonitor =
                InstrumentationRegistry.getInstrumentation()
                        .addMonitor(
                                filter,
                                new Instrumentation.ActivityResult(Activity.RESULT_OK, null),
                                true);
        mTestContext.setIntentFilterForHost("127.0.0.1", filter);

        mTabbedActivityTestRule.startOnBlankPage();

        String url2 = mTestServer.getURL(HELLO_PAGE);
        String url1 = getUrlWithParam(NAVIGATION_FROM_TARGET_BLANK_LINK, url2);

        launchTwa("com.foo.bar", url1);

        ChromeActivity activity = mCustomTabActivityRule.getActivity();

        TouchCommon.singleClickView(activity.getActivityTab().getView());

        // The TWA is still displaying the initial web page
        Assert.assertEquals(new GURL(url1).getSpec(), activity.getActivityTab().getUrl().getSpec());

        // url1 and url2 are in the scope of the same TWA but an intent was generated anyway
        Assert.assertFalse(mActivityMonitor.getHits() == 1);
    }

    private void doTestInitialIntentToApp(boolean allowInitialIntentToLeave, boolean prewarm)
            throws Exception {
        final String initialUrl = "https://example.com/path";

        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        filter.addDataAuthority("example.com", null);
        filter.addDataScheme("https");

        mTestContext.setIntentFilterForHost("example.com", filter);

        AsyncInitializationActivity.interceptMoveTaskToBackForTesting();
        AtomicInteger lastResultValue = new AtomicInteger();
        final CallbackHelper navigated = new CallbackHelper();
        InterceptNavigationDelegateImpl.setResultCallbackForTesting(
                (Pair<GURL, OverrideUrlLoadingResult> result) -> {
                    Assert.assertEquals(initialUrl, result.first.getSpec());
                    lastResultValue.set(result.second.getResultType());
                    navigated.notifyCalled();
                });

        if (prewarm) {
            CustomTabsTestUtils.warmUpAndWait();
        }

        Intent intent = getCustomTabFromChromeIntent(initialUrl, false);

        if (allowInitialIntentToLeave) {
            intent.putExtra(CustomTabsIntent.EXTRA_INITIAL_NAVIGATION_CAN_LEAVE_BROWSER, true);
        }

        ActivityMonitor[] monitor = new ActivityMonitor[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ApplicationStatus.registerStateListenerForAllActivities(
                            new ActivityStateListener() {
                                @Override
                                public void onActivityStateChange(Activity activity, int newState) {
                                    assertThat(activity.getClass())
                                            .isAssignableTo(CustomTabActivity.class);
                                    if (newState == ActivityState.CREATED) {
                                        // We need to add the ActivityMonitor after the activity
                                        // starts or we'll block the intent, as the filter matches
                                        // both the intent that starts Chrome and the one that
                                        // Chrome sends.
                                        monitor[0] =
                                                InstrumentationRegistry.getInstrumentation()
                                                        .addMonitor(
                                                                filter,
                                                                new Instrumentation.ActivityResult(
                                                                        Activity.RESULT_OK, null),
                                                                true);
                                    }
                                }
                            });
                });

        mCustomTabActivityRule.launchActivity(intent);

        if (allowInitialIntentToLeave) {
            CriteriaHelper.pollUiThread(
                    () -> {
                        Criteria.checkThat(monitor[0].getHits(), Matchers.is(1));
                    },
                    10000L,
                    CriteriaHelper.DEFAULT_POLLING_INTERVAL);
            CriteriaHelper.pollUiThread(
                    () -> AsyncInitializationActivity.wasMoveTaskToBackInterceptedForTesting());
        } else {
            navigated.waitForOnly(10, TimeUnit.SECONDS);
            Assert.assertEquals(OverrideUrlLoadingResultType.NO_OVERRIDE, lastResultValue.get());
            Assert.assertFalse(
                    AsyncInitializationActivity.wasMoveTaskToBackInterceptedForTesting());
            Assert.assertEquals(0, monitor[0].getHits());
        }
    }

    @Test
    @Feature("CustomTabFromChrome")
    @LargeTest
    public void testInitialIntentToApp() throws Exception {
        doTestInitialIntentToApp(false, false);
    }

    @Test
    @Feature("CustomTabFromChrome")
    @LargeTest
    public void testInitialIntentToApp_allowToLeave() throws Exception {
        doTestInitialIntentToApp(true, false);
    }

    @Test
    @Feature("CustomTabFromChrome")
    @LargeTest
    public void testInitialIntentToApp_prewarmed() throws Exception {
        doTestInitialIntentToApp(false, true);
    }

    @Test
    @Feature("CustomTabFromChrome")
    @LargeTest
    public void testInitialIntentToApp_allowToLeave_prewarmed() throws Exception {
        doTestInitialIntentToApp(true, true);
    }

    @Test
    @Feature("CustomTabFromChrome")
    @LargeTest
    @EnableFeatures(ChromeFeatureList.CCT_DESTROY_TAB_WHEN_MODEL_IS_EMPTY)
    public void testInitialIntentToApp_CctFinishesAfterHandoff() throws Exception {
        final String initialUrl = "https://example.com/path";
        final CallbackHelper onHandedOffCallback = new CallbackHelper();

        CustomTabActivity.setOnFinishCallbackForTesting(onHandedOffCallback::notifyCalled);

        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        filter.addDataAuthority("example.com", null);
        filter.addDataScheme("https");
        mTestContext.setIntentFilterForHost("example.com", filter);

        ActivityMonitor[] monitor = new ActivityMonitor[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ApplicationStatus.registerStateListenerForAllActivities(
                            new ActivityStateListener() {
                                @Override
                                public void onActivityStateChange(Activity activity, int newState) {
                                    if (activity instanceof CustomTabActivity
                                            && newState == ActivityState.CREATED) {
                                        monitor[0] =
                                                InstrumentationRegistry.getInstrumentation()
                                                        .addMonitor(
                                                                filter,
                                                                new Instrumentation.ActivityResult(
                                                                        Activity.RESULT_OK, null),
                                                                true);
                                        ApplicationStatus.unregisterActivityStateListener(this);
                                    }
                                }
                            });
                });

        Intent intent = getCustomTabFromChromeIntent(initialUrl, false);
        intent.putExtra(CustomTabsIntent.EXTRA_INITIAL_NAVIGATION_CAN_LEAVE_BROWSER, true);
        Context context = ContextUtils.getApplicationContext();

        CustomTabActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        CustomTabActivity.class,
                        Stage.CREATED,
                        () -> context.startActivity(intent));
        mCustomTabActivityRule.setActivity(activity);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "ActivityMonitor was not set", monitor[0], Matchers.notNullValue());
                    Criteria.checkThat(
                            "External app was not launched", monitor[0].getHits(), Matchers.is(1));
                },
                10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        ApplicationTestUtils.waitForActivityState(activity, Stage.DESTROYED);

        Assert.assertEquals(
                "onNavigationHandedOffToExternalApp was not called.",
                1,
                onHandedOffCallback.getCallCount());
    }
}
