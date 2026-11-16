// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab_group_suggestion.toolbar.GroupSuggestionsButtonController;
import org.chromium.chrome.browser.tab_group_suggestion.toolbar.GroupSuggestionsButtonControllerFactory;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.web_distiller.core.WebDistillerUrlUtils;
import org.chromium.components.web_distiller.core.WebDistillerUrlUtilsJni;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.function.Supplier;

/** Unit tests for {@link ContextualPageActionController} */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.CONTEXTUAL_PAGE_ACTIONS})
public class ContextualPageActionControllerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private SettableMonotonicObservableSupplier<Profile> mProfileSupplier;
    private SettableNullableObservableSupplier<Tab> mTabSupplier;
    private UserDataHost mTabUserDataHost;

    @Mock private Profile mMockProfile;
    @Mock private Tab mMockTab;
    @Mock private AdaptiveToolbarButtonController mMockAdaptiveToolbarController;
    @Mock private ContextualPageActionController.Natives mMockControllerJni;
    @Mock private WebDistillerUrlUtils.Natives mMockWebDistillerUrlUtilsJni;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor private ArgumentCaptor<Callback<Integer>> mResultCallbackCaptor;

    @Before
    public void setUp() {

        mProfileSupplier = ObservableSuppliers.createMonotonic();
        mTabSupplier = ObservableSuppliers.createNullable();
        mTabUserDataHost = new UserDataHost();

        ContextualPageActionControllerJni.setInstanceForTesting(mMockControllerJni);
        WebDistillerUrlUtilsJni.setInstanceForTesting(mMockWebDistillerUrlUtilsJni);
        when(mMockTab.getUserDataHost()).thenReturn(mTabUserDataHost);
    }

    private ContextualPageActionController createContextualPageActionController() {
        ContextualPageActionController contextualPageActionController =
                new ContextualPageActionController(
                        mProfileSupplier,
                        mTabSupplier,
                        mMockAdaptiveToolbarController,
                        /* shoppingServiceSupplier= */ null,
                        /* bookmarkModelSupplier= */ null) {
                    @Override
                    protected void initActionProviders(
                            Supplier<ShoppingService> shoppingServiceSupplier,
                            Supplier<BookmarkModel> bookmarkModelSupplier) {
                        mActionProviders.put(
                                AdaptiveToolbarButtonVariant.READER_MODE,
                                (ActionProvider)
                                        (tab, signalAccumulator) -> {
                                            // Supply all signals and notify controller.
                                            signalAccumulator.setSignal(
                                                    AdaptiveToolbarButtonVariant.READER_MODE, true);
                                            signalAccumulator.setSignal(
                                                    AdaptiveToolbarButtonVariant.PRICE_TRACKING,
                                                    true);
                                            signalAccumulator.setSignal(
                                                    AdaptiveToolbarButtonVariant.PRICE_INSIGHTS,
                                                    true);
                                            signalAccumulator.setSignal(
                                                    AdaptiveToolbarButtonVariant.DISCOUNTS, true);
                                        });
                    }
                };

        mProfileSupplier.set(mMockProfile);

        return contextualPageActionController;
    }

    private void setMockSegmentationResult(@AdaptiveToolbarButtonVariant int buttonVariant) {
        Mockito.doAnswer(
                        invocation -> {
                            Callback<Integer> callback = invocation.getArgument(2);
                            callback.onResult(buttonVariant);
                            return null;
                        })
                .when(mMockControllerJni)
                .computeContextualPageAction(any(), any(), any());
    }

    /** Creates a controller whose only provider never responds, so a request ends by timing out. */
    private void createControllerWithPendingProvider() {
        ContextualPageActionController controller = createContextualPageActionController();
        controller.mActionProviders.clear();
        controller.mActionProviders.put(
                AdaptiveToolbarButtonVariant.PRICE_TRACKING, (tab, signalAccumulator) -> {});
    }

    private void finishNavigation(GURL url, boolean isSameDocument, boolean hasCommitted) {
        NavigationHandle navigation =
                NavigationHandle.createForTesting(
                        url,
                        /* isInPrimaryMainFrame= */ true,
                        isSameDocument,
                        /* isRendererInitiated= */ false,
                        PageTransition.LINK,
                        /* hasUserGesture= */ false,
                        /* isReload= */ false);
        navigation.callDidFinishForTesting(url, hasCommitted);
        verify(mMockTab).addObserver(mTabObserverCaptor.capture());
        mTabObserverCaptor.getValue().onDidFinishNavigationInPrimaryMainFrame(mMockTab, navigation);
    }

    private void commitNavigation(GURL url) {
        finishNavigation(url, /* isSameDocument= */ false, /* hasCommitted= */ true);
    }

    @Test
    public void nonHttpNavigationRevertsToDefaultAction() {
        setMockSegmentationResult(AdaptiveToolbarButtonVariant.PRICE_INSIGHTS);
        ContextualPageActionController controller = createContextualPageActionController();
        ContextualPageActionController.ActionProvider actionProvider =
                Mockito.spy(
                        new ContextualPageActionController.ActionProvider() {
                            @Override
                            public void getAction(Tab tab, SignalAccumulator signalAccumulator) {
                                signalAccumulator.setSignal(
                                        AdaptiveToolbarButtonVariant.PRICE_INSIGHTS,
                                        /* signalValue= */ true);
                            }
                        });
        controller.mActionProviders.clear();
        controller.mActionProviders.put(
                AdaptiveToolbarButtonVariant.PRICE_INSIGHTS, actionProvider);
        mTabSupplier.set(mMockTab);
        assertTrue(controller.hasPriceInsights());
        Mockito.clearInvocations(mMockAdaptiveToolbarController, actionProvider);

        commitNavigation(JUnitTestGURLs.EXAMPLE_URL);
        verify(mMockAdaptiveToolbarController, never()).revertToStaticAction();
        assertTrue(controller.hasPriceInsights());

        commitNavigation(JUnitTestGURLs.NTP_NATIVE_URL);
        verify(mMockAdaptiveToolbarController).revertToStaticAction();
        verify(actionProvider).onActionShown(mMockTab, AdaptiveToolbarButtonVariant.UNKNOWN);
        assertFalse(controller.hasPriceInsights());
        // showDynamicAction() records a page load sample, which a revert must not do.
        verify(mMockAdaptiveToolbarController, never()).showDynamicAction(anyInt());

        // A subsequent paint on a new HTTP(S) page still computes and shows a dynamic action.
        mTabObserverCaptor.getValue().didFirstVisuallyNonEmptyPaint(mMockTab);
        verify(mMockAdaptiveToolbarController)
                .showDynamicAction(AdaptiveToolbarButtonVariant.PRICE_INSIGHTS);
    }

    @Test
    public void nonHttpNavigationWithoutNewDocumentKeepsAction() {
        when(mMockTab.isLoading()).thenReturn(true);
        createContextualPageActionController();
        mTabSupplier.set(mMockTab);

        finishNavigation(
                JUnitTestGURLs.NTP_NATIVE_URL,
                /* isSameDocument= */ false,
                /* hasCommitted= */ false);
        verify(mMockAdaptiveToolbarController, never()).revertToStaticAction();

        finishNavigation(
                JUnitTestGURLs.NTP_NATIVE_URL,
                /* isSameDocument= */ true,
                /* hasCommitted= */ true);
        verify(mMockAdaptiveToolbarController, never()).revertToStaticAction();
    }

    @Test
    public void nonHttpNavigationDropsRequestWaitingForSignals() {
        createControllerWithPendingProvider();
        mTabSupplier.set(mMockTab);

        commitNavigation(JUnitTestGURLs.NTP_NATIVE_URL);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mMockControllerJni, never()).computeContextualPageAction(any(), any(), any());
    }

    @Test
    public void nonHttpNavigationDropsRequestWaitingForResult() {
        createControllerWithPendingProvider();
        mTabSupplier.set(mMockTab);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockControllerJni)
                .computeContextualPageAction(any(), any(), mResultCallbackCaptor.capture());

        commitNavigation(JUnitTestGURLs.NTP_NATIVE_URL);
        mResultCallbackCaptor.getValue().onResult(AdaptiveToolbarButtonVariant.PRICE_TRACKING);

        verify(mMockAdaptiveToolbarController, never()).showDynamicAction(anyInt());
    }

    @Test
    public void distilledPageNavigationKeepsAction() {
        when(mMockWebDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(true);
        when(mMockTab.isLoading()).thenReturn(true);
        createContextualPageActionController();
        mTabSupplier.set(mMockTab);

        commitNavigation(JUnitTestGURLs.CHROME_DISTILLER_EXAMPLE_URL);

        verify(mMockAdaptiveToolbarController, never()).revertToStaticAction();
    }

    @Test
    public void loadingTabsAreIgnored() {
        setMockSegmentationResult(AdaptiveToolbarButtonVariant.PRICE_TRACKING);

        when(mMockTab.isLoading()).thenReturn(true);

        createContextualPageActionController();

        mTabSupplier.set(mMockTab);

        verify(mMockAdaptiveToolbarController, never()).showDynamicAction(anyInt());
    }

    @Test
    public void incognitoTabsRevertToDefaultAction() {
        setMockSegmentationResult(AdaptiveToolbarButtonVariant.PRICE_TRACKING);

        when(mMockTab.isIncognito()).thenReturn(true);

        createContextualPageActionController();

        mTabSupplier.set(mMockTab);

        verify(mMockAdaptiveToolbarController)
                .showDynamicAction(AdaptiveToolbarButtonVariant.UNKNOWN);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_TAB_GROUPING})
    public void tabGroupingControllerIsCreatedWithFlag() {
        var groupSuggestionButtonController = mock(GroupSuggestionsButtonController.class);
        GroupSuggestionsButtonControllerFactory.setControllerForTesting(
                groupSuggestionButtonController);

        var cpaController =
                new ContextualPageActionController(
                        mProfileSupplier,
                        mTabSupplier,
                        mMockAdaptiveToolbarController,
                        /* shoppingServiceSupplier= */ null,
                        /* bookmarkModelSupplier= */ null);

        mProfileSupplier.set(mMockProfile);

        assertNotNull(
                cpaController.mActionProviders.get(AdaptiveToolbarButtonVariant.TAB_GROUPING));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_TAB_GROUPING})
    public void testDestroy() {
        var groupSuggestionButtonController = mock(GroupSuggestionsButtonController.class);
        GroupSuggestionsButtonControllerFactory.setControllerForTesting(
                groupSuggestionButtonController);

        var cpaController =
                new ContextualPageActionController(
                        mProfileSupplier,
                        mTabSupplier,
                        mMockAdaptiveToolbarController,
                        /* shoppingServiceSupplier= */ null,
                        /* bookmarkModelSupplier= */ null);

        mProfileSupplier.set(mMockProfile);
        cpaController.destroy();
        verify(groupSuggestionButtonController).destroy();
    }
}
