// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.collection.IsIterableContainingInOrder.contains;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.test.util.browser.offlinepages.FakeOfflinePageBridge.createOfflinePageItem;
import static org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.createDummySuggestions;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.metrics.test.DisableHistogramsRule;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder.PartialBindCallback;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.suggestions.ContentSuggestionsAdditionalAction;
import org.chromium.chrome.browser.suggestions.SuggestionsEventReporter;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsRanker;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.offlinepages.FakeOfflinePageBridge;
import org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.CategoryInfoBuilder;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;

/**
 * Unit tests for {@link SuggestionsSection}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {CustomShadowAsyncTask.class})
public class SuggestionsSectionTest {
    @Rule
    public DisableHistogramsRule mDisableHistogramsRule = new DisableHistogramsRule();

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    private static final int TEST_CATEGORY_ID = 42;

    private static final int REMOTE_TEST_CATEGORY =
            KnownCategories.REMOTE_CATEGORIES_OFFSET + TEST_CATEGORY_ID;

    private static final int EXPANDABLE_HEADER_PREF = Pref.NTP_ARTICLES_LIST_VISIBLE;

    @Mock
    private SuggestionsSection.Delegate mDelegate;
    @Mock
    private ListObserver<PartialBindCallback> mObserver;
    @Mock
    private SuggestionsUiDelegate mUiDelegate;
    @Mock
    private PrefServiceBridge mPrefServiceBridge;
    @Mock
    private SigninManager mSigninManager;
    @Mock
    private IdentityManager mIdentityManager;

    private FakeSuggestionsSource mSuggestionsSource;
    private FakeOfflinePageBridge mBridge;

    @Before
    public void setUp() {
        RecordUserAction.setDisabledForTests(true);
        MockitoAnnotations.initMocks(this);

        mBridge = new FakeOfflinePageBridge();

        mSuggestionsSource = spy(new FakeSuggestionsSource());
        mSuggestionsSource.setStatusForCategory(TEST_CATEGORY_ID, CategoryStatus.AVAILABLE);
        when(mUiDelegate.getSuggestionsSource()).thenReturn(mSuggestionsSource);
        when(mUiDelegate.getNavigationDelegate())
                .thenReturn(mock(SuggestionsNavigationDelegate.class));
        when(mUiDelegate.getEventReporter()).thenReturn(mock(SuggestionsEventReporter.class));

        doNothing().when(mPrefServiceBridge).setBoolean(anyInt(), anyBoolean());
        PrefServiceBridge.setInstanceForTesting(mPrefServiceBridge);

        // Set empty variation params for the test.
        CardsVariationParameters.setTestVariationParams(new HashMap<>());

        // Set up a test account and initialize to the signed in state.
        NewTabPageTestUtils.setUpTestAccount();
        when(mSigninManager.getIdentityManager()).thenReturn(mIdentityManager);
        when(mIdentityManager.hasPrimaryAccount()).thenReturn(false);
        when(mSigninManager.isSignInAllowed()).thenReturn(true);
    }

    @After
    public void tearDown() {
        RecordUserAction.setDisabledForTests(false);
        PrefServiceBridge.setInstanceForTesting(null);
    }

    @Test
    @Feature({"Ntp"})
    public void testDismissSibling() {
        List<SnippetArticle> snippets = createDummySuggestions(3, TEST_CATEGORY_ID);
        SuggestionsSection section = createSectionWithFetchAction(true);

        section.setStatus(CategoryStatus.AVAILABLE);
        assertNotNull(section.getActionItemForTesting());

        // Without snippets.
        assertEquals(ItemViewType.HEADER, section.getItemViewType(0));
        assertEquals(Collections.emptySet(), section.getItemDismissalGroup(0));
        assertEquals(ItemViewType.STATUS, section.getItemViewType(1));
        assertEquals(Collections.emptySet(), section.getItemDismissalGroup(1));
        assertEquals(ItemViewType.ACTION, section.getItemViewType(2));
        assertEquals(Collections.emptySet(), section.getItemDismissalGroup(2));

        // With snippets.
        section.appendSuggestions(snippets, /* keepSectionSize = */ true,
                /* reportPrefetchedSuggestionsCount = */ false);
        assertEquals(ItemViewType.HEADER, section.getItemViewType(0));
        assertEquals(Collections.emptySet(), section.getItemDismissalGroup(0));
        assertEquals(ItemViewType.SNIPPET, section.getItemViewType(1));
        assertEquals(Collections.singleton(1), section.getItemDismissalGroup(1));
    }

    @Test
    @Feature({"Ntp"})
    public void testGetDismissalGroupWithoutHeader() {
        SuggestionsSection section = createSectionWithFetchAction(true);
        section.setHeaderVisibility(false);

        assertEquals(ItemViewType.STATUS, section.getItemViewType(0));
        assertEquals(Collections.emptySet(), section.getItemDismissalGroup(0));

        assertEquals(ItemViewType.ACTION, section.getItemViewType(1));
        assertEquals(Collections.emptySet(), section.getItemDismissalGroup(1));
    }

    @Test
    @Feature({"Ntp"})
    public void testGetDismissalGroupWithoutAction() {
        SuggestionsSection section = createSectionWithFetchAction(false);

        assertEquals(ItemViewType.STATUS, section.getItemViewType(1));
        assertEquals(Collections.emptySet(), section.getItemDismissalGroup(1));
    }

    @Test
    @Feature({"Ntp"})
    public void testGetDismissalGroupActionAndHeader() {
        SuggestionsSection section = createSectionWithFetchAction(false);
        section.setHeaderVisibility(false);

        assertEquals(ItemViewType.STATUS, section.getItemViewType(0));
        assertEquals(Collections.emptySet(), section.getItemDismissalGroup(0));
    }

    @Test
    @Feature({"Ntp"})
    public void testAddSuggestionsNotification() {
        final int suggestionCount = 5;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount,
                TEST_CATEGORY_ID);

        SuggestionsSection section = createSectionWithFetchAction(false);
        // Simulate initialisation by the adapter. Here we don't care about the notifications, since
        // the RecyclerView will be updated through notifyDataSetChanged.
        section.setStatus(CategoryStatus.AVAILABLE);
        Mockito.<ListObserver>reset(mObserver);

        assertEquals(2, section.getItemCount()); // When empty, we have the header and status card.
        assertEquals(ItemViewType.STATUS, section.getItemViewType(1));

        section.appendSuggestions(snippets, /* keepSectionSize = */ true,
                /* reportPrefetchedSuggestionsCount = */ false);
        verify(mObserver).onItemRangeInserted(section, 1, suggestionCount);
        verify(mObserver).onItemRangeRemoved(section, 1 + suggestionCount, 1);
    }

    @Test
    @Feature({"Ntp"})
    public void testSetStatusNotification() {
        final int suggestionCount = 5;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount,
                TEST_CATEGORY_ID);
        SuggestionsSection section = createSectionWithFetchAction(false);

        // Simulate initialisation by the adapter. Here we don't care about the notifications, since
        // the RecyclerView will be updated through notifyDataSetChanged.
        section.setStatus(CategoryStatus.AVAILABLE);
        section.appendSuggestions(snippets, /* keepSectionSize = */ true,
                /* reportPrefetchedSuggestionsCount = */ false);
        Mockito.<ListObserver>reset(mObserver);

        // We don't clear suggestions when the status is AVAILABLE.
        section.setStatus(CategoryStatus.AVAILABLE);
        verifyNoMoreInteractions(mObserver);

        // We clear existing suggestions when the status is not AVAILABLE, and show the status card.
        section.setStatus(CategoryStatus.CATEGORY_EXPLICITLY_DISABLED);
        verify(mObserver).onItemRangeRemoved(section, 1, suggestionCount);
        verify(mObserver).onItemRangeInserted(section, 1, 1);

        // A loading state item triggers showing the loading item.
        section.setStatus(CategoryStatus.AVAILABLE_LOADING);
        verify(mObserver).onItemRangeInserted(section, 2, 1);

        section.setStatus(CategoryStatus.AVAILABLE);
        verify(mObserver).onItemRangeRemoved(section, 2, 1);
    }

    @Test
    @Feature({"Ntp"})
    public void testRemoveUnknownSuggestion() {
        SuggestionsSection section = createSectionWithFetchAction(false);
        section.setStatus(CategoryStatus.AVAILABLE);
        section.removeSuggestionById("foobar");
    }

    @Test
    @Feature({"Ntp"})
    public void testRemoveSuggestionNotification() {
        final int suggestionCount = 2;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount,
                TEST_CATEGORY_ID);

        SuggestionsSection section = createSectionWithFetchAction(false);
        section.setStatus(CategoryStatus.AVAILABLE);
        Mockito.<ListObserver>reset(mObserver);

        section.appendSuggestions(snippets, /* keepSectionSize = */ true,
                /* reportPrefetchedSuggestionsCount = */ false);

        section.removeSuggestionById(snippets.get(1).mIdWithinCategory);
        verify(mObserver).onItemRangeRemoved(section, 2, 1);

        section.removeSuggestionById(snippets.get(0).mIdWithinCategory);
        verify(mObserver).onItemRangeRemoved(section, 1, 1);
        verify(mObserver).onItemRangeInserted(section, 1, 1);

        assertEquals(2, section.getItemCount());
        assertEquals(ItemViewType.STATUS, section.getItemViewType(1));
    }

    @Test
    @Feature({"Ntp"})
    public void testRemoveSuggestionNotificationWithButton() {
        final int suggestionCount = 2;
        List<SnippetArticle> snippets = createDummySuggestions(suggestionCount,
                TEST_CATEGORY_ID);

        SuggestionsCategoryInfo info = new CategoryInfoBuilder(TEST_CATEGORY_ID)
                                               .withAction(ContentSuggestionsAdditionalAction.FETCH)
                                               .showIfEmpty()
                                               .build();
        SuggestionsSection section = createSection(info);
        section.setStatus(CategoryStatus.AVAILABLE);
        Mockito.<ListObserver>reset(mObserver);
        assertEquals(3, section.getItemCount()); // We have the header and status card and a button.

        section.appendSuggestions(snippets, /* keepSectionSize = */ true,
                /* reportPrefetchedSuggestionsCount = */ false);
        assertEquals(4, section.getItemCount());

        section.removeSuggestionById(snippets.get(0).mIdWithinCategory);
        verify(mObserver).onItemRangeRemoved(section, 1, 1);

        section.removeSuggestionById(snippets.get(1).mIdWithinCategory);
        verify(mObserver, times(2)).onItemRangeRemoved(section, 1, 1);
        verify(mObserver).onItemRangeInserted(section, 1, 1); // Only the status card is added.
        assertEquals(3, section.getItemCount());
        assertEquals(ItemViewType.STATUS, section.getItemViewType(1));
        assertEquals(ItemViewType.ACTION, section.getItemViewType(2));
    }

    @Test
    @Feature({"Ntp"})
    public void testExpandableHeaderNoSuggestions() {
        // Set to the collapsed state initially.
        when(mPrefServiceBridge.getBoolean(eq(EXPANDABLE_HEADER_PREF))).thenReturn(false);
        SuggestionsSection section =
                createSection(new CategoryInfoBuilder(KnownCategories.ARTICLES)
                                      .withAction(ContentSuggestionsAdditionalAction.FETCH)
                                      .showIfEmpty()
                                      .build());
        mSuggestionsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        section.setStatus(CategoryStatus.AVAILABLE);

        Mockito.<ListObserver>reset(mObserver);
        assertEquals(1, section.getItemCount());
        assertEquals(ItemViewType.HEADER, section.getItemViewType(0));

        // Simulate toggling the header to the expanded state.
        section.getHeaderItemForTesting().toggleHeader();
        assertEquals(4, section.getItemCount());
        assertEquals(ItemViewType.HEADER, section.getItemViewType(0));
        assertEquals(ItemViewType.PROMO, section.getItemViewType(1));
        assertEquals(ItemViewType.STATUS, section.getItemViewType(2));
        assertEquals(ItemViewType.ACTION, section.getItemViewType(3));

        // Simulate toggling the header to the collapsed state.
        section.getHeaderItemForTesting().toggleHeader();
        assertEquals(1, section.getItemCount());
        assertEquals(ItemViewType.HEADER, section.getItemViewType(0));
    }

    @Test
    @Feature({"Ntp"})
    public void testExpandableHeaderWithSuggestions() {
        // Set to the expanded state initially.
        when(mPrefServiceBridge.getBoolean(eq(EXPANDABLE_HEADER_PREF))).thenReturn(true);
        SuggestionsSection section =
                createSection(new CategoryInfoBuilder(KnownCategories.ARTICLES)
                                      .withAction(ContentSuggestionsAdditionalAction.FETCH)
                                      .showIfEmpty()
                                      .build());
        mSuggestionsSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        section.setStatus(CategoryStatus.AVAILABLE);

        List<SnippetArticle> suggestions =
                mSuggestionsSource.createAndSetSuggestions(3, KnownCategories.ARTICLES);
        section.appendSuggestions(suggestions,
                /* keepSectionSize = */ true, /* reportPrefetchedSuggestionsCount = */ false);

        Mockito.<ListObserver>reset(mObserver);
        assertEquals(6, section.getItemCount());
        assertEquals(ItemViewType.HEADER, section.getItemViewType(0));
        assertEquals(ItemViewType.PROMO, section.getItemViewType(1));
        assertEquals(ItemViewType.SNIPPET, section.getItemViewType(2));
        assertEquals(ItemViewType.SNIPPET, section.getItemViewType(3));
        assertEquals(ItemViewType.SNIPPET, section.getItemViewType(4));
        assertEquals(ItemViewType.ACTION, section.getItemViewType(5));

        // Simulate toggling the header to the collapsed state.
        section.getHeaderItemForTesting().toggleHeader();
        assertEquals(1, section.getItemCount());
        assertEquals(ItemViewType.HEADER, section.getItemViewType(0));

        // Simulate toggling the header to the expanded state.
        section.getHeaderItemForTesting().toggleHeader();
        assertEquals(6, section.getItemCount());
        assertEquals(ItemViewType.HEADER, section.getItemViewType(0));
        assertEquals(ItemViewType.PROMO, section.getItemViewType(1));
        assertEquals(ItemViewType.SNIPPET, section.getItemViewType(2));
        assertEquals(ItemViewType.SNIPPET, section.getItemViewType(3));
        assertEquals(ItemViewType.SNIPPET, section.getItemViewType(4));
        assertEquals(ItemViewType.ACTION, section.getItemViewType(5));
    }

    @Test
    @Feature({"Ntp"})
    public void testOfflineStatus() {
        final int suggestionCount = 3;
        final List<SnippetArticle> snippets = createDummySuggestions(suggestionCount,
                TEST_CATEGORY_ID);
        assertNull(snippets.get(0).getOfflinePageOfflineId());
        assertNull(snippets.get(1).getOfflinePageOfflineId());
        assertNull(snippets.get(2).getOfflinePageOfflineId());

        final OfflinePageItem item0 = createOfflinePageItem(snippets.get(0).mUrl, 0L);
        final OfflinePageItem item1 = createOfflinePageItem(snippets.get(1).mUrl, 1L);

        mBridge.setIsOfflinePageModelLoaded(true);
        mBridge.setItems(Arrays.asList(item0, item1));

        SuggestionsSection section = createSectionWithFetchAction(true);
        section.setStatus(CategoryStatus.AVAILABLE);
        section.appendSuggestions(snippets, /* keepSectionSize = */ true,
                /* reportPrefetchedSuggestionsCount = */ false);

        // Check that we pick up the correct information.
        assertEquals(Long.valueOf(0L), snippets.get(0).getOfflinePageOfflineId());
        assertEquals(Long.valueOf(1L), snippets.get(1).getOfflinePageOfflineId());
        assertNull(snippets.get(2).getOfflinePageOfflineId());

        final OfflinePageItem item2 = createOfflinePageItem(snippets.get(2).mUrl, 2L);

        mBridge.setItems(Arrays.asList(item1, item2));

        // Check that a change in OfflinePageBridge state forces an update.
        mBridge.fireOfflinePageModelLoaded();
        assertNull(snippets.get(0).getOfflinePageOfflineId());
        assertEquals(Long.valueOf(1L), snippets.get(1).getOfflinePageOfflineId());
        assertEquals(Long.valueOf(2L), snippets.get(2).getOfflinePageOfflineId());

        // TODO(bauerb): Once this code switches away from partial bind callbacks in favor of
        // property keys, verify that this is an offline badge update.
        verify(mObserver).onItemRangeChanged(
                eq(section), eq(0), eq(1), any(PartialBindCallback.class));
        verify(mObserver).onItemRangeChanged(
                eq(section), eq(2), eq(1), any(PartialBindCallback.class));
    }

    @Test
    @Feature({"Ntp"})
    public void testOfflineStatusIgnoredIfDestroyed() {
        final int suggestionCount = 2;
        final List<SnippetArticle> suggestions =
                mSuggestionsSource.createAndSetSuggestions(suggestionCount, TEST_CATEGORY_ID);
        assertNull(suggestions.get(0).getOfflinePageOfflineId());
        assertNull(suggestions.get(1).getOfflinePageOfflineId());

        final OfflinePageItem item0 = createOfflinePageItem(suggestions.get(0).mUrl, 0L);
        mBridge.setIsOfflinePageModelLoaded(true);
        mBridge.setItems(Collections.singletonList(item0));

        SuggestionsSection section = createSectionWithSuggestions(suggestions);

        // The offline status should propagate before detaching.
        assertEquals(Long.valueOf(0L), suggestions.get(0).getOfflinePageOfflineId());
        assertNull(suggestions.get(1).getOfflinePageOfflineId());

        section.destroy();

        final OfflinePageItem item1 = createOfflinePageItem(suggestions.get(1).mUrl, 1L);
        mBridge.setItems(Arrays.asList(item0, item1));
        // Check that a change in OfflinePageBridge state forces an update.
        mBridge.fireOfflinePageModelLoaded();

        // The offline status should not change any more.
        assertEquals(Long.valueOf(0L), suggestions.get(0).getOfflinePageOfflineId());
        assertNull(suggestions.get(1).getOfflinePageOfflineId());
    }

    @Test
    @Feature({"Ntp"})
    public void testOfflineStatusIgnoredIfSuggestionRemoved() {
        final List<SnippetArticle> suggestions =
                mSuggestionsSource.createAndSetSuggestions(2, TEST_CATEGORY_ID);
        assertNull(suggestions.get(0).getOfflinePageOfflineId());
        assertNull(suggestions.get(1).getOfflinePageOfflineId());

        final OfflinePageItem item0 = createOfflinePageItem(suggestions.get(0).mUrl, 0L);
        mBridge.setIsOfflinePageModelLoaded(true);
        mBridge.setItems(Collections.singletonList(item0));

        SuggestionsSection section = createSectionWithSuggestions(suggestions);

        // The offline status should propagate.
        assertEquals(Long.valueOf(0L), suggestions.get(0).getOfflinePageOfflineId());
        assertNull(suggestions.get(1).getOfflinePageOfflineId());

        // Let the fake bridge answer requests asynchronously.
        mBridge.setAnswersRequestsImmediately(false);
        final OfflinePageItem item1 = createOfflinePageItem(suggestions.get(1).mUrl, 1L);
        mBridge.setItems(Arrays.asList(item0, item1));
        mBridge.fireOfflinePageModelLoaded();

        // Remove a suggestion while an OfflinePageItem callback is in progress.
        section.removeSuggestionById(suggestions.get(1).mIdWithinCategory);
        assertEquals(section.getSuggestionsCount(), 1);

        // Reset any previous change notifications.
        // TODO(bauerb): Once this code switches away from partial bind callbacks in favor of
        // property keys, we can distinguish the offline badge update from others.
        Mockito.<ListObserver>reset(mObserver);
        mBridge.answerAllRequests();

        // The offline status should not change.
        assertNull(suggestions.get(1).getOfflinePageOfflineId());
        verify(mObserver, never()).onItemRangeChanged(any(), anyInt(), anyInt(), any());
    }

    @Test
    @Feature({"Ntp"})
    public void testFetchAction() {
        // When only FetchMore is shown when enabled.

        // Spy so that VerifyAction can check methods being called.
        SuggestionsCategoryInfo info =
                spy(new CategoryInfoBuilder(TEST_CATEGORY_ID)
                                .withAction(ContentSuggestionsAdditionalAction.FETCH)
                                .showIfEmpty()
                                .build());
        SuggestionsSection section = createSection(info);

        assertTrue(section.getActionItemForTesting().isVisible());
        verifyAction(section, ContentSuggestionsAdditionalAction.FETCH);
    }

    @Test
    @Feature({"Ntp"})
    public void testNoAction() {
        // Test where no action is enabled.

        // Spy so that VerifyAction can check methods being called.
        SuggestionsCategoryInfo info =
                spy(new CategoryInfoBuilder(TEST_CATEGORY_ID).showIfEmpty().build());
        SuggestionsSection section = createSection(info);

        assertFalse(section.getActionItemForTesting().isVisible());
        verifyAction(section, ContentSuggestionsAdditionalAction.NONE);
    }

    @Test
    @Feature({"Ntp"})
    public void testFetchMoreProgressDisplay() {
        final int suggestionCount = 3;
        // Spy so that VerifyAction can check methods being called.
        SuggestionsCategoryInfo info =
                spy(new CategoryInfoBuilder(TEST_CATEGORY_ID)
                                .withAction(ContentSuggestionsAdditionalAction.FETCH)
                                .showIfEmpty()
                                .build());
        SuggestionsSection section = createSection(info);
        section.setStatus(CategoryStatus.AVAILABLE);
        section.appendSuggestions(createDummySuggestions(suggestionCount, TEST_CATEGORY_ID), true,
                /* reportPrefetchedSuggestionsCount = */ false);
        assertEquals(ActionItem.State.BUTTON, section.getActionItemForTesting().getState());

        // Tap the button
        verifyAction(section, ContentSuggestionsAdditionalAction.FETCH);
        assertEquals(
                ActionItem.State.MORE_BUTTON_LOADING, section.getActionItemForTesting().getState());

        // Simulate receiving suggestions.
        section.setStatus(CategoryStatus.AVAILABLE);
        section.appendSuggestions(createDummySuggestions(suggestionCount, TEST_CATEGORY_ID), false,
                /* reportPrefetchedSuggestionsCount = */ false);
        assertEquals(ActionItem.State.BUTTON, section.getActionItemForTesting().getState());
    }

    /**
     * Tests that the More button appends new suggestions after dismissing all items. The tricky
     * condition is that if a section is empty, we issue a fetch instead of a fetch-more. This means
     * we are using the 'updateModels()' flow to append to the list the user is looking at.
     */
    @Test
    @Feature({"Ntp"})
    public void testFetchMoreAfterDismissAll() {
        final int suggestionCount = 10;
        SuggestionsSection section = createSection(new CategoryInfoBuilder(REMOTE_TEST_CATEGORY)
                .withAction(ContentSuggestionsAdditionalAction.FETCH)
                .showIfEmpty()
                .build());
        section.setStatus(CategoryStatus.AVAILABLE);
        List<SnippetArticle> suggestions =
                createDummySuggestions(suggestionCount, REMOTE_TEST_CATEGORY);
        section.appendSuggestions(suggestions,
                /* keepSectionSize = */ true, /* reportPrefetchedSuggestionsCount = */ false);
        assertEquals(ActionItem.State.BUTTON, section.getActionItemForTesting().getState());
        assertEquals(10, section.getSuggestionsCount());
        assertTrue(section.getCategoryInfo().isRemote());

        // Dismiss all suggestions.
        for (int i = 0; i < suggestionCount; i++) {
            suggestions.get(i).mExposed = true;
            @SuppressWarnings("unchecked")
            Callback<String> callback = mock(Callback.class);
            section.dismissItem(1, callback);
            verify(callback).onResult(anyString());
        }
        assertEquals(0, section.getSuggestionsCount());

        // Tap the button -- we handle this case explicitly here to avoid complexity in
        // verifyAction().
        section.getActionItemForTesting().performAction(mUiDelegate, null, null);
        verify(mUiDelegate.getSuggestionsSource(), times(1)).fetchRemoteSuggestions();

        // Simulate the arrival of new data.
        // In production, fetchRemoteSuggestions() triggers a fetch through the bridge. We simulate
        // the functionality triggered by new data at the bridge.
        mSuggestionsSource.setStatusForCategory(REMOTE_TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSuggestionsSource.createAndSetSuggestions(10, REMOTE_TEST_CATEGORY);
        section.setStatus(CategoryStatus.AVAILABLE);
        section.updateSuggestions();

        assertEquals(10, section.getSuggestionsCount());

        assertFalse(section.isDataStale());
    }

    @Test
    @Feature("Ntp")
    public void testFetchMoreFailure() {
        SuggestionsSection section = createSection(new CategoryInfoBuilder(REMOTE_TEST_CATEGORY)
                .withAction(ContentSuggestionsAdditionalAction.FETCH)
                .showIfEmpty()
                .build());
        section.setStatus(CategoryStatus.AVAILABLE);
        section.appendSuggestions(createDummySuggestions(10, REMOTE_TEST_CATEGORY),
                /* keepSectionSize = */ true, /* reportPrefetchedSuggestionsCount = */ false);
        assertEquals(ActionItem.State.BUTTON, section.getActionItemForTesting().getState());
        assertTrue(section.getCategoryInfo().isRemote());

        // Tap the button, providing a Runnable for when it fails.
        Runnable sectionOnFailureRunnable = mock(Runnable.class);
        section.getActionItemForTesting()
                .performAction(mUiDelegate, sectionOnFailureRunnable, null);

        // Ensure the tap leads to a fetch request on the source with a (potentially different)
        // failure Runnable.
        ArgumentCaptor<Runnable> sourceOnFailureRunnable = ArgumentCaptor.forClass(Runnable.class);
        verify(mUiDelegate.getSuggestionsSource(), times(1)).fetchSuggestions(
                eq(REMOTE_TEST_CATEGORY), any(), any(), sourceOnFailureRunnable.capture());

        // Ensure the progress spinner is shown.
        assertEquals(
                ActionItem.State.MORE_BUTTON_LOADING, section.getActionItemForTesting().getState());

        // Simulate a failure.
        sourceOnFailureRunnable.getValue().run();

        // Ensure we're back to the button and the section's failure runnable has been run.
        assertEquals(ActionItem.State.BUTTON, section.getActionItemForTesting().getState());
        verify(sectionOnFailureRunnable, times(1)).run();
    }

    @SuppressWarnings("unchecked")
    @Test
    @Feature("Ntp")
    public void testFetchMoreNoSuggestions() {
        SuggestionsSection section = createSection(new CategoryInfoBuilder(REMOTE_TEST_CATEGORY)
                .withAction(ContentSuggestionsAdditionalAction.FETCH)
                .showIfEmpty()
                .build());
        section.setStatus(CategoryStatus.AVAILABLE);
        section.appendSuggestions(createDummySuggestions(10, REMOTE_TEST_CATEGORY),
                /* keepSectionSize = */ true, /* reportPrefetchedSuggestionsCount = */ false);
        assertEquals(ActionItem.State.BUTTON, section.getActionItemForTesting().getState());
        assertTrue(section.getCategoryInfo().isRemote());

        // Tap the button, providing a Runnable for when it succeeds with no suggestions.
        Runnable sectionOnNoNewSuggestionsRunnable = mock(Runnable.class);
        section.getActionItemForTesting()
                .performAction(mUiDelegate, null, sectionOnNoNewSuggestionsRunnable);

        // Ensure the tap leads to a fetch request on the source with a (potentially different)
        // failure Runnable.
        ArgumentCaptor<Callback> sourceOnSuccessCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mUiDelegate.getSuggestionsSource(), times(1)).fetchSuggestions(
                eq(REMOTE_TEST_CATEGORY), any(), sourceOnSuccessCallback.capture(), any());

        // Check the runnable isn't called if we return suggestions.
        sourceOnSuccessCallback.getValue().onResult(
                createDummySuggestions(2, REMOTE_TEST_CATEGORY));
        verify(sectionOnNoNewSuggestionsRunnable, times(0)).run();

        // Check the runnable is called when we return no suggestions.
        sourceOnSuccessCallback.getValue().onResult(new LinkedList<SnippetArticle>());
        verify(sectionOnNoNewSuggestionsRunnable, times(1)).run();
    }

    /**
     * Tests that the UI updates on updated suggestions.
     */
    @Test
    @Feature({"Ntp"})
    public void testUpdateSectionReplacesSuggestions() {
        SuggestionsSection section = createSectionWithSuggestions(
                mSuggestionsSource.createAndSetSuggestions(4, TEST_CATEGORY_ID));
        assertEquals(4, section.getSuggestionsCount());

        mSuggestionsSource.createAndSetSuggestions(3, TEST_CATEGORY_ID);
        section.updateSuggestions();
        verify(mObserver).onItemRangeRemoved(section, 1, 4);
        verify(mObserver).onItemRangeInserted(section, 1, 3);
        assertEquals(3, section.getSuggestionsCount());

        assertFalse(section.isDataStale());
    }

    /**
     * Tests that the UI does not update when updating is disabled by a parameter.
     */
    @Test
    @Feature({"Ntp"})
    public void testUpdateSectionDoesNothingWhenReplacingIsDisabled() {
        // Override variation params for the test.
        HashMap<String, String> params = new HashMap<>();
        params.put("ignore_updates_for_existing_suggestions", "true");
        CardsVariationParameters.setTestVariationParams(params);

        SuggestionsSection section = createSectionWithSuggestions(
                mSuggestionsSource.createAndSetSuggestions(4, TEST_CATEGORY_ID));
        assertEquals(4, section.getSuggestionsCount());

        mSuggestionsSource.createAndSetSuggestions(3, TEST_CATEGORY_ID);
        section.updateSuggestions();
        verify(mObserver, never())
                .onItemRangeRemoved(any(ListObservable.class), anyInt(), anyInt());
        verify(mObserver, never())
                .onItemRangeInserted(any(ListObservable.class), anyInt(), anyInt());
        assertEquals(4, section.getSuggestionsCount());

        assertTrue(section.isDataStale());
    }

    /**
     * Tests that the UI does not update the first item of the section if it has been viewed.
     */
    @Test
    @Feature({"Ntp"})
    public void testUpdateSectionDoesNotReplaceFirstSuggestionWhenSeen() {
        List<SnippetArticle> snippets =
                mSuggestionsSource.createAndSetSuggestions(4, TEST_CATEGORY_ID, "old");
        // Copy the list when passing to the section - it may alter it but we later need it.
        SuggestionsSection section =
                createSectionWithSuggestions(new ArrayList<>(snippets));
        assertEquals(4, section.getSuggestionsCount());

        snippets.get(0).mExposed = true;

        List<SnippetArticle> newSnippets =
                mSuggestionsSource.createAndSetSuggestions(3, TEST_CATEGORY_ID, "new");
        section.updateSuggestions();
        verify(mObserver).onItemRangeRemoved(section, 2, 3);
        verify(mObserver).onItemRangeInserted(section, 2, 2);
        assertEquals(3, section.getSuggestionsCount());
        List<SnippetArticle> sectionSuggestions = getSuggestions(section);
        assertEquals(snippets.get(0), sectionSuggestions.get(0));
        assertNotEquals(snippets.get(1), sectionSuggestions.get(1));
        assertEquals(newSnippets.get(0), sectionSuggestions.get(1));
        assertNotEquals(snippets.get(2), sectionSuggestions.get(2));
        assertEquals(newSnippets.get(1), sectionSuggestions.get(2));

        assertTrue(section.isDataStale());
    }

    /**
     * Tests that the UI does not update the first two items of the section if they have been
     * viewed.
     */
    @Test
    @Feature({"Ntp"})
    public void testUpdateSectionDoesNotReplaceFirstTwoSuggestionWhenSeen() {
        List<SnippetArticle> snippets =
                mSuggestionsSource.createAndSetSuggestions(4, TEST_CATEGORY_ID, "old");
        // Copy the list when passing to the section - it may alter it but we later need it.
        SuggestionsSection section =
                createSectionWithSuggestions(new ArrayList<>(snippets));
        assertEquals(4, section.getSuggestionsCount());

        snippets.get(0).mExposed = true;
        snippets.get(1).mExposed = true;

        List<SnippetArticle> newSnippets =
                mSuggestionsSource.createAndSetSuggestions(3, TEST_CATEGORY_ID, "new");
        section.updateSuggestions();
        verify(mObserver).onItemRangeRemoved(section, 3, 2);
        verify(mObserver).onItemRangeInserted(section, 3, 1);
        assertEquals(3, section.getSuggestionsCount());
        List<SnippetArticle> sectionSuggestions = getSuggestions(section);
        assertEquals(snippets.get(0), sectionSuggestions.get(0));
        assertEquals(snippets.get(1), sectionSuggestions.get(1));
        assertNotEquals(snippets.get(2), sectionSuggestions.get(2));
        assertEquals(newSnippets.get(0), sectionSuggestions.get(2));

        assertTrue(section.isDataStale());
    }

    /**
     * Tests that the UI does not update any items of the section if the new list is shorter than
     * what has been viewed.
     */
    @Test
    @Feature({"Ntp"})
    public void testUpdateSectionDoesNothingWhenNewListIsShorterThanItemsSeen() {
        List<SnippetArticle> snippets = createDummySuggestions(4, TEST_CATEGORY_ID, "old");
        // Copy the list when passing to the section - it may alter it but we later need it.
        SuggestionsSection section =
                createSectionWithSuggestions(new ArrayList<>(snippets));
        assertEquals(4, section.getSuggestionsCount());

        // Mark first two suggestions as seen.
        snippets.get(0).mExposed = true;
        snippets.get(1).mExposed = true;

        mSuggestionsSource.createAndSetSuggestions(1, TEST_CATEGORY_ID);
        section.updateSuggestions();
        // Even though the new list has just one suggestion, we need to keep the two seen ones
        // around.
        verify(mObserver).onItemRangeRemoved(section, 3, 2);
        verify(mObserver, never())
                .onItemRangeInserted(any(ListObservable.class), anyInt(), anyInt());
        assertEquals(2, section.getSuggestionsCount());
        List<SnippetArticle> sectionSuggestions = getSuggestions(section);
        assertEquals(snippets.get(0), sectionSuggestions.get(0));
        assertEquals(snippets.get(1), sectionSuggestions.get(1));

        assertTrue(section.isDataStale());
    }

    /**
     * Tests that the UI updates items of the section if which have not been seen.
     */
    @Test
    @Feature({"Ntp"})
    public void testUiUpdatesNotSeenItems() {
        List<SnippetArticle> snippets = createDummySuggestions(4, TEST_CATEGORY_ID, "old");
        // Copy the list when passing to the section - it may alter it but we later need it.
        SuggestionsSection section = createSectionWithSuggestions(new ArrayList<>(snippets));
        assertEquals(4, section.getSuggestionsCount());

        // Mark first two suggestions as seen.
        snippets.get(0).mExposed = true;
        snippets.get(1).mExposed = true;

        // Remove the first item (already seen) and the last item (not yet seen).
        List<SnippetArticle> sectionSuggestions = getSuggestions(section);
        section.removeSuggestionById(sectionSuggestions.get(0).mIdWithinCategory);
        section.removeSuggestionById(sectionSuggestions.get(3).mIdWithinCategory);
        // Two suggestions left, one seen, one unseen.
        assertEquals(2, section.getSuggestionsCount());

        Mockito.<ListObserver>reset(mObserver);

        mSuggestionsSource.setSuggestionsForCategory(
                TEST_CATEGORY_ID, createDummySuggestions(4, TEST_CATEGORY_ID));
        section.updateSuggestions();
        assertEquals(4, section.getSuggestionsCount());
        // Verify the list.
        sectionSuggestions = getSuggestions(section);
        // Only the seen suggestion should stay in the list.
        assertEquals(snippets.get(1), sectionSuggestions.get(0));
        assertEquals(mSuggestionsSource.getSuggestionsForCategory(TEST_CATEGORY_ID).get(0),
                sectionSuggestions.get(1));
        assertEquals(mSuggestionsSource.getSuggestionsForCategory(TEST_CATEGORY_ID).get(1),
                sectionSuggestions.get(2));
        assertEquals(mSuggestionsSource.getSuggestionsForCategory(TEST_CATEGORY_ID).get(2),
                sectionSuggestions.get(3));

        assertTrue(section.isDataStale());
    }

    /**
     * Tests that the UI does not update when the section has been viewed.
     */
    @Test
    @Feature({"Ntp"})
    public void testUpdateSectionDoesNothingWhenAllSeen() {
        List<SnippetArticle> snippets = createDummySuggestions(4, TEST_CATEGORY_ID, "old");
        SuggestionsSection section = createSectionWithSuggestions(snippets);
        assertEquals(4, section.getSuggestionsCount());

        for (SnippetArticle snippet : snippets) snippet.mExposed = true;

        mSuggestionsSource.setSuggestionsForCategory(
                TEST_CATEGORY_ID, createDummySuggestions(3, TEST_CATEGORY_ID));
        section.updateSuggestions();
        verify(mObserver, never())
                .onItemRangeRemoved(any(ListObservable.class), anyInt(), anyInt());
        verify(mObserver, never())
                .onItemRangeInserted(any(ListObservable.class), anyInt(), anyInt());

        // All old snippets should be in place.
        assertEquals(snippets, getSuggestions(section));

        assertTrue(section.isDataStale());
    }

    /**
     * Tests that the UI does not update when anything has been appended.
     */
    @Test
    @Feature({"Ntp"})
    public void testUpdateSectionDoesNothingWhenUserAppended() {
        List<SnippetArticle> snippets = createDummySuggestions(4, TEST_CATEGORY_ID, "old");
        SuggestionsSection section = createSectionWithSuggestions(snippets);

        // Append another 3 suggestions.
        List<SnippetArticle> appendedSnippets =
                createDummySuggestions(3, TEST_CATEGORY_ID, "appended");
        section.appendSuggestions(appendedSnippets, /* keepSectionSize = */ false,
                /* reportPrefetchedSuggestionsCount = */ false);

        // All 7 snippets should be in place.
        snippets.addAll(appendedSnippets);
        assertEquals(snippets, getSuggestions(section));

        // Try to replace them with another list. Should have no effect.
        mSuggestionsSource.setSuggestionsForCategory(
                TEST_CATEGORY_ID, createDummySuggestions(5, TEST_CATEGORY_ID, "new"));
        section.updateSuggestions();

        // All previous snippets should be in place.
        assertEquals(snippets, getSuggestions(section));

        assertTrue(section.isDataStale());
    }

    @Test
    @Feature({"Ntp"})
    public void testCardIsNotifiedWhenBecomingFirst() {
        List<SnippetArticle> suggestions = createDummySuggestions(5, /* categoryId = */ 42);
        SuggestionsSection section = createSectionWithFetchAction(false);
        section.appendSuggestions(suggestions, /* keepSectionSize = */ true,
                /* reportPrefetchedSuggestionsCount = */ false);
        Mockito.<ListObserver>reset(mObserver);

        // Remove the first card. The second one should get the update.
        section.removeSuggestionById(suggestions.get(0).mIdWithinCategory);
        verify(mObserver).onItemRangeChanged(
                same(section), eq(1), eq(1), any(PartialBindCallback.class));
    }

    @Test
    @Feature({"Ntp"})
    public void testCardIsNotifiedWhenBecomingLast() {
        List<SnippetArticle> suggestions = createDummySuggestions(5, /* categoryId = */ 42);
        SuggestionsSection section = createSectionWithFetchAction(false);
        section.appendSuggestions(suggestions, /* keepSectionSize = */ true,
                /* reportPrefetchedSuggestionsCount = */ false);
        Mockito.<ListObserver>reset(mObserver);

        // Remove the last card. The penultimate one should get the update.
        section.removeSuggestionById(suggestions.get(4).mIdWithinCategory);
        verify(mObserver).onItemRangeChanged(
                same(section), eq(4), eq(1), any(PartialBindCallback.class));
    }

    @Test
    @Feature({"Ntp"})
    public void testCardIsNotifiedWhenBecomingSoleCard() {
        List<SnippetArticle> suggestions = createDummySuggestions(2, /* categoryId = */ 42);
        SuggestionsSection section = createSectionWithFetchAction(false);
        section.appendSuggestions(suggestions, /* keepSectionSize = */ true,
                /* reportPrefetchedSuggestionsCount = */ false);
        Mockito.<ListObserver>reset(mObserver);

        // Remove the last card. The penultimate one should get the update.
        section.removeSuggestionById(suggestions.get(1).mIdWithinCategory);
        verify(mObserver).onItemRangeChanged(
                same(section), eq(1), eq(1), any(PartialBindCallback.class));
    }

    @Test
    @Feature({"Ntp"})
    public void testGetItemDismissalGroupWithSuggestions() {
        List<SnippetArticle> suggestions = createDummySuggestions(5, TEST_CATEGORY_ID);
        SuggestionsSection section = createSectionWithFetchAction(false);
        section.appendSuggestions(suggestions, /* keepSectionSize = */ true,
                /* reportPrefetchedSuggestionsCount = */ false);

        assertThat(section.getItemDismissalGroup(1).size(), is(1));
        assertThat(section.getItemDismissalGroup(1), contains(1));
    }

    @Test
    @Feature({"Ntp"})
    public void testGetItemDismissalGroupWithActionItem() {
        SuggestionsSection section = createSectionWithFetchAction(true);
        assertThat(section.getItemDismissalGroup(1).size(), is(0));
    }

    @Test
    @Feature({"Ntp"})
    public void testGetItemDismissalGroupWithoutActionItem() {
        SuggestionsSection section = createSectionWithFetchAction(false);
        assertThat(section.getItemDismissalGroup(1).size(), is(0));
    }

    @Test
    @Feature({"Ntp"})
    public void testCardIsNotifiedWhenNotTheLastAnymore() {
        List<SnippetArticle> suggestions = createDummySuggestions(5, /* categoryId = */ 42);
        SuggestionsSection section = createSectionWithFetchAction(false);

        section.appendSuggestions(suggestions, /* keepSectionSize = */ true,
                /* reportPrefetchedSuggestionsCount = */ false);
        Mockito.<ListObserver>reset(mObserver);

        section.appendSuggestions(createDummySuggestions(2, /* categoryId = */ 42, "new"),
                /* keepSectionSize = */ false, /* reportPrefetchedSuggestionsCount = */ false);
        verify(mObserver).onItemRangeChanged(
                same(section), eq(5), eq(1), any(PartialBindCallback.class));
    }

    private SuggestionsSection createSectionWithSuggestions(List<SnippetArticle> snippets) {
        SuggestionsSection section = createSectionWithFetchAction(true);
        section.setStatus(CategoryStatus.AVAILABLE);
        section.appendSuggestions(snippets, /* keepSectionSize = */ true,
                /* reportPrefetchedSuggestionsCount = */ false);

        // Reset any notification invocations on the parent from setting the initial list
        // of suggestions.
        Mockito.<ListObserver>reset(mObserver);
        return section;
    }

    @SafeVarargs
    private static <T> Set<T> setOf(T... elements) {
        return new TreeSet<>(Arrays.asList(elements));
    }

    private static List<SnippetArticle> getSuggestions(SuggestionsSection section) {
        ArrayList<SnippetArticle> suggestions = new ArrayList<>(section.getSuggestionsCount());
        for (int i = 0; i < section.getSuggestionsCount(); i++) {
            suggestions.add(section.getSuggestionForTesting(i));
        }
        return suggestions;
    }

    private SuggestionsSection createSectionWithFetchAction(boolean hasFetchAction) {
        CategoryInfoBuilder builder = new CategoryInfoBuilder(TEST_CATEGORY_ID).showIfEmpty();
        if (hasFetchAction) builder.withAction(ContentSuggestionsAdditionalAction.FETCH);
        return createSection(builder.build());
    }

    private SuggestionsSection createSection(SuggestionsCategoryInfo info) {
        SuggestionsSection section = new SuggestionsSection(mDelegate, mUiDelegate,
                mock(SuggestionsRanker.class), mBridge, info, mSigninManager);
        section.addObserver(mObserver);
        return section;
    }

    @SuppressWarnings("unchecked")
    private void verifyAction(
            SuggestionsSection section, @ContentSuggestionsAdditionalAction int action) {
        if (action != ContentSuggestionsAdditionalAction.NONE) {
            section.getActionItemForTesting().performAction(mUiDelegate, null, null);
        }

        // noinspection unchecked -- See https://crbug.com/740162 for rationale.
        verify(mUiDelegate.getSuggestionsSource(),
                (action == ContentSuggestionsAdditionalAction.FETCH ? times(1) : never()))
                .fetchSuggestions(anyInt(), any(String[].class), any(Callback.class),
                        any(Runnable.class));
    }
}
