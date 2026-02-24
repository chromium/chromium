// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;

import android.app.SearchManager;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.drawable.GradientDrawable;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.core.content.ContextCompat;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.metrics.UmaActivityObserver;
import org.chromium.chrome.browser.omnibox.LocationBarBackgroundDrawable;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.rlz.RevenueStats;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactoryJni;
import org.chromium.chrome.browser.searchwidget.SearchActivity.TerminationReason;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient.IntentBuilder;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.ResolutionType;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.util.Map;
import java.util.Set;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({
    ChromeFeatureList.PROCESS_RANK_POLICY_ANDROID,
    ChromeFeatureList.UMA_SESSION_CORRECTNESS_FIXES
})
public class SearchActivityUnitTest {
    private static final String TEST_URL = "https://abc.xyz/";
    private static final String TEST_REFERRER = "com.package.name";
    private static final OmniboxLoadUrlParams LOAD_URL_PARAMS_SIMPLE =
            new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED).build();
    private static final String HISTOGRAM_SUFFIX_SEARCH_WIDGET = ".SearchWidget";
    private static final String HISTOGRAM_SUFFIX_SHORTCUTS_WIDGET = ".ShortcutsWidget";
    private static final String HISTOGRAM_SUFFIX_CUSTOM_TAB = ".CustomTab";
    private static final String HISTOGRAM_SUFFIX_LAUNCHER = ".Launcher";
    private static final String HISTOGRAM_SUFFIX_HUB = ".Hub";

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock SearchActivityUtils.TestDelegate mUtils;
    private @Mock TemplateUrlService mTemplateUrlSvc;
    private @Mock Profile mProfile;
    private @Mock TemplateUrlServiceFactoryJni mTemplateUrlFactoryJni;
    private @Mock WebContents mWebContents;
    private @Mock Tab mTab;
    private @Mock SearchActivity.SearchActivityDelegate mDelegate;
    private @Mock SearchActivityLocationBarLayout mLocationBar;
    private @Mock UmaActivityObserver mUmaObserver;
    private @Mock Callback<@Nullable String> mSetCustomTabSearchClient;
    private @Mock LocationBarBackgroundDrawable mSearchBoxBackground;
    private @Mock LocationBarCoordinator mLocationBarCoordinator;
    private @Mock UrlBarCoordinator mUrlCoordinator;
    private @Mock StatusCoordinator mStatusCoordinator;
    private MonotonicObservableSupplier<Profile> mProfileSupplier;
    private OneshotSupplier<ProfileProvider> mProfileProviderSupplier;

    private ActivityController<SearchActivity> mController;
    private SearchActivity mActivity;
    private ShadowActivity mShadowActivity;
    private SearchBoxDataProvider mDataProvider;
    private View mAnchorView;

    @Before
    public void setUp() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        mController = Robolectric.buildActivity(SearchActivity.class);
        mActivity = spy(mController.get());
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mShadowActivity = shadowOf(mActivity);
        mDataProvider = mActivity.getSearchBoxDataProviderForTesting();

        // Many of the scenarios could be tested by simply applying a test instance of the
        // TemplateUrlService to TemplateUrlServiceFactory#setInstanceForTesting.
        // Some scenarios however require Factory to return null, which isn't currently possible.
        TemplateUrlServiceFactoryJni.setInstanceForTesting(mTemplateUrlFactoryJni);
        lenient()
                .doReturn(mTemplateUrlSvc)
                .when(mTemplateUrlFactoryJni)
                .getTemplateUrlService(any());

        mProfileSupplier = mActivity.getProfileSupplierForTesting();

        SearchActivity.setDelegateForTests(mDelegate);
        mActivity.setLocationBarLayoutForTesting(mLocationBar);
        mProfileProviderSupplier = mActivity.createProfileProvider();

        mAnchorView = new View(mActivity);
        GradientDrawable anchorViewBackground = new GradientDrawable();
        anchorViewBackground.setTint(
                ContextCompat.getColor(mActivity, R.color.search_suggestion_bg_color));
        mAnchorView.setBackground(anchorViewBackground);
        mActivity.setAnchorViewForTesting(mAnchorView);
        mActivity.setLocationBarCoordinatorForTesting(mLocationBarCoordinator);

        lenient().when(mLocationBar.getBackground()).thenReturn(mSearchBoxBackground);
        lenient()
                .when(mLocationBarCoordinator.getStatusCoordinator())
                .thenReturn(mStatusCoordinator);
        lenient().when(mLocationBarCoordinator.getUrlBarCoordinator()).thenReturn(mUrlCoordinator);

        SearchActivityUtils.setDelegateForTesting(mUtils);
        WebContentsFactory.setWebContentsForTesting(mWebContents);
        TabBuilder.setTabForTesting(mTab);
        RevenueStats.setCustomTabSearchClientHookForTesting(mSetCustomTabSearchClient);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
    }

    @After
    public void tearDown() {
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        FirstRunStatus.setFirstRunFlowComplete(false);
        IdentityServicesProvider.setInstanceForTests(null);
        TemplateUrlServiceFactory.setInstanceForTesting(null);
    }

    private Intent buildTestWidgetIntent(@IntentOrigin int intentOrigin) {
        return newIntentBuilder(intentOrigin, TEST_URL).build();
    }

    private Intent buildTestServiceIntent(@IntentOrigin int intentOrigin) {
        return newIntentBuilder(intentOrigin, TEST_URL)
                .setResolutionType(ResolutionType.SEND_TO_CALLER)
                .build();
    }

    private IntentBuilder newIntentBuilder(@IntentOrigin int intentOrigin, String url) {
        return new SearchActivityClientImpl(mActivity, intentOrigin)
                .newIntentBuilder()
                .setPageUrl(new GURL(url));
    }

    private void setProfile(Profile profile) {
        ProfileManager.setLastUsedProfileForTesting(profile);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
    }

    @Test
    public void searchActivity_forcesPhoneUi() {
        assertTrue(mActivity.getEmbedderUiOverridesForTesting().isForcedPhoneStyleOmnibox());
    }

    @Test
    public void loadUrl_dispatchResultToCallingActivity() {
        setProfile(mProfile);
        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.CUSTOM_TAB), false);

        ArgumentCaptor<OmniboxLoadUrlParams> captor =
                ArgumentCaptor.forClass(OmniboxLoadUrlParams.class);

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON,
                                TerminationReason.NAVIGATION)
                        .expectIntRecord(
                                SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON
                                        + HISTOGRAM_SUFFIX_CUSTOM_TAB,
                                TerminationReason.NAVIGATION)
                        .build()) {
            mActivity.loadUrl(LOAD_URL_PARAMS_SIMPLE, false);
            verify(mUtils).resolveOmniboxRequestForResult(eq(mActivity), captor.capture());
        }

        assertEquals(TEST_URL, captor.getValue().url);
        assertNull(mShadowActivity.getNextStartedActivity());
    }

    @Test
    public void loadUrl_openInChromeBrowser() {
        setProfile(mProfile);
        mActivity.handleNewIntent(
                buildTestWidgetIntent(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET), false);

        mActivity.loadUrl(LOAD_URL_PARAMS_SIMPLE, false);
        verify(mUtils, never()).resolveOmniboxRequestForResult(any(), any());
        assertNotNull(mShadowActivity.getNextStartedActivity());
    }

    @Test
    public void terminateSession_dispatchResultToCallingActivity() {
        var intent = buildTestServiceIntent(IntentOrigin.CUSTOM_TAB);
        mActivity.handleNewIntent(intent, false);
        clearInvocations(mUtils);

        // Start at the first non-NAVIGATION reason. NAVIGATION is covered by a separate test
        // because it resolves termination slightly differently.
        for (@TerminationReason int reason = TerminationReason.NAVIGATION + 1;
                reason < TerminationReason.COUNT;
                reason++) {
            try (var watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON, reason)
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON
                                            + HISTOGRAM_SUFFIX_CUSTOM_TAB,
                                    reason)
                            .build()) {
                mActivity.finish(reason, null);
                verify(mUtils).resolveOmniboxRequestForResult(mActivity, null);
                clearInvocations(mUtils);

                // Subsequent calls must be ignored.
                mActivity.finish(reason, null);
                verifyNoMoreInteractions(mUtils);

                mShadowActivity.resetIsFinishing();
            }
        }
    }

    @Test
    public void terminateSession_startsChrome() {
        var intent =
                newIntentBuilder(IntentOrigin.LAUNCHER, TEST_URL)
                        .setResolutionType(ResolutionType.OPEN_OR_LAUNCH_CHROME)
                        .build();
        mActivity.handleNewIntent(intent, false);
        clearInvocations(mUtils);

        // Start at the first non-NAVIGATION reason. NAVIGATION is covered by a separate test
        // because it resolves termination slightly differently.
        for (@TerminationReason int reason = TerminationReason.NAVIGATION + 1;
                reason < TerminationReason.COUNT;
                reason++) {
            try (var watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON, reason)
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON
                                            + HISTOGRAM_SUFFIX_LAUNCHER,
                                    reason)
                            .build()) {
                mActivity.finish(reason, null);

                Intent nextStartedActivity = mShadowActivity.getNextStartedActivity();
                assertNotNull(nextStartedActivity);
                assertNull(nextStartedActivity.getData());
                verifyNoMoreInteractions(mUtils);
                clearInvocations(mUtils);

                // Subsequent calls must be ignored.
                mShadowActivity.clearNextStartedActivities();
                mActivity.finish(reason, null);
                assertNull(mShadowActivity.getNextStartedActivity());
                mShadowActivity.resetIsFinishing();
            }
        }
    }

    @Test
    public void terminateSession_searchWidget() {
        mActivity.handleNewIntent(buildTestWidgetIntent(IntentOrigin.SEARCH_WIDGET), false);
        clearInvocations(mUtils);

        for (@TerminationReason int reason = TerminationReason.NAVIGATION;
                reason < TerminationReason.COUNT;
                reason++) {
            try (var watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON, reason)
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON
                                            + HISTOGRAM_SUFFIX_SEARCH_WIDGET,
                                    reason)
                            .build()) {
                mActivity.finish(reason, null);
                verifyNoMoreInteractions(mUtils);

                // Verify that termination reason is recorded exactly once.
                mActivity.finish(reason, null);
                verifyNoMoreInteractions(mUtils);
                mShadowActivity.resetIsFinishing();
            }
        }
        verify(mUtils, never()).resolveOmniboxRequestForResult(any(), any());
    }

    @Test
    public void terminateSession_shortcutsWidget() {
        mActivity.handleNewIntent(
                buildTestWidgetIntent(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET), false);
        clearInvocations(mUtils);

        for (@TerminationReason int reason = TerminationReason.NAVIGATION;
                reason < TerminationReason.COUNT;
                reason++) {
            try (var watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON, reason)
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON
                                            + HISTOGRAM_SUFFIX_SHORTCUTS_WIDGET,
                                    reason)
                            .build()) {
                mActivity.finish(reason, null);
                verifyNoMoreInteractions(mUtils);

                // Verify that termination reason is recorded exactly once.
                mActivity.finish(reason, null);
                verifyNoMoreInteractions(mUtils);
                mShadowActivity.resetIsFinishing();
            }
        }
        verify(mUtils, never()).resolveOmniboxRequestForResult(any(), any());
    }

    @Test
    public void handleNewIntent_forHubSearch() {
        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.HUB), false);

        assertEquals(
                PageClassification.ANDROID_HUB_VALUE,
                mDataProvider.getPageClassification(/* prefetch= */ true));
        assertEquals(
                PageClassification.ANDROID_HUB_VALUE,
                mDataProvider.getPageClassification(/* prefetch= */ false));
        assertFalse(mActivity.getEmbedderUiOverridesForTesting().isLensEntrypointAllowed());
        assertFalse(mActivity.getEmbedderUiOverridesForTesting().isVoiceEntrypointAllowed());

        verify(mStatusCoordinator).setOnStatusIconNavigateBackButtonPress(any());
    }

    @Test
    public void exitSearchViaCustomBackArrow_HubSearch() {
        View view = mock(View.class);

        ArgumentCaptor<OnClickListener> captor = ArgumentCaptor.forClass(OnClickListener.class);
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON
                                        + HISTOGRAM_SUFFIX_HUB,
                                TerminationReason.CUSTOM_BACK_ARROW)
                        .build();

        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.HUB), false);
        verify(mStatusCoordinator).setOnStatusIconNavigateBackButtonPress(captor.capture());
        OnClickListener listener = captor.getValue();
        listener.onClick(view);
        histograms.assertExpected();
    }

    @Test
    public void cancelHubSearch_onBackKeyPressed() {
        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.HUB), false);
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON
                                        + HISTOGRAM_SUFFIX_HUB,
                                TerminationReason.BACK_KEY_PRESSED)
                        .build();

        assertFalse(mActivity.isFinishing());
        assertFalse(mActivity.isActivityFinishingOrDestroyed());
        mActivity.handleBackKeyPressed();
        assertTrue(mActivity.isActivityFinishingOrDestroyed());
        assertTrue(mActivity.isFinishing());
        histograms.assertExpected();
    }

    @Test
    public void handleNewIntent_forJumpStartOmnibox() {
        // Jump-start Omnibox relies on cached data above anything else.
        // Save some data to confirm it's properly picked.
        String jumpStartUrl = "https://asdf.com/ghjkl";
        CachedZeroSuggestionsManager.saveJumpStartContext(
                new CachedZeroSuggestionsManager.JumpStartContext(new GURL(jumpStartUrl), 123));

        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.LAUNCHER), false);

        assertEquals(123, mDataProvider.getPageClassification(/* prefetch= */ true));
        assertEquals(123, mDataProvider.getPageClassification(/* prefetch= */ false));
        assertEquals(jumpStartUrl, mDataProvider.getCurrentGurl().getSpec());
    }

    @Test
    public void handleNewIntent_forSearchWidget() {
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SearchActivity.HISTOGRAM_INTENT_ORIGIN, IntentOrigin.SEARCH_WIDGET)) {
            mActivity.handleNewIntent(buildTestWidgetIntent(IntentOrigin.SEARCH_WIDGET), false);
        }

        assertEquals(
                PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                mDataProvider.getPageClassification(/* prefetch= */ true));
        assertEquals(
                PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                mDataProvider.getPageClassification(/* prefetch= */ false));
        assertFalse(mActivity.getEmbedderUiOverridesForTesting().isLensEntrypointAllowed());
        assertTrue(mActivity.getEmbedderUiOverridesForTesting().isVoiceEntrypointAllowed());
    }

    @Test
    public void handleNewIntent_forQuickActionSearchWidget() {
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SearchActivity.HISTOGRAM_INTENT_ORIGIN,
                        IntentOrigin.QUICK_ACTION_SEARCH_WIDGET)) {
            mActivity.handleNewIntent(
                    buildTestWidgetIntent(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET), false);
        }

        assertEquals(
                PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE,
                mDataProvider.getPageClassification(/* prefetch= */ true));
        assertEquals(
                PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE,
                mDataProvider.getPageClassification(/* prefetch= */ false));
        assertTrue(mActivity.getEmbedderUiOverridesForTesting().isLensEntrypointAllowed());
        assertTrue(mActivity.getEmbedderUiOverridesForTesting().isVoiceEntrypointAllowed());
    }

    @Test
    public void handleNewIntent_forCustomTabNoProfile() {
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SearchActivity.HISTOGRAM_INTENT_ORIGIN, IntentOrigin.CUSTOM_TAB)) {
            mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.CUSTOM_TAB), false);
        }

        assertEquals(
                PageClassification.OTHER_ON_CCT_VALUE,
                mDataProvider.getPageClassification(/* prefetch= */ true));
        assertEquals(
                PageClassification.OTHER_ON_CCT_VALUE,
                mDataProvider.getPageClassification(/* prefetch= */ false));
        assertFalse(mActivity.getEmbedderUiOverridesForTesting().isLensEntrypointAllowed());
        assertFalse(mActivity.getEmbedderUiOverridesForTesting().isVoiceEntrypointAllowed());

        // Note that the profile is not available at this point, so we should not attempt to refine
        // the page class.
        verifyNoMoreInteractions(mTemplateUrlFactoryJni, mTemplateUrlSvc);
    }

    @Test
    public void handleNewIntent_passIntentUrlToLocationBarData() {
        mActivity.handleNewIntent(buildTestWidgetIntent(IntentOrigin.SEARCH_WIDGET), false);
        assertEquals(TEST_URL, mDataProvider.getCurrentGurl().getSpec());
    }

    @Test
    public void recordUsage_searchWidget() {
        int[] searchTypes = new int[] {SearchType.TEXT, SearchType.VOICE, SearchType.LENS};

        for (var searchType : searchTypes) {
            var tester = new UserActionTester();

            SearchActivity.recordUsage(IntentOrigin.SEARCH_WIDGET, searchType);

            var actions = tester.getActions();
            assertEquals(1, actions.size());
            assertEquals(SearchActivity.USED_ANY_FROM_SEARCH_WIDGET, actions.get(0));

            tester.tearDown();
        }
    }

    @Test
    public void recordUsage_customTabs() {
        int[] searchTypes = new int[] {SearchType.TEXT, SearchType.VOICE, SearchType.LENS};

        for (var searchType : searchTypes) {
            var tester = new UserActionTester();

            SearchActivity.recordUsage(IntentOrigin.CUSTOM_TAB, searchType);

            var actions = tester.getActions();
            assertEquals(0, actions.size());

            tester.tearDown();
        }
    }

    @Test
    public void recordUsage_shortcutsWidget() {
        var searchTypes =
                Map.of(
                        SearchType.TEXT,
                        SearchActivity.USED_TEXT_FROM_SHORTCUTS_WIDGET,
                        SearchType.VOICE,
                        SearchActivity.USED_VOICE_FROM_SHORTCUTS_WIDGET,
                        SearchType.LENS,
                        SearchActivity.USED_LENS_FROM_SHORTCUTS_WIDGET,
                        // Invalid search type.
                        ~0,
                        "");

        for (var searchType : searchTypes.entrySet()) {
            var tester = new UserActionTester();

            SearchActivity.recordUsage(
                    IntentOrigin.QUICK_ACTION_SEARCH_WIDGET, searchType.getKey());
            var value = searchType.getValue();
            var actions = tester.getActions();
            if (value.isEmpty()) {
                assertEquals(0, actions.size());
            } else {
                assertEquals(1, actions.size());
                assertEquals(value, actions.get(0));
            }

            tester.tearDown();
        }
    }

    @Test
    public void recordUsage_unknownOrigins() {
        var originsToSkip =
                Set.of(
                        IntentOrigin.SEARCH_WIDGET,
                        IntentOrigin.QUICK_ACTION_SEARCH_WIDGET,
                        IntentOrigin.CUSTOM_TAB,
                        IntentOrigin.HUB);
        int[] searchTypes = new int[] {SearchType.TEXT, SearchType.VOICE, SearchType.LENS};

        for (int origin = 0; origin < 10; origin++) {
            if (originsToSkip.contains(origin)) continue;

            for (int searchType : searchTypes) {
                var tester = new UserActionTester();

                SearchActivity.recordUsage(origin, searchType);
                assertEquals(0, tester.getActions().size());

                tester.tearDown();
            }
        }
    }

    @Test
    public void refinePageClassWithProfile_noRefinementForSearchWidget() {
        doReturn(true).when(mTemplateUrlSvc).isSearchResultsPageFromDefaultSearchProvider(any());
        mActivity.handleNewIntent(buildTestWidgetIntent(IntentOrigin.SEARCH_WIDGET), false);

        assertEquals(
                PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                mDataProvider.getPageClassification(/* prefetch= */ false));
        verifyNoMoreInteractions(mTemplateUrlSvc);
    }

    @Test
    public void refinePageClassWithProfile_noRefinementForShortcutsWidget() {
        doReturn(true).when(mTemplateUrlSvc).isSearchResultsPageFromDefaultSearchProvider(any());
        mActivity.handleNewIntent(
                buildTestWidgetIntent(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET), false);

        assertEquals(
                PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE,
                mDataProvider.getPageClassification(/* prefetch= */ false));
        verifyNoMoreInteractions(mTemplateUrlSvc);
    }

    @Test
    public void refinePageClassWithProfile_refinesBasicUrlForSearchResultsPage() {
        setProfile(mProfile);

        {
            // Simulate Search Results Page.
            doReturn(true)
                    .when(mTemplateUrlSvc)
                    .isSearchResultsPageFromDefaultSearchProvider(any());
            mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.CUSTOM_TAB), false);
            assertEquals(
                    PageClassification.SEARCH_RESULT_PAGE_ON_CCT_VALUE,
                    mDataProvider.getPageClassification(/* prefetch= */ false));
            assertEquals(
                    PageClassification.SEARCH_RESULT_PAGE_ON_CCT_VALUE,
                    mDataProvider.getPageClassification(/* prefetch= */ true));
        }

        {
            // Simulate arbitrary page.
            doReturn(false)
                    .when(mTemplateUrlSvc)
                    .isSearchResultsPageFromDefaultSearchProvider(any());
            mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.CUSTOM_TAB), false);
            assertEquals(
                    PageClassification.OTHER_ON_CCT_VALUE,
                    mDataProvider.getPageClassification(/* prefetch= */ false));
            assertEquals(
                    PageClassification.OTHER_ON_CCT_VALUE,
                    mDataProvider.getPageClassification(/* prefetch= */ true));
        }
    }

    @Test
    public void refinePageClassWithProfile_ignoresNullUrl() {
        doReturn(true).when(mTemplateUrlSvc).isSearchResultsPageFromDefaultSearchProvider(any());
        mActivity.handleNewIntent(
                newIntentBuilder(IntentOrigin.CUSTOM_TAB, /* url= */ null)
                        .setResolutionType(ResolutionType.SEND_TO_CALLER)
                        .build(),
                false);

        assertEquals(
                PageClassification.OTHER_ON_CCT_VALUE,
                mDataProvider.getPageClassification(/* prefetch= */ false));
        verifyNoMoreInteractions(mTemplateUrlSvc);
    }

    @Test
    public void refinePageClassWithProfile_ignoresEmptyUrl() {
        doReturn(true).when(mTemplateUrlSvc).isSearchResultsPageFromDefaultSearchProvider(any());
        mActivity.handleNewIntent(
                newIntentBuilder(IntentOrigin.CUSTOM_TAB, /* url= */ "")
                        .setResolutionType(ResolutionType.SEND_TO_CALLER)
                        .build(),
                false);

        assertEquals(
                PageClassification.OTHER_ON_CCT_VALUE,
                mDataProvider.getPageClassification(/* prefetch= */ false));
        verifyNoMoreInteractions(mTemplateUrlSvc);
    }

    @Test
    public void refinePageClassWithProfile_ignoresInvalidUrl() {
        doReturn(true).when(mTemplateUrlSvc).isSearchResultsPageFromDefaultSearchProvider(any());
        mActivity.handleNewIntent(
                newIntentBuilder(IntentOrigin.CUSTOM_TAB, /* url= */ "aoeui")
                        .setResolutionType(ResolutionType.SEND_TO_CALLER)
                        .build(),
                false);

        assertEquals(
                PageClassification.OTHER_ON_CCT_VALUE,
                mDataProvider.getPageClassification(/* prefetch= */ false));
        verifyNoMoreInteractions(mTemplateUrlSvc);
    }

    @Test
    public void refinePageClassWithProfile_noTemplateUrl() {
        doReturn(null).when(mTemplateUrlFactoryJni).getTemplateUrlService(any());

        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.CUSTOM_TAB), false);

        assertEquals(
                PageClassification.OTHER_ON_CCT_VALUE,
                mDataProvider.getPageClassification(/* prefetch= */ false));
    }

    @Test
    public void finishNativeInitialization_stopActivityWhenSearchEnginePromoCanceled() {
        mActivity.handleNewIntent(new Intent(), false);
        doNothing().when(mActivity).finishDeferredInitialization();

        setProfile(mProfile);
        mActivity.finishNativeInitialization();

        ArgumentCaptor<Callback<Boolean>> captor = ArgumentCaptor.forClass(Callback.class);
        verify(mDelegate).showSearchEngineDialogIfNeeded(eq(mActivity), captor.capture());

        // Notify Activity that the search engine promo dialog was canceled.
        captor.getValue().onResult(false);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        verify(mActivity, never()).finishDeferredInitialization();
        assertTrue(mActivity.isFinishing());
    }

    @Test
    public void finishNativeInitialization_stopActivityWhenSearchEnginePromoFailed() {
        mActivity.handleNewIntent(new Intent(), false);
        doNothing().when(mActivity).finishDeferredInitialization();

        setProfile(mProfile);
        mActivity.finishNativeInitialization();

        ArgumentCaptor<Callback<Boolean>> captor = ArgumentCaptor.forClass(Callback.class);
        verify(mDelegate).showSearchEngineDialogIfNeeded(eq(mActivity), captor.capture());

        // "should never happen".
        captor.getValue().onResult(null);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        verify(mActivity, never()).finishDeferredInitialization();
        assertTrue(mActivity.isFinishing());
    }

    @Test
    public void finishNativeInitialization_resumeActivityAfterSearchEnginePromoCleared() {
        doNothing().when(mActivity).finishDeferredInitialization();
        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.UNKNOWN), false);

        setProfile(mProfile);
        mActivity.finishNativeInitialization();

        ArgumentCaptor<Callback<Boolean>> captor = ArgumentCaptor.forClass(Callback.class);
        verify(mDelegate).showSearchEngineDialogIfNeeded(eq(mActivity), captor.capture());

        // Notify Activity that the search engine promo dialog was completed.
        captor.getValue().onResult(true);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        verify(mActivity).finishDeferredInitialization();
        assertFalse(mActivity.isFinishing());
    }

    @Test
    public void finish_recordsUnspecifiedTerminationReason() {
        mActivity.handleNewIntent(new Intent(), false);

        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON,
                        TerminationReason.UNSPECIFIED)) {
            mActivity.finish();
        }
    }

    @Test
    public void finishNativeInitialization_abortIfActivityTerminated() {
        mActivity.handleNewIntent(new Intent(), false);
        doNothing().when(mActivity).finishDeferredInitialization();

        setProfile(mProfile);
        mActivity.finishNativeInitialization();

        ArgumentCaptor<Callback<Boolean>> captor = ArgumentCaptor.forClass(Callback.class);
        verify(mDelegate).showSearchEngineDialogIfNeeded(eq(mActivity), captor.capture());

        // Cancel activity, and notify that the search engine promo dialog was completed.
        mActivity.finish(TerminationReason.ACTIVITY_FOCUS_LOST, null);
        captor.getValue().onResult(true);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        verify(mActivity, never()).finishDeferredInitialization();
    }

    @Test
    @DisableFeatures({OmniboxFeatureList.ANDROID_HUB_SEARCH_TAB_GROUPS})
    public void finishNativeInitialization_setHubSearchBoxUrlBarElements() {
        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.HUB), false);

        setProfile(mProfile);
        mActivity.finishNativeInitialization();

        String expectedText = mActivity.getResources().getString(R.string.hub_search_empty_hint);

        verify(mUrlCoordinator).setUrlBarHintText(expectedText);
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.ANDROID_HUB_SEARCH_TAB_GROUPS})
    public void finishNativeInitialization_setHubSearchBoxUrlBarElements_withTabGroups() {
        OmniboxFeatures.sAndroidHubSearchEnableTabGroupStrings.setForTesting(true);

        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.HUB), false);

        setProfile(mProfile);
        mActivity.finishNativeInitialization();

        String expectedText =
                mActivity.getResources().getString(R.string.hub_search_empty_hint_with_tab_groups);

        verify(mUrlCoordinator).setUrlBarHintText(expectedText);
    }

    @Test
    public void cancelSearch_onBackKeyPressed() {
        mActivity.handleNewIntent(new Intent(), false);

        assertFalse(mActivity.isFinishing());
        assertFalse(mActivity.isActivityFinishingOrDestroyed());
        mActivity.handleBackKeyPressed();
        assertTrue(mActivity.isActivityFinishingOrDestroyed());
        assertTrue(mActivity.isFinishing());
    }

    @Test
    public void cancelSearch_onBackGesture() {
        // Same as above, but with predictive back gesture enabled.
        mActivity.handleNewIntent(new Intent(), false);

        assertFalse(mActivity.isFinishing());
        assertFalse(mActivity.isActivityFinishingOrDestroyed());
        mActivity.getOnBackPressedDispatcher().onBackPressed();
        assertTrue(mActivity.isActivityFinishingOrDestroyed());
        assertTrue(mActivity.isFinishing());
    }

    @Test
    public void cancelSearch_onTapOutside() {
        mActivity.handleNewIntent(new Intent(), false);

        assertFalse(mActivity.isFinishing());
        assertFalse(mActivity.isActivityFinishingOrDestroyed());
        var view = mActivity.createContentView();
        view.performClick();
        assertTrue(mActivity.isActivityFinishingOrDestroyed());
        assertTrue(mActivity.isFinishing());
    }

    @Test
    public void createProfileProvider_tracksProfileManager() {
        assertNull(mProfileSupplier.get());
        setProfile(mProfile);
        assertEquals(mProfile, mProfileSupplier.get());
    }

    @Test
    public void onNewIntent_applyQuery() {
        ArgumentCaptor<AutocompleteInput> captor = ArgumentCaptor.forClass(AutocompleteInput.class);

        var intent = buildTestWidgetIntent(IntentOrigin.SEARCH_WIDGET);
        intent.putExtra(SearchManager.QUERY, "query1");
        mActivity.onNewIntent(intent);
        verify(mLocationBar).beginQuery(eq(IntentOrigin.SEARCH_WIDGET), eq(SearchType.TEXT), any());
        verify(mLocationBarCoordinator).setUrlBarFocus(captor.capture());
        assertEquals("query1", captor.getValue().getUserText());
        clearInvocations(mLocationBar, mLocationBarCoordinator);

        intent = buildTestWidgetIntent(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET);
        intent.putExtra(SearchManager.QUERY, "query2");
        mActivity.onNewIntent(intent);
        verify(mLocationBar)
                .beginQuery(
                        eq(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET), eq(SearchType.TEXT), any());
        verify(mLocationBarCoordinator).setUrlBarFocus(captor.capture());
        assertEquals("query2", captor.getValue().getUserText());
        clearInvocations(mLocationBar, mLocationBarCoordinator);

        intent = buildTestWidgetIntent(IntentOrigin.SEARCH_WIDGET);
        intent.putExtra(SearchManager.QUERY, "");
        mActivity.onNewIntent(intent);
        verify(mLocationBar).beginQuery(eq(IntentOrigin.SEARCH_WIDGET), eq(SearchType.TEXT), any());
        verify(mLocationBarCoordinator).setUrlBarFocus(captor.capture());
        assertEquals("", captor.getValue().getUserText());
        clearInvocations(mLocationBar, mLocationBarCoordinator);

        intent = buildTestServiceIntent(IntentOrigin.CUSTOM_TAB);
        intent.removeExtra(SearchManager.QUERY);
        mActivity.onNewIntent(intent);
        verify(mLocationBar).beginQuery(eq(IntentOrigin.CUSTOM_TAB), eq(SearchType.TEXT), any());
        verify(mLocationBarCoordinator).setUrlBarFocus(captor.capture());
        assertEquals("", captor.getValue().getUserText());
    }

    @Test
    public void onResumeWithNative_fromSearchWidget() {
        mActivity.onNewIntent(buildTestWidgetIntent(IntentOrigin.SEARCH_WIDGET));
        mActivity.setUmaActivityObserverForTesting(mUmaObserver);
        mActivity.onResumeWithNative();

        verify(mUmaObserver).startUmaSession(eq(null), any());
        verifyNoMoreInteractions(mUmaObserver, mSetCustomTabSearchClient);
    }

    @Test
    public void onResumeWithNative_fromQuickActionWidget() {
        mActivity.onNewIntent(buildTestWidgetIntent(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET));
        mActivity.setUmaActivityObserverForTesting(mUmaObserver);
        mActivity.onResumeWithNative();

        verify(mUmaObserver).startUmaSession(eq(null), any());
        verifyNoMoreInteractions(mUmaObserver, mSetCustomTabSearchClient);
    }

    @Test
    public void onResumeWithNative_fromCustomTabs_withoutPackage() {
        ChromeFeatureList.sSearchinCctApplyReferrerId.setForTesting(true);
        mActivity.onNewIntent(buildTestServiceIntent(IntentOrigin.CUSTOM_TAB));
        mActivity.setUmaActivityObserverForTesting(mUmaObserver);

        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SearchActivity.HISTOGRAM_INTENT_REFERRER_VALID, false)) {
            mActivity.onResumeWithNative();
        }

        verify(mUmaObserver).startUmaSession(eq(null), any());
        verify(mSetCustomTabSearchClient).onResult(null);
        verifyNoMoreInteractions(mUmaObserver, mSetCustomTabSearchClient);
    }

    @Test
    public void onResumeWithNative_fromCustomTabs_withPackage() {
        ChromeFeatureList.sSearchinCctApplyReferrerId.setForTesting(true);
        mActivity.onNewIntent(
                newIntentBuilder(IntentOrigin.CUSTOM_TAB, TEST_URL)
                        .setReferrer(TEST_REFERRER)
                        .setResolutionType(ResolutionType.SEND_TO_CALLER)
                        .build());
        mActivity.setUmaActivityObserverForTesting(mUmaObserver);

        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SearchActivity.HISTOGRAM_INTENT_REFERRER_VALID, true)) {
            mActivity.onResumeWithNative();
        }

        verify(mUmaObserver).startUmaSession(eq(null), any());
        verify(mSetCustomTabSearchClient).onResult("app-cct-" + TEST_REFERRER);
        verifyNoMoreInteractions(mUmaObserver, mSetCustomTabSearchClient);
    }

    @Test
    public void onResumeWithNative_fromCustomTabs_propagationDisabled() {
        ChromeFeatureList.sSearchinCctApplyReferrerId.setForTesting(false);
        mActivity.onNewIntent(buildTestServiceIntent(IntentOrigin.CUSTOM_TAB));
        mActivity.setUmaActivityObserverForTesting(mUmaObserver);

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(SearchActivity.HISTOGRAM_INTENT_REFERRER_VALID)
                        .build()) {
            mActivity.onResumeWithNative();
        }

        verify(mUmaObserver).startUmaSession(eq(null), any());
        verify(mSetCustomTabSearchClient, never()).onResult(any());
        verifyNoMoreInteractions(mUmaObserver, mSetCustomTabSearchClient);
    }

    @Test
    public void onPauseWithNative() {
        mActivity.onPauseWithNative();

        verify(mSetCustomTabSearchClient).onResult(null);
        verifyNoMoreInteractions(mSetCustomTabSearchClient);
    }

    @Test
    public void shouldStartGpuProcess_alwaysTrue() {
        assertTrue(mActivity.shouldStartGpuProcess());
    }

    @Test
    public void onUrlFocusChange_propagateFocusGainEvent() {
        LocationBarCoordinator coordinator = mock(LocationBarCoordinator.class);
        mActivity.setLocationBarCoordinatorForTesting(coordinator);
        mActivity.onUrlFocusChange(true);
        verify(coordinator).setUrlFocusChangeInProgress(false);
        assertFalse(mActivity.isFinishing());
    }

    @Test
    public void recordNavigationTargetType() {
        setProfile(mProfile);
        GURL nativeUrl = new GURL(getOriginalNativeNtpUrl());
        GURL searchUrl = new GURL("https://google.com");
        GURL webUrl = new GURL("https://abc.xyz");

        var variants =
                Map.of(
                        nativeUrl, SearchActivity.NavigationTargetType.NATIVE_PAGE,
                        searchUrl, SearchActivity.NavigationTargetType.SEARCH,
                        webUrl, SearchActivity.NavigationTargetType.URL);

        doReturn(true)
                .when(mTemplateUrlSvc)
                .isSearchResultsPageFromDefaultSearchProvider(searchUrl);

        for (var entry : variants.entrySet()) {
            var type = entry.getValue();
            try (var watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(SearchActivity.HISTOGRAM_NAVIGATION_TARGET_TYPE, type)
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_NAVIGATION_TARGET_TYPE
                                            + HISTOGRAM_SUFFIX_SEARCH_WIDGET,
                                    type)
                            .build()) {
                mActivity.onNewIntent(buildTestWidgetIntent(IntentOrigin.SEARCH_WIDGET));
                mActivity.recordNavigationTargetType(entry.getKey());
            }

            try (var watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(SearchActivity.HISTOGRAM_NAVIGATION_TARGET_TYPE, type)
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_NAVIGATION_TARGET_TYPE
                                            + HISTOGRAM_SUFFIX_SHORTCUTS_WIDGET,
                                    type)
                            .build()) {
                mActivity.onNewIntent(
                        buildTestWidgetIntent(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET));
                mActivity.recordNavigationTargetType(entry.getKey());
            }

            try (var watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(SearchActivity.HISTOGRAM_NAVIGATION_TARGET_TYPE, type)
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_NAVIGATION_TARGET_TYPE
                                            + HISTOGRAM_SUFFIX_CUSTOM_TAB,
                                    type)
                            .build()) {
                mActivity.onNewIntent(buildTestServiceIntent(IntentOrigin.CUSTOM_TAB));
                mActivity.recordNavigationTargetType(entry.getKey());
            }
        }
    }

    @Test
    public void onTopResumedActivityChanged_clearOmniboxFocusIfNotActive() {
        doNothing().when(mActivity).super_onTopResumedActivityChanged(anyBoolean());
        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.SEARCH_WIDGET), false);
        mActivity.onTopResumedActivityChanged(false);
        verify(mLocationBar).clearOmniboxFocus();
        verify(mActivity).super_onTopResumedActivityChanged(false);
    }

    @Test
    public void onTopResumedActivityChanged_requestOmniboxFocusIfActive() {
        doNothing().when(mActivity).super_onTopResumedActivityChanged(anyBoolean());
        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.SEARCH_WIDGET), false);
        mActivity.onTopResumedActivityChanged(true);
        verify(mLocationBar).requestOmniboxFocus();
        verify(mActivity).super_onTopResumedActivityChanged(true);
    }

    @Test
    public void onTopResumedActivityChanged_finishActivityFocusLostHubSearch() {
        doNothing().when(mActivity).super_onTopResumedActivityChanged(anyBoolean());
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON
                                        + HISTOGRAM_SUFFIX_HUB,
                                TerminationReason.ACTIVITY_FOCUS_LOST)
                        .build();

        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.HUB), false);
        mActivity.onTopResumedActivityChanged(false);
        histograms.assertExpected();
    }

    @Test
    public void verifySearchBoxColorScheme_toggleIncognitoStatus() {
        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.HUB), false);

        // Assert that the search box has the correct color scheme on inflation.
        assertEquals(
                ColorStateList.valueOf(mActivity.getColor(R.color.omnibox_suggestion_dropdown_bg)),
                ((GradientDrawable) mAnchorView.getBackground()).getColor());
        verify(mSearchBoxBackground)
                .setBackgroundColor(
                        ContextCompat.getColor(mActivity, R.color.search_suggestion_bg_color));

        // Toggle the incognito state and check that the search box has the correct color scheme.
        mDataProvider.setIsIncognitoForTesting(true);
        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.HUB), false);

        assertEquals(
                ColorStateList.valueOf(mActivity.getColor(R.color.omnibox_dropdown_bg_incognito)),
                ((GradientDrawable) mAnchorView.getBackground()).getColor());
        verify(mSearchBoxBackground)
                .setBackgroundColor(
                        ContextCompat.getColor(
                                mActivity, R.color.toolbar_text_box_background_incognito));

        // Toggle to non-incognito and check that the search box has the correct color scheme.
        mDataProvider.setIsIncognitoForTesting(false);
        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.HUB), false);

        assertEquals(
                ColorStateList.valueOf(mActivity.getColor(R.color.omnibox_suggestion_dropdown_bg)),
                ((GradientDrawable) mAnchorView.getBackground()).getColor());
        verify(mSearchBoxBackground, times(2))
                .setBackgroundColor(
                        ContextCompat.getColor(mActivity, R.color.search_suggestion_bg_color));
    }
}
