// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.junit.Assert.assertNotNull;
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
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_suggestion.toolbar.GroupSuggestionsButtonController;
import org.chromium.chrome.browser.tab_group_suggestion.toolbar.GroupSuggestionsButtonControllerFactory;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonController;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.components.commerce.core.ShoppingService;

import java.util.function.Supplier;

/** Unit tests for {@link ContextualPageActionController} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.CONTEXTUAL_PAGE_ACTIONS})
public class ContextualPageActionControllerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private ObservableSupplierImpl<Profile> mProfileSupplier;
    private ObservableSupplierImpl<Tab> mTabSupplier;
    private UserDataHost mTabUserDataHost;

    @Mock private Profile mMockProfile;
    @Mock private Tab mMockTab;
    @Mock private AdaptiveToolbarButtonController mMockAdaptiveToolbarController;
    @Mock private ContextualPageActionController.Natives mMockControllerJni;

    @Before
    public void setUp() {

        mProfileSupplier = new ObservableSupplierImpl<>();
        mTabSupplier = new ObservableSupplierImpl<>();
        mTabUserDataHost = new UserDataHost();

        ContextualPageActionControllerJni.setInstanceForTesting(mMockControllerJni);
        when(mMockTab.getUserDataHost()).thenReturn(mTabUserDataHost);
    }

    private ContextualPageActionController createContextualPageActionController() {
        ContextualPageActionController contextualPageActionController =
                new ContextualPageActionController(
                        mProfileSupplier,
                        mTabSupplier,
                        mMockAdaptiveToolbarController,
                        null,
                        null) {
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
