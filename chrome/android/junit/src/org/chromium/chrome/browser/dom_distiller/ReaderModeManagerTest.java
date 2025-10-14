// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Looper;
import android.util.Pair;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;

import org.chromium.base.Callback;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager.DistillationResult;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager.DistillationStatus;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager.EntryPoint;
import org.chromium.chrome.browser.dom_distiller.TabDistillabilityProvider.DistillabilityObserver;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.components.ukm.UkmRecorderJni;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.concurrent.TimeoutException;

/** This class tests the behavior of the {@link ReaderModeManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ReaderModeManagerTest {
    private static final GURL MOCK_DISTILLER_URL = new GURL("chrome-distiller://url");
    private static final GURL MOCK_URL = JUnitTestGURLs.GOOGLE_URL_CAT;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private Tab mTab;
    @Mock private MockWebContents mWebContents;
    @Mock private TabDistillabilityProvider mDistillabilityProvider;
    @Mock private NavigationController mNavController;
    @Mock private DomDistillerTabUtils.Natives mDistillerTabUtilsJniMock;
    @Mock private DomDistillerUrlUtils.Natives mDistillerUrlUtilsJniMock;
    @Mock private NavigationHandle mNavigationHandle;
    @Mock private MessageDispatcher mMessageDispatcher;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefService mPrefService;
    @Mock private UkmRecorder.Natives mUkmRecorderJniMock;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private LoadCommittedDetails mLoadCommitedDetails;
    @Mock private Activity mActivity;
    @Mock private Resources mResources;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    private TabObserver mTabObserver;

    @Captor private ArgumentCaptor<DistillabilityObserver> mDistillabilityObserverCaptor;
    private DistillabilityObserver mDistillabilityObserver;

    @Captor private ArgumentCaptor<WebContentsObserver> mWebContentsObserverCaptor;
    private WebContentsObserver mWebContentsObserver;

    @Captor private ArgumentCaptor<Callback<Boolean>> mDistillationCallbackCaptor;

    private UserDataHost mUserDataHost;
    private UnownedUserDataHost mUnownedUserDataHost;
    private ReaderModeManager mManager;
    private OneshotSupplierImpl<Boolean> mButtonVisibilitySupplier;

    @Before
    public void setUp() throws TimeoutException {
        org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtilsJni.setInstanceForTesting(
                mDistillerTabUtilsJniMock);
        DomDistillerUrlUtilsJni.setInstanceForTesting(mDistillerUrlUtilsJniMock);
        DomDistillerTabUtils.setDistillerHeuristicsForTesting(
                DistillerHeuristicsType.ADABOOST_MODEL);
        UkmRecorderJni.setInstanceForTesting(mUkmRecorderJniMock);

        mUserDataHost = new UserDataHost();
        mUserDataHost.setUserData(TabDistillabilityProvider.USER_DATA_KEY, mDistillabilityProvider);

        mUnownedUserDataHost = new UnownedUserDataHost();
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mUnownedUserDataHost);
        SnackbarManagerProvider.attach(mWindowAndroid, mSnackbarManager);

        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getUrl()).thenReturn(MOCK_URL);
        when(mTab.getContext()).thenReturn(ApplicationProvider.getApplicationContext());
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mWebContents.getNavigationController()).thenReturn(mNavController);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWebContents.getTitle()).thenReturn("Test Title");
        when(mNavController.getUseDesktopUserAgent()).thenReturn(false);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.READER_FOR_ACCESSIBILITY)).thenReturn(false);

        when(mDistillerUrlUtilsJniMock.isDistilledPage(MOCK_DISTILLER_URL.getSpec()))
                .thenReturn(true);

        when(mDistillerUrlUtilsJniMock.getOriginalUrlFromDistillerUrl(MOCK_DISTILLER_URL.getSpec()))
                .thenReturn(MOCK_URL);

        when(mDistillerUrlUtilsJniMock.getDistillerViewUrlFromUrl(
                        eq("chrome-distiller"), eq(MOCK_URL.getSpec()), eq("Test Title")))
                .thenReturn(MOCK_DISTILLER_URL.getSpec());

        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mActivity.getResources()).thenReturn(mResources);
        when(mActivity.getPackageName())
                .thenReturn(ApplicationProvider.getApplicationContext().getPackageName());
        Configuration configuration = new Configuration();
        configuration.uiMode = Configuration.UI_MODE_NIGHT_NO;
        when(mResources.getConfiguration()).thenReturn(configuration);

        mManager = new ReaderModeManager(mTab, () -> mMessageDispatcher);

        // Ensure the tab observer is attached when the manager is created.
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        mTabObserver = mTabObserverCaptor.getValue();

        // Ensure the distillability observer is attached when the tab is shown.
        mTabObserver.onShown(mTab, 0);
        verify(mDistillabilityProvider).addObserver(mDistillabilityObserverCaptor.capture());
        mDistillabilityObserver = mDistillabilityObserverCaptor.getValue();

        // An observer should have also been added to the web contents.
        verify(mWebContents).addObserver(mWebContentsObserverCaptor.capture());
        mWebContentsObserver = mWebContentsObserverCaptor.getValue();
        mManager.clearSavedSitesForTesting();
        mButtonVisibilitySupplier = new OneshotSupplierImpl<Boolean>();
    }

    @Test
    @Feature("ReaderMode")
    public void testMobileFriendlyNotDistillable() {
        Pair<Boolean, Integer> result =
                ReaderModeManager.computeDistillationStatus(mTab, true, true, true);
        assertTrue("Distillability should be fully determined.", result.first);
        assertEquals(
                "Page shouldn't be distillable.",
                ReaderModeManager.DistillationStatus.NOT_POSSIBLE,
                (int) result.second);
    }

    @Test
    @Feature("ReaderMode")
    @EnableFeatures(
            DomDistillerFeatures.READER_MODE_IMPROVEMENTS
                    + ":trigger_on_mobile_friendly_pages/true")
    public void testMobileFriendlyNotDistillable_exceptWhenFeatureFlagAndParamEnabled() {
        Pair<Boolean, Integer> result =
                ReaderModeManager.computeDistillationStatus(mTab, true, true, true);
        assertTrue("Distillability should be fully determined.", result.first);
        assertEquals(
                "Page should be be distillable.",
                ReaderModeManager.DistillationStatus.POSSIBLE,
                (int) result.second);
    }

    @Test
    @Feature("ReaderMode")
    public void testUi_notTriggered() {
        mDistillabilityObserver.onIsPageDistillableResult(mTab, false, true, false);
        assertEquals(
                "Distillation should not be possible.",
                DistillationStatus.NOT_POSSIBLE,
                mManager.getDistillationStatus());
        verifyNoMoreInteractions(mMessageDispatcher);
    }

    @Test
    @Feature("ReaderMode")
    public void testUi_notTriggered_navBeforeCallback() {
        // Simulate a page navigation prior to the distillability callback happening.
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);

        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);
        assertEquals(
                "Distillation should not be possible.",
                DistillationStatus.NOT_POSSIBLE,
                mManager.getDistillationStatus());
    }

    @Test
    @Feature("ReaderMode")
    public void testUi_notTriggered_muted() {
        when(mTab.isCustomTab()).thenReturn(true);
        mManager.muteSiteForTesting(MOCK_URL);
        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);
        assertEquals(
                "Distillation should be possible.",
                DistillationStatus.POSSIBLE,
                mManager.getDistillationStatus());
        verify(mMessageDispatcher, never()).enqueueMessage(any(), any(), anyInt(), anyBoolean());
    }

    @Test
    @Feature("ReaderMode")
    public void testUi_notTriggered_mutedByDomain() {
        when(mTab.isCustomTab()).thenReturn(true);
        mManager.muteSiteForTesting(JUnitTestGURLs.GOOGLE_URL_DOG);
        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);
        assertEquals(
                "Distillation should be possible.",
                DistillationStatus.POSSIBLE,
                mManager.getDistillationStatus());
        verify(
                        mMessageDispatcher,
                        never().description("Reader mode should be muted in this domain"))
                .enqueueMessage(any(), any(), anyInt(), anyBoolean());
    }

    @Test
    @Feature("ReaderMode")
    public void testUi_notTriggered_contextualPageActionUiEnabled() {
        when(mTab.isIncognito()).thenReturn(false);
        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);
        assertEquals(
                "Distillation isn't possible because it will be handled by the CPA.",
                DistillationStatus.NOT_POSSIBLE,
                mManager.getDistillationStatus());
        verify(
                        mMessageDispatcher,
                        never().description(
                                        "Message should be suppressed as the CPA UI will be shown"))
                .enqueueMessage(
                        any(), eq(mWebContents), eq(MessageScopeType.NAVIGATION), eq(false));
    }

    @Test
    @Feature("ReaderMode")
    @DisableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testUi_notTriggered_contextualPageActionUiEnabled_exceptOnCct() {
        when(mTab.isCustomTab()).thenReturn(true);
        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);
        assertEquals(
                "Distillation should be possible.",
                DistillationStatus.POSSIBLE,
                mManager.getDistillationStatus());
        verify(mMessageDispatcher)
                .enqueueMessage(
                        any(), eq(mWebContents), eq(MessageScopeType.NAVIGATION), eq(false));
    }

    @Test
    @Feature("ReaderMode")
    public void testUi_notTriggered_contextualPageActionUiEnabled_exceptOnIncognitoTabs() {
        when(mTab.isIncognito()).thenReturn(true);
        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);
        assertEquals(
                "Distillation should be possible.",
                DistillationStatus.POSSIBLE,
                mManager.getDistillationStatus());
        verify(mMessageDispatcher)
                .enqueueMessage(
                        any(), eq(mWebContents), eq(MessageScopeType.NAVIGATION), eq(false));
    }

    @Test
    @Feature("ReaderMode")
    public void testWebContentsObserver_distillerNavigationRemoved() {
        when(mNavController.getEntryAtIndex(0))
                .thenReturn(createNavigationEntry(0, MOCK_DISTILLER_URL));
        when(mNavController.getEntryAtIndex(1)).thenReturn(createNavigationEntry(1, MOCK_URL));

        // Simulate a navigation from a distilled page.
        when(mNavController.getLastCommittedEntryIndex()).thenReturn(0);
        when(mNavigationHandle.isSameDocument()).thenReturn(false);
        when(mNavigationHandle.hasCommitted()).thenReturn(true);
        when(mNavigationHandle.getUrl()).thenReturn(MOCK_URL);

        mWebContentsObserver.didStartNavigationInPrimaryMainFrame(mNavigationHandle);
        mWebContentsObserver.didFinishNavigationInPrimaryMainFrame(mNavigationHandle);

        // Distiller entry should have been removed.
        verify(mNavController).removeEntryAtIndex(0);
    }

    @Test
    @Feature("ReaderMode")
    public void testWebContentsObserver_navigateToDistilledPage() {
        when(mNavController.getEntryAtIndex(0))
                .thenReturn(createNavigationEntry(0, MOCK_DISTILLER_URL));

        // Simulate a navigation to a distilled page.
        when(mNavController.getLastCommittedEntryIndex()).thenReturn(0);
        when(mNavigationHandle.isSameDocument()).thenReturn(false);
        when(mNavigationHandle.getUrl()).thenReturn(MOCK_DISTILLER_URL);

        mWebContentsObserver.didStartNavigationInPrimaryMainFrame(mNavigationHandle);
        mWebContentsObserver.didFinishNavigationInPrimaryMainFrame(mNavigationHandle);

        assertEquals(
                "Distillation should have started.",
                DistillationStatus.STARTED,
                mManager.getDistillationStatus());
    }

    @Test
    @Feature("ReaderMode")
    public void testWebContentsObserver_sameDocumentLoad() {
        when(mNavController.getEntryAtIndex(0)).thenReturn(createNavigationEntry(0, MOCK_URL));

        // Simulate an same-document navigation.
        when(mNavController.getLastCommittedEntryIndex()).thenReturn(0);
        when(mNavigationHandle.isSameDocument()).thenReturn(true);
        when(mNavigationHandle.getUrl()).thenReturn(MOCK_URL);

        mWebContentsObserver.didStartNavigationInPrimaryMainFrame(mNavigationHandle);
        mWebContentsObserver.didFinishNavigationInPrimaryMainFrame(mNavigationHandle);

        assertEquals(
                "Distillation should not be possible.",
                DistillationStatus.NOT_POSSIBLE,
                mManager.getDistillationStatus());
    }

    @Test
    @Feature("ReaderMode")
    public void testDistillationMetricsOnDistillabilityResult() {
        when(mTab.isCustomTab()).thenReturn(true);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                ReaderModeManager.ACCESSIBILITY_SETTING_HISTOGRAM, false)
                        .expectIntRecord(
                                ReaderModeManager.PAGE_DISTILLATION_RESULT_HISTOGRAM,
                                DistillationResult.DISTILLABLE)
                        .build();
        mDistillabilityObserver.onIsPageDistillableResult(
                mTab,
                /* isDistillable= */ true,
                /* isLast= */ true,
                /* isMobileOptimized= */ false);
        watcher.assertExpected();
        verify(mUkmRecorderJniMock)
                .recordEventWithMultipleMetrics(
                        any(), eq("DomDistiller.Android.DistillabilityResult"), any());
    }

    @Test
    @Feature("ReaderMode")
    public void testDistillationMetricsOnDistillabilityResult_noMetricsRecordedForRegularTabs() {
        when(mTab.isCustomTab()).thenReturn(false);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(ReaderModeManager.ACCESSIBILITY_SETTING_HISTOGRAM)
                        .expectNoRecords(ReaderModeManager.PAGE_DISTILLATION_RESULT_HISTOGRAM)
                        .build();
        mDistillabilityObserver.onIsPageDistillableResult(
                mTab,
                /* isDistillable= */ true,
                /* isLast= */ true,
                /* isMobileOptimized= */ false);
        watcher.assertExpected();
    }

    @Test
    @Feature("ReaderMode")
    public void testDistillationMetricsOnDistillabilityResult_mobilePageExcluded() {
        when(mTab.isCustomTab()).thenReturn(true);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                ReaderModeManager.ACCESSIBILITY_SETTING_HISTOGRAM, false)
                        .expectIntRecord(
                                ReaderModeManager.PAGE_DISTILLATION_RESULT_HISTOGRAM,
                                DistillationResult.DISTILLABLE_BUT_EXCLUDED_MOBILE)
                        .build();
        mDistillabilityObserver.onIsPageDistillableResult(
                mTab, /* isDistillable= */ true, /* isLast= */ true, /* isMobileOptimized= */ true);
        watcher.assertExpected();
    }

    @Test
    @Feature("ReaderMode")
    public void testDistillationMetricsOnDistillabilityResult_mobilePageNotExcluded() {
        when(mTab.isCustomTab()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.READER_FOR_ACCESSIBILITY)).thenReturn(true);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                ReaderModeManager.ACCESSIBILITY_SETTING_HISTOGRAM, true)
                        .expectIntRecord(
                                ReaderModeManager.PAGE_DISTILLATION_RESULT_HISTOGRAM,
                                DistillationResult.DISTILLABLE)
                        .build();
        mDistillabilityObserver.onIsPageDistillableResult(
                mTab, /* isDistillable= */ true, /* isLast= */ true, /* isMobileOptimized= */ true);
        watcher.assertExpected();
    }

    @Test
    @Feature("ReaderMode")
    public void testDistillationMetricsOnDistillabilityResult_requestDesktopSiteExcluded() {
        when(mTab.isCustomTab()).thenReturn(true);

        WebContents mockWebContents = mock(WebContents.class);
        NavigationController mockNavigationController = mock(NavigationController.class);
        // Set "request desktop page" on.
        when(mockNavigationController.getUseDesktopUserAgent()).thenReturn(true);
        when(mockWebContents.getNavigationController()).thenReturn(mockNavigationController);
        when(mTab.getWebContents()).thenReturn(mockWebContents);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                ReaderModeManager.ACCESSIBILITY_SETTING_HISTOGRAM, false)
                        .expectIntRecord(
                                ReaderModeManager.PAGE_DISTILLATION_RESULT_HISTOGRAM,
                                DistillationResult.DISTILLABLE_BUT_EXCLUDED_RDS)
                        .build();
        mDistillabilityObserver.onIsPageDistillableResult(
                mTab,
                /* isDistillable= */ true,
                /* isLast= */ true,
                /* isMobileOptimized= */ false);
        watcher.assertExpected();
    }

    @Test
    @Feature("ReaderMode")
    @DisableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testTryShowingPrompt_CctCpaOff_ShouldShowPrompt() {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.isCustomTab()).thenReturn(true);

        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);

        verify(mMessageDispatcher)
                .enqueueMessage(
                        any(), eq(mWebContents), eq(MessageScopeType.NAVIGATION), eq(false));
    }

    @Test
    @Feature("ReaderMode")
    @EnableFeatures({
        ChromeFeatureList.CCT_ADAPTIVE_BUTTON,
        DomDistillerFeatures.READER_MODE_DISTILL_IN_APP // Makes test mocking easier.
    })
    public void testTryShowingPrompt_CctCpaButtonShowing_ShouldNotShowPrompt() {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.isCustomTab()).thenReturn(true);

        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);

        // Simulate the button UI being displayed.
        mButtonVisibilitySupplier.set(true);
        mManager.onContextualPageActionShown(mButtonVisibilitySupplier, /* isReaderMode= */ true);

        verify(mMessageDispatcher, never())
                .enqueueMessage(any(), any(), eq(MessageScopeType.NAVIGATION), anyBoolean());

        // Verify the histogram for fallback UI is NOT recorded when button gets shown.
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("CustomTab.AdaptiveToolbarButton.FallbackUi")
                        .build();
        mManager.activateReaderMode(EntryPoint.APP_MENU);
        watcher.assertExpected();
    }

    @Test
    @Feature("ReaderMode")
    @EnableFeatures({
        ChromeFeatureList.CCT_ADAPTIVE_BUTTON,
        DomDistillerFeatures.READER_MODE_DISTILL_IN_APP // Makes test mocking easier.
    })
    public void testTryShowingPrompt_CctCpaButtonShowingDelayed_ShouldNotShowPrompt() {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.isCustomTab()).thenReturn(true);

        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);

        // Simulate the button UI being displayed.
        mManager.onContextualPageActionShown(mButtonVisibilitySupplier, /* isReaderMode= */ true);

        // The visibility is determined in delayed fashion - after |onContextualPageActionShown|.
        mButtonVisibilitySupplier.set(true);
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mMessageDispatcher, never())
                .enqueueMessage(any(), any(), eq(MessageScopeType.NAVIGATION), anyBoolean());

        // Verify the histogram for fallback UI is NOT recorded when button gets shown.
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("CustomTab.AdaptiveToolbarButton.FallbackUi")
                        .build();
        mManager.activateReaderMode(EntryPoint.APP_MENU);
        watcher.assertExpected();
    }

    @Test
    @Feature("ReaderMode")
    @EnableFeatures({ChromeFeatureList.CCT_ADAPTIVE_BUTTON})
    @DisableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void testTryShowingPrompt_CctCpaButtonNotShowing_FallbackMessage_ShouldShowPrompt() {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.isCustomTab()).thenReturn(true);
        when(mWebContents.getLastCommittedUrl()).thenReturn(MOCK_URL);

        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);

        // Simulate the button UI not being displayed.
        mButtonVisibilitySupplier.set(false);
        mManager.onContextualPageActionShown(mButtonVisibilitySupplier, /* isReaderMode= */ true);

        verify(mMessageDispatcher)
                .enqueueMessage(
                        any(), eq(mWebContents), eq(MessageScopeType.NAVIGATION), eq(false));

        // Verify the histogram for fallback UI is recorded when activating the reader mode page.
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTab.AdaptiveToolbarButton.FallbackUi",
                        AdaptiveToolbarButtonVariant.READER_MODE);
        mManager.activateReaderMode(EntryPoint.APP_MENU);
        watcher.assertExpected();
    }

    @Test
    @Feature("ReaderMode")
    @EnableFeatures({ChromeFeatureList.CCT_ADAPTIVE_BUTTON})
    @DisableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void testTryShowingPrompt_CctCpaButtonNotShowing_FallbackMenu_ShouldNotShowPrompt() {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.isCustomTab()).thenReturn(true);
        when(mTab.isLoading()).thenReturn(false);
        when(mWebContents.getLastCommittedUrl()).thenReturn(MOCK_URL);

        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);

        FeatureOverrides.overrideParam(
                ChromeFeatureList.CCT_ADAPTIVE_BUTTON,
                ReaderModeManager.CPA_FALLBACK_MENU_PARAM,
                true);
        // Simulate the button UI not being displayed.
        mButtonVisibilitySupplier.set(false);
        mManager.onContextualPageActionShown(mButtonVisibilitySupplier, /* isReaderMode= */ true);

        verify(mMessageDispatcher, never())
                .enqueueMessage(
                        any(), eq(mWebContents), eq(MessageScopeType.NAVIGATION), eq(false));

        // Verify the histogram for fallback UI is NOT recorded.
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("CustomTab.AdaptiveToolbarButton.FallbackUi")
                        .build();
        mManager.activateReaderMode(EntryPoint.APP_MENU);
        watcher.assertExpected();
    }

    @Test
    @Feature("ReaderMode")
    @EnableFeatures({ChromeFeatureList.CCT_ADAPTIVE_BUTTON})
    @DisableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void testTryShowingPrompt_CctCpaButtonNotShowingDelayed_ShouldShowPrompt() {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.isCustomTab()).thenReturn(true);
        when(mWebContents.getLastCommittedUrl()).thenReturn(MOCK_URL);

        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);

        // Simulate the button UI not being displayed.
        mManager.onContextualPageActionShown(mButtonVisibilitySupplier, /* isReaderMode= */ true);

        // The visibility is determined in delayed fashion - after |onContextualPageActionShown|.
        mButtonVisibilitySupplier.set(false);
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mMessageDispatcher)
                .enqueueMessage(
                        any(), eq(mWebContents), eq(MessageScopeType.NAVIGATION), eq(false));

        // Verify the histogram for fallback UI is recorded when activating the reader mode page.
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTab.AdaptiveToolbarButton.FallbackUi",
                        AdaptiveToolbarButtonVariant.READER_MODE);
        mManager.activateReaderMode(EntryPoint.APP_MENU);
        watcher.assertExpected();
    }

    @Test
    @Feature("ReaderMode")
    @EnableFeatures(ChromeFeatureList.CCT_ADAPTIVE_BUTTON)
    public void testTryShowingPrompt_CctCpaOn_Incognito_ShouldShowPromptIfApplicable() {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.isIncognito()).thenReturn(true);
        when(mTab.isCustomTab()).thenReturn(false);

        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);

        verify(mMessageDispatcher)
                .enqueueMessage(
                        any(), eq(mWebContents), eq(MessageScopeType.NAVIGATION), eq(false));
    }

    @Test
    @Feature("ReaderMode")
    public void testHideReadingMode() {
        UserActionTester userActionTester = new UserActionTester();

        mManager.hideReaderMode();
        verify(mTab).goBack();
        assertEquals(1, userActionTester.getActionCount("MobileReaderModeHidden"));
    }

    @Test
    @Feature("ReaderMode")
    @EnableFeatures({DomDistillerFeatures.READER_MODE_DISTILL_IN_APP})
    public void testDistillationSuccess_noSnackbar() {
        when(mTab.getWebContents()).thenReturn(mWebContents);

        mManager.navigateToReaderMode();
        verify(mDistillerTabUtilsJniMock)
                .distillCurrentPageAndViewIfSuccessful(
                        any(), mDistillationCallbackCaptor.capture());
        mDistillationCallbackCaptor.getValue().onResult(true);
        verify(mSnackbarManager, times(0)).showSnackbar(any());
    }

    @Test
    @Feature("ReaderMode")
    @EnableFeatures({DomDistillerFeatures.READER_MODE_DISTILL_IN_APP})
    public void testDistillationFailure_showSnackbar() {
        when(mTab.getWebContents()).thenReturn(mWebContents);

        mManager.navigateToReaderMode();
        verify(mDistillerTabUtilsJniMock)
                .distillCurrentPageAndViewIfSuccessful(
                        any(), mDistillationCallbackCaptor.capture());
        mDistillationCallbackCaptor.getValue().onResult(false);
        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    @Feature("ReaderMode")
    public void testStartedReaderMode_Cct_ShouldNotTriggerStoppedMetric() {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getLastCommittedUrl()).thenReturn(MOCK_URL);
        when(mTab.isCustomTab()).thenReturn(true);

        UserActionTester userActionTester = new UserActionTester();

        mManager.activateReaderMode(EntryPoint.APP_MENU);

        assertEquals(
                1, userActionTester.getActionCount("DomDistiller.Android.OnStartedReaderMode"));
        assertEquals(
                0, userActionTester.getActionCount("DomDistiller.Android.OnStoppedReaderMode"));
    }

    @Test
    @Feature("ReaderMode")
    public void testStoppedReaderMode_onHidden_ShouldTriggerStoppedMetric() {
        UserActionTester userActionTester = new UserActionTester();
        when(mTab.isCustomTab()).thenReturn(true);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("DomDistiller.Time.ViewingReaderModePage")
                        .build();

        mManager.navigateToReaderMode();
        mTabObserver.onHidden(mTab, 1);

        assertEquals(
                1, userActionTester.getActionCount("DomDistiller.Android.OnStoppedReaderMode"));
        watcher.assertExpected();
    }

    @Test
    @Feature("ReaderMode")
    public void testStartedReaderMode_onDestroyed_ShouldTriggerStoppedMetric() {
        UserActionTester userActionTester = new UserActionTester();
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("DomDistiller.Time.ViewingReaderModePage")
                        .build();

        mManager.navigateToReaderMode();
        mTabObserver.onDestroyed(mTab);

        assertEquals(
                1, userActionTester.getActionCount("DomDistiller.Android.OnStoppedReaderMode"));
        watcher.assertExpected();
    }

    @Test
    @Feature("ReaderMode")
    public void testStartedReaderMode_navigationEntryCommitted_ShouldTriggerStoppedMetric() {
        UserActionTester userActionTester = new UserActionTester();
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("DomDistiller.Time.ViewingReaderModePage")
                        .build();

        mManager.navigateToReaderMode();
        mWebContentsObserver.navigationEntryCommitted(mLoadCommitedDetails);

        assertEquals(
                1, userActionTester.getActionCount("DomDistiller.Android.OnStoppedReaderMode"));
        watcher.assertExpected();
    }

    private NavigationEntry createNavigationEntry(int index, GURL url) {
        return new NavigationEntry(
                index, url, url, url, "", null, 0, 0, /* isInitialEntry= */ false);
    }
}
