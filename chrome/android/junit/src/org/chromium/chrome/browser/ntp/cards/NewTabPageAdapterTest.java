// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsUnitTestUtils.makeUiConfig;
import static org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.createDummySuggestions;
import static org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.registerCategory;

import android.accounts.Account;
import android.content.res.Resources;
import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.AdapterDataObserver;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowResources;

import org.chromium.base.Callback;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder.PartialBindCallback;
import org.chromium.chrome.browser.ntp.cards.SignInPromo.SigninObserver;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.suggestions.ContentSuggestionsAdditionalAction;
import org.chromium.chrome.browser.suggestions.DestructionObserver;
import org.chromium.chrome.browser.suggestions.SuggestionsEventReporter;
import org.chromium.chrome.browser.suggestions.SuggestionsRanker;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.test.support.DisableHistogramsRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.CategoryInfoBuilder;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.net.NetworkChangeNotifier;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;

/**
 * Unit tests for {@link NewTabPageAdapter}. {@link AccountManagerFacade} uses AsyncTasks, thus
 * the need for {@link CustomShadowAsyncTask}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {CustomShadowAsyncTask.class})
@DisableFeatures({ChromeFeatureList.CONTENT_SUGGESTIONS_SCROLL_TO_LOAD,
        ChromeFeatureList.SIMPLIFIED_NTP, ChromeFeatureList.CHROME_DUET,
        ChromeFeatureList.UNIFIED_CONSENT})
public class NewTabPageAdapterTest {
    @Rule
    public DisableHistogramsRule mDisableHistogramsRule = new DisableHistogramsRule();

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @CategoryInt
    private static final int TEST_CATEGORY = 42;

    private FakeSuggestionsSource mSource;
    private NewTabPageAdapter mAdapter;
    @Mock
    private SigninManager mMockSigninManager;
    @Mock
    private OfflinePageBridge mOfflinePageBridge;
    @Mock
    private SuggestionsUiDelegate mUiDelegate;
    @Mock
    private PrefServiceBridge mPrefServiceBridge;

    /**
     * Stores information about a section that should be present in the adapter.
     */
    private static class SectionDescriptor {
        // TODO(https://crbug.com/754763): Smells. To be cleaned up.
        public boolean mIsSignInPromo;

        public boolean mHeader = true;
        public List<SnippetArticle> mSuggestions;
        public boolean mStatusCard;
        public boolean mViewAllButton;
        public boolean mFetchButton;
        public boolean mProgressItem;

        public SectionDescriptor() {}

        public SectionDescriptor(List<SnippetArticle> suggestions) {
            mSuggestions = suggestions;
        }

        public SectionDescriptor withViewAllButton() {
            assertFalse(mProgressItem);
            mViewAllButton = true;
            return this;
        }

        public SectionDescriptor withFetchButton() {
            assertFalse(mProgressItem);
            mFetchButton = true;
            return this;
        }

        public SectionDescriptor withProgress() {
            assertFalse(mViewAllButton);
            assertFalse(mFetchButton);
            mProgressItem = true;
            return this;
        }

        public SectionDescriptor isSigninPromo() {
            mIsSignInPromo = true;
            return this;
        }

        public SectionDescriptor withStatusCard() {
            mStatusCard = true;
            return this;
        }
    }

    /**
     * Checks the list of items from the adapter against a sequence of expectation, which is
     * expressed as a sequence of calls to the {@code expect...()} methods.
     */
    private static class ItemsMatcher { // TODO(pke): Find better name.
        private final List<String> mExpectedDescriptions = new ArrayList<>();
        private final List<String> mActualDescriptions = new ArrayList<>();

        public ItemsMatcher(RecyclerViewAdapter.Delegate root) {
            for (int i = 0; i < root.getItemCount(); i++) {
                mActualDescriptions.add(root.describeItemForTesting(i));
            }
        }

        private void expectDescription(String description) {
            mExpectedDescriptions.add(description);
        }

        public void expectSection(SectionDescriptor descriptor) {
            if (descriptor.mIsSignInPromo) {
                expectDescription("SIGN_IN_PROMO");
                return;
            }

            if (descriptor.mHeader) {
                expectDescription("HEADER");
            }

            for (SnippetArticle suggestion : descriptor.mSuggestions) {
                expectDescription(
                        String.format(Locale.US, "SUGGESTION(%1.42s)", suggestion.mTitle));
            }

            if (descriptor.mStatusCard) {
                expectDescription("NO_SUGGESTIONS");
            }

            if (descriptor.mViewAllButton) {
                expectDescription(String.format(
                        Locale.US, "ACTION(%d)", ContentSuggestionsAdditionalAction.VIEW_ALL));
            }

            if (descriptor.mFetchButton) {
                expectDescription(String.format(
                        Locale.US, "ACTION(%d)", ContentSuggestionsAdditionalAction.FETCH));
            }

            if (descriptor.mProgressItem) {
                expectDescription("PROGRESS");
            }
        }

        public void expectAboveTheFoldItem() {
            expectDescription("ABOVE_THE_FOLD");
        }

        public void expectAllDismissedItem() {
            expectDescription("ALL_DISMISSED");
        }

        public void expectFooter() {
            expectDescription("FOOTER");
        }

        public void finish() {
            assertThat(mActualDescriptions, is(mExpectedDescriptions));
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Ensure that NetworkChangeNotifier is initialized.
        if (!NetworkChangeNotifier.isInitialized()) {
            NetworkChangeNotifier.init();
        }
        NetworkChangeNotifier.forceConnectivityState(true);

        // Set empty variation params for the test.
        CardsVariationParameters.setTestVariationParams(new HashMap<>());

        // Initialise AccountManagerFacade and add one dummy account.
        FakeAccountManagerDelegate fakeAccountManager = new FakeAccountManagerDelegate(
                FakeAccountManagerDelegate.ENABLE_PROFILE_DATA_SOURCE);
        AccountManagerFacade.overrideAccountManagerFacadeForTests(fakeAccountManager);
        Account account = AccountManagerFacade.createAccountFromName("test@gmail.com");
        fakeAccountManager.addAccountHolderExplicitly(new AccountHolder.Builder(account).build());
        assertFalse(AccountManagerFacade.get().isUpdatePending());

        // Initialise the sign in state. We will be signed in by default in the tests.
        assertFalse(ChromePreferenceManager.getInstance().readBoolean(
                ChromePreferenceManager.NTP_SIGNIN_PROMO_DISMISSED, false));
        SigninManager.setInstanceForTesting(mMockSigninManager);
        when(mMockSigninManager.isSignedInOnNative()).thenReturn(true);
        when(mMockSigninManager.isSignInAllowed()).thenReturn(true);

        mSource = new FakeSuggestionsSource();
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.INITIALIZING);
        mSource.setInfoForCategory(
                TEST_CATEGORY, new CategoryInfoBuilder(TEST_CATEGORY).showIfEmpty().build());

        // Initialize a test instance for PrefServiceBridge.
        when(mPrefServiceBridge.getBoolean(anyInt())).thenReturn(false);
        doNothing().when(mPrefServiceBridge).setBoolean(anyInt(), anyBoolean());
        PrefServiceBridge.setInstanceForTesting(mPrefServiceBridge);

        resetUiDelegate();
        reloadNtp();
    }

    @After
    public void tearDown() {
        CardsVariationParameters.setTestVariationParams(null);
        SigninManager.setInstanceForTesting(null);
        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.NTP_SIGNIN_PROMO_DISMISSED, false);
        ChromePreferenceManager.getInstance().clearNewTabPageSigninPromoSuppressionPeriodStart();
        PrefServiceBridge.setInstanceForTesting(null);
    }

    /**
     * Tests the content of the adapter under standard conditions: on start and after a suggestions
     * fetch.
     */
    @Test
    @Feature({"Ntp"})
    public void testSuggestionLoading() {
        assertItemsFor(sectionWithStatusCard().withProgress());

        final int numSuggestions = 3;
        List<SnippetArticle> suggestions = createDummySuggestions(numSuggestions, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);

        assertItemsFor(section(suggestions));
    }

    /**
     * Tests that the adapter keeps listening for suggestion updates.
     */
    @Test
    @Feature({"Ntp"})
    public void testSuggestionLoadingInitiallyEmpty() {
        // If we don't get anything, we should be in the same situation as the initial one.
        mSource.setSuggestionsForCategory(TEST_CATEGORY, new ArrayList<>());
        assertItemsFor(sectionWithStatusCard().withProgress());

        // We should load new suggestions when we get notified about them.
        final int numSuggestions = 5;

        List<SnippetArticle> suggestions = createDummySuggestions(numSuggestions, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);

        assertItemsFor(section(suggestions));
    }

    /**
     * Tests that the adapter clears the suggestions when asked to.
     */
    @Test
    @Feature({"Ntp"})
    public void testSuggestionClearing() {
        List<SnippetArticle> suggestions = createDummySuggestions(4, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(suggestions));

        // If we get told that the category is enabled, we just leave the current suggestions do not
        // clear them.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        assertItemsFor(section(suggestions));

        // When the category is disabled, the section should go away completely.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.CATEGORY_EXPLICITLY_DISABLED);
        assertItemsFor();

        // Now we're in the "all dismissed" state. No suggestions should be accepted.
        suggestions = createDummySuggestions(6, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor();

        // After a full refresh, the adapter should accept suggestions again.
        mSource.fireFullRefreshRequired();
        assertItemsFor(section(suggestions));
    }

    /**
     * Tests that the adapter loads suggestions only when the status is favorable.
     */
    @Test
    @Feature({"Ntp"})
    public void testSuggestionLoadingBlock() {
        List<SnippetArticle> suggestions = createDummySuggestions(3, TEST_CATEGORY);

        // By default, status is INITIALIZING, so we can load suggestions.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(suggestions));

        // Add another suggestion.
        suggestions.add(new SnippetArticle(TEST_CATEGORY, "https://site.com/url3", "title3", "pub3",
                "https://site.com/url3", 0, 0, 0, false, /* thumbnailDominantColor = */ null));

        // When the provider is removed, we should not be able to load suggestions. The UI should
        // stay the same though.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.NOT_PROVIDED);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(suggestions.subList(0, 3)));

        // INITIALIZING lets us load suggestions still.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.INITIALIZING);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(sectionWithStatusCard().withProgress());

        // The adapter should now be waiting for new suggestions and the fourth one should appear.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(suggestions));

        // When the category gets disabled, the section should go away and not load any suggestions.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.CATEGORY_EXPLICITLY_DISABLED);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor();
    }

    /**
     * Tests how the loading indicator reacts to status changes.
     */
    @Test
    @Feature({"Ntp"})
    public void testProgressIndicatorDisplay() {
        SuggestionsSection section = mAdapter.getSectionListForTesting().getSection(TEST_CATEGORY);
        ActionItem item = section.getActionItemForTesting();

        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.INITIALIZING);
        assertEquals(ActionItem.State.LOADING, item.getState());

        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        assertEquals(ActionItem.State.HIDDEN, item.getState());

        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE_LOADING);
        assertEquals(ActionItem.State.LOADING, item.getState());

        // After the section gets disabled, it should gone completely, so checking the progress
        // indicator doesn't make sense anymore.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.CATEGORY_EXPLICITLY_DISABLED);
        assertEquals(mAdapter.getSectionListForTesting().getSection(TEST_CATEGORY), null);
    }

    /**
     * Tests that the entire section disappears if its status switches to LOADING_ERROR or
     * CATEGORY_EXPLICITLY_DISABLED. Also tests that they are not shown when the NTP reloads.
     */
    @Test
    @Feature({"Ntp"})
    public void testSectionClearingWhenUnavailable() {
        List<SnippetArticle> suggestions = createDummySuggestions(5, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(suggestions));

        // When the category goes away with a hard error, the section is cleared from the UI.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.LOADING_ERROR);
        assertItemsFor();

        // Same when loading a new NTP.
        reloadNtp();
        assertItemsFor();

        // Same for CATEGORY_EXPLICITLY_DISABLED.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        reloadNtp();
        assertItemsFor(section(suggestions));
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.CATEGORY_EXPLICITLY_DISABLED);
        assertItemsFor();

        reloadNtp();
        assertItemsFor();
    }

    /**
     * Tests that the UI remains untouched if a category switches to NOT_PROVIDED.
     */
    @Test
    @Feature({"Ntp"})
    public void testUIUntouchedWhenNotProvided() {
        List<SnippetArticle> suggestions = createDummySuggestions(4, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(suggestions));

        // When the category switches to NOT_PROVIDED, UI stays the same.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.NOT_PROVIDED);
        mSource.silentlyRemoveCategory(TEST_CATEGORY);
        assertItemsFor(section(suggestions));

        reloadNtp();
        assertItemsFor();
    }

    /**
     * Tests that the UI updates on updated suggestions.
     */
    @Test
    @Feature({"Ntp"})
    public void testUIUpdatesOnNewSuggestionsWhenOtherSectionSeen() {
        List<SnippetArticle> suggestions = createDummySuggestions(4, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);

        @CategoryInt
        final int otherCategory = TEST_CATEGORY + 1;
        List<SnippetArticle> otherSuggestions = createDummySuggestions(2, otherCategory);
        mSource.setStatusForCategory(otherCategory, CategoryStatus.AVAILABLE);
        mSource.setInfoForCategory(
                otherCategory, new CategoryInfoBuilder(otherCategory).showIfEmpty().build());
        mSource.setSuggestionsForCategory(otherCategory, otherSuggestions);

        reloadNtp();
        assertItemsFor(section(suggestions), section(otherSuggestions));

        // Indicate that the whole section is being viewed.
        for (SnippetArticle article : otherSuggestions) article.mExposed = true;

        List<SnippetArticle> newSuggestions = createDummySuggestions(3, TEST_CATEGORY, "new");
        mSource.setSuggestionsForCategory(TEST_CATEGORY, newSuggestions);
        assertItemsFor(section(newSuggestions), section(otherSuggestions));

        reloadNtp();
        assertItemsFor(section(newSuggestions), section(otherSuggestions));
    }

    /** Tests whether a section stays visible if empty, if required. */
    @Test
    @Feature({"Ntp"})
    public void testSectionVisibleIfEmpty() {
        // Part 1: VisibleIfEmpty = true
        FakeSuggestionsSource suggestionsSource = new FakeSuggestionsSource();
        suggestionsSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.INITIALIZING);
        suggestionsSource.setInfoForCategory(
                TEST_CATEGORY, new CategoryInfoBuilder(TEST_CATEGORY).showIfEmpty().build());

        // 1.1 - Initial state
        when(mUiDelegate.getSuggestionsSource()).thenReturn(suggestionsSource);
        reloadNtp();
        assertItemsFor(sectionWithStatusCard().withProgress());

        // 1.2 - With suggestions
        List<SnippetArticle> suggestions =
                Collections.unmodifiableList(createDummySuggestions(3, TEST_CATEGORY));
        suggestionsSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        suggestionsSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(suggestions));

        // 1.3 - When all suggestions are dismissed
        SuggestionsSection section = mAdapter.getSectionListForTesting().getSection(TEST_CATEGORY);
        assertSectionMatches(section(suggestions), section);
        section.removeSuggestionById(suggestions.get(0).mIdWithinCategory);
        section.removeSuggestionById(suggestions.get(1).mIdWithinCategory);
        section.removeSuggestionById(suggestions.get(2).mIdWithinCategory);
        assertItemsFor(sectionWithStatusCard());

        // Part 2: VisibleIfEmpty = false
        suggestionsSource = new FakeSuggestionsSource();
        suggestionsSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        suggestionsSource.setInfoForCategory(
                TEST_CATEGORY, new CategoryInfoBuilder(TEST_CATEGORY).build());

        // 2.1 - Initial state
        when(mUiDelegate.getSuggestionsSource()).thenReturn(suggestionsSource);
        reloadNtp();
        assertItemsFor();

        // 2.2 - With suggestions
        suggestionsSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        suggestionsSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor();

        // 2.3 - When all suggestions are dismissed - N/A, suggestions don't get added.
    }

    /**
     * Tests that the more button is shown for sections that declare it.
     */
    @Test
    @Feature({"Ntp"})
    public void testViewAllButton() {
        // Part 1: With "View All" action
        FakeSuggestionsSource suggestionsSource = new FakeSuggestionsSource();
        suggestionsSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.INITIALIZING);
        suggestionsSource.setInfoForCategory(TEST_CATEGORY,
                new CategoryInfoBuilder(TEST_CATEGORY)
                        .withAction(ContentSuggestionsAdditionalAction.VIEW_ALL)
                        .showIfEmpty()
                        .build());

        // 1.1 - Initial state.
        when(mUiDelegate.getSuggestionsSource()).thenReturn(suggestionsSource);
        reloadNtp();
        assertItemsFor(sectionWithStatusCard().withProgress());

        // 1.2 - With suggestions.
        List<SnippetArticle> suggestions =
                Collections.unmodifiableList(createDummySuggestions(3, TEST_CATEGORY));
        suggestionsSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        suggestionsSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(suggestions).withViewAllButton());

        // 1.3 - When all suggestions are dismissed.
        SuggestionsSection section = mAdapter.getSectionListForTesting().getSection(TEST_CATEGORY);
        assertSectionMatches(section(suggestions).withViewAllButton(), section);
        section.removeSuggestionById(suggestions.get(0).mIdWithinCategory);
        section.removeSuggestionById(suggestions.get(1).mIdWithinCategory);
        section.removeSuggestionById(suggestions.get(2).mIdWithinCategory);
        assertItemsFor(sectionWithStatusCard().withViewAllButton());

        // Part 1: Without "View All" action
        suggestionsSource = new FakeSuggestionsSource();
        suggestionsSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.INITIALIZING);
        suggestionsSource.setInfoForCategory(
                TEST_CATEGORY, new CategoryInfoBuilder(TEST_CATEGORY).showIfEmpty().build());

        // 2.1 - Initial state.
        when(mUiDelegate.getSuggestionsSource()).thenReturn(suggestionsSource);
        reloadNtp();
        assertItemsFor(sectionWithStatusCard().withProgress());

        // 2.2 - With suggestions.
        suggestionsSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        suggestionsSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(suggestions));

        // 2.3 - When all suggestions are dismissed.
        section = mAdapter.getSectionListForTesting().getSection(TEST_CATEGORY);
        assertSectionMatches(section(suggestions), section);
        section.removeSuggestionById(suggestions.get(0).mIdWithinCategory);
        section.removeSuggestionById(suggestions.get(1).mIdWithinCategory);
        section.removeSuggestionById(suggestions.get(2).mIdWithinCategory);
        assertItemsFor(sectionWithStatusCard());
    }

    /**
     * Tests that the more button is shown for sections that declare it.
     */
    @Test
    @Feature({"Ntp"})
    public void testFetchButton() {
        @CategoryInt
        final int category = TEST_CATEGORY;

        // Part 1: With "Fetch more" action
        FakeSuggestionsSource suggestionsSource = new FakeSuggestionsSource();
        suggestionsSource.setStatusForCategory(category, CategoryStatus.INITIALIZING);
        suggestionsSource.setInfoForCategory(category,
                new CategoryInfoBuilder(category)
                        .withAction(ContentSuggestionsAdditionalAction.FETCH)
                        .showIfEmpty()
                        .build());

        // 1.1 - Initial state.
        when(mUiDelegate.getSuggestionsSource()).thenReturn(suggestionsSource);
        reloadNtp();
        assertItemsFor(sectionWithStatusCard().withProgress());

        // 1.2 - With suggestions.
        List<SnippetArticle> articles =
                Collections.unmodifiableList(createDummySuggestions(3, category));
        suggestionsSource.setStatusForCategory(category, CategoryStatus.AVAILABLE);
        suggestionsSource.setSuggestionsForCategory(category, articles);
        assertItemsFor(section(articles).withFetchButton());

        // 1.3 - When all suggestions are dismissed.
        SuggestionsSection section = mAdapter.getSectionListForTesting().getSection(category);
        assertSectionMatches(section(articles).withFetchButton(), section);
        section.removeSuggestionById(articles.get(0).mIdWithinCategory);
        section.removeSuggestionById(articles.get(1).mIdWithinCategory);
        section.removeSuggestionById(articles.get(2).mIdWithinCategory);
        assertItemsFor(sectionWithStatusCard().withFetchButton());

        // Part 1: Without "Fetch more" action
        suggestionsSource = new FakeSuggestionsSource();
        suggestionsSource.setStatusForCategory(category, CategoryStatus.INITIALIZING);
        suggestionsSource.setInfoForCategory(
                category, new CategoryInfoBuilder(category).showIfEmpty().build());

        // 2.1 - Initial state.
        when(mUiDelegate.getSuggestionsSource()).thenReturn(suggestionsSource);
        reloadNtp();
        assertItemsFor(sectionWithStatusCard().withProgress());

        // 2.2 - With suggestions.
        suggestionsSource.setStatusForCategory(category, CategoryStatus.AVAILABLE);
        suggestionsSource.setSuggestionsForCategory(category, articles);
        assertItemsFor(section(articles));

        // 2.3 - When all suggestions are dismissed.
        section = mAdapter.getSectionListForTesting().getSection(category);
        assertSectionMatches(section(articles), section);
        section.removeSuggestionById(articles.get(0).mIdWithinCategory);
        section.removeSuggestionById(articles.get(1).mIdWithinCategory);
        section.removeSuggestionById(articles.get(2).mIdWithinCategory);
        assertItemsFor(sectionWithStatusCard());
    }

    /**
     * Tests that invalidated suggestions are immediately removed.
     */
    @Test
    @Feature({"Ntp"})
    public void testSuggestionInvalidated() {
        List<SnippetArticle> suggestions = createDummySuggestions(3, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(suggestions));

        SnippetArticle removed = suggestions.remove(1);
        mSource.fireSuggestionInvalidated(TEST_CATEGORY, removed.mIdWithinCategory);
        assertItemsFor(section(suggestions));
    }

    /**
     * Tests that the UI handles dynamically added (server-side) categories correctly.
     */
    @Test
    @Feature({"Ntp"})
    public void testDynamicCategories() {
        List<SnippetArticle> suggestions = createDummySuggestions(3, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);
        assertItemsFor(section(suggestions));

        int dynamicCategory1 = 1010;
        List<SnippetArticle> dynamics1 = createDummySuggestions(5, dynamicCategory1);
        mSource.setInfoForCategory(dynamicCategory1,
                new CategoryInfoBuilder(dynamicCategory1)
                        .withAction(ContentSuggestionsAdditionalAction.VIEW_ALL)
                        .build());
        mSource.setStatusForCategory(dynamicCategory1, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(dynamicCategory1, dynamics1);
        reloadNtp();

        assertItemsFor(section(suggestions), section(dynamics1).withViewAllButton());

        int dynamicCategory2 = 1011;
        List<SnippetArticle> dynamics2 = createDummySuggestions(11, dynamicCategory2);
        mSource.setInfoForCategory(dynamicCategory2,
                new CategoryInfoBuilder(dynamicCategory1).build());
        mSource.setStatusForCategory(dynamicCategory2, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(dynamicCategory2, dynamics2);
        reloadNtp();
        assertItemsFor(
                section(suggestions), section(dynamics1).withViewAllButton(), section(dynamics2));
    }

    @Test
    @Feature({"Ntp"})
    public void testArticlesForYouSection() {
        when(mPrefServiceBridge.getBoolean(eq(Pref.NTP_ARTICLES_LIST_VISIBLE))).thenReturn(true);
        // Show one section of suggestions from the test category, and one section with Articles for
        // You.
        List<SnippetArticle> suggestions = createDummySuggestions(3, TEST_CATEGORY);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, suggestions);

        mSource.setInfoForCategory(KnownCategories.ARTICLES,
                new CategoryInfoBuilder(KnownCategories.ARTICLES).build());
        mSource.setStatusForCategory(KnownCategories.ARTICLES, CategoryStatus.AVAILABLE);
        List<SnippetArticle> articles = createDummySuggestions(3, KnownCategories.ARTICLES);
        mSource.setSuggestionsForCategory(KnownCategories.ARTICLES, articles);

        reloadNtp();
        assertItemsFor(section(suggestions), section(articles));

        // Remove the test category section. The remaining lone Articles for You section should
        // have a header.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.NOT_PROVIDED);
        reloadNtp();
        assertItemsFor(section(articles));
    }

    /**
     * Tests that the order of the categories is kept.
     */
    @Test
    @Feature({"Ntp"})
    public void testCategoryOrder() {
        int[] categories = {TEST_CATEGORY, TEST_CATEGORY + 2, TEST_CATEGORY + 3, TEST_CATEGORY + 4};
        FakeSuggestionsSource suggestionsSource = new FakeSuggestionsSource();
        when(mUiDelegate.getSuggestionsSource()).thenReturn(suggestionsSource);
        registerCategory(suggestionsSource, categories[0], 0);
        registerCategory(suggestionsSource, categories[1], 0);
        registerCategory(suggestionsSource, categories[2], 0);
        registerCategory(suggestionsSource, categories[3], 0);
        reloadNtp();

        List<RecyclerViewAdapter.Delegate<NewTabPageViewHolder, PartialBindCallback>> children =
                mAdapter.getSectionListForTesting().getChildren();
        assertEquals(4, children.size());
        assertEquals(SuggestionsSection.class, children.get(0).getClass());
        assertEquals(categories[0], getCategory(children.get(0)));
        assertEquals(SuggestionsSection.class, children.get(1).getClass());
        assertEquals(categories[1], getCategory(children.get(1)));
        assertEquals(SuggestionsSection.class, children.get(2).getClass());
        assertEquals(categories[2], getCategory(children.get(2)));
        assertEquals(SuggestionsSection.class, children.get(3).getClass());
        assertEquals(categories[3], getCategory(children.get(3)));

        // With a different order.
        suggestionsSource = new FakeSuggestionsSource();
        when(mUiDelegate.getSuggestionsSource()).thenReturn(suggestionsSource);
        registerCategory(suggestionsSource, categories[0], 0);
        registerCategory(suggestionsSource, categories[2], 0);
        registerCategory(suggestionsSource, categories[3], 0);
        registerCategory(suggestionsSource, categories[1], 0);
        reloadNtp();

        children = mAdapter.getSectionListForTesting().getChildren();
        assertEquals(4, children.size());
        assertEquals(SuggestionsSection.class, children.get(0).getClass());
        assertEquals(categories[0], getCategory(children.get(0)));
        assertEquals(SuggestionsSection.class, children.get(1).getClass());
        assertEquals(categories[2], getCategory(children.get(1)));
        assertEquals(SuggestionsSection.class, children.get(2).getClass());
        assertEquals(categories[3], getCategory(children.get(2)));
        assertEquals(SuggestionsSection.class, children.get(3).getClass());
        assertEquals(categories[1], getCategory(children.get(3)));

        // With unknown categories.
        suggestionsSource = new FakeSuggestionsSource();
        when(mUiDelegate.getSuggestionsSource()).thenReturn(suggestionsSource);
        registerCategory(suggestionsSource, categories[0], 0);
        registerCategory(suggestionsSource, categories[2], 0);
        registerCategory(suggestionsSource, categories[3], 0);
        reloadNtp();

        // The adapter is already initialised, it will not accept new categories anymore.
        registerCategory(suggestionsSource, TEST_CATEGORY + 5, 1);
        registerCategory(suggestionsSource, categories[1], 1);

        children = mAdapter.getSectionListForTesting().getChildren();
        assertEquals(3, children.size());
        assertEquals(SuggestionsSection.class, children.get(0).getClass());
        assertEquals(categories[0], getCategory(children.get(0)));
        assertEquals(SuggestionsSection.class, children.get(1).getClass());
        assertEquals(categories[2], getCategory(children.get(1)));
        assertEquals(SuggestionsSection.class, children.get(2).getClass());
        assertEquals(categories[3], getCategory(children.get(2)));
    }

    @Test
    @Feature({"Ntp"})
    public void testChangeNotifications() {
        FakeSuggestionsSource suggestionsSource = spy(new FakeSuggestionsSource());
        registerCategory(suggestionsSource, TEST_CATEGORY, 3);
        when(mUiDelegate.getSuggestionsSource()).thenReturn(suggestionsSource);

        @SuppressWarnings("unchecked")
        Callback<String> itemDismissedCallback = mock(Callback.class);

        reloadNtp();
        AdapterDataObserver dataObserver = mock(AdapterDataObserver.class);
        mAdapter.registerAdapterDataObserver(dataObserver);

        // Adapter content:
        // Idx | Item
        // ----|----------------
        // 0   | Above-the-fold
        // 1   | Header
        // 2-4 | Sugg*3
        // 5   | Action
        // 6   | Footer

        // Dismiss the second suggestion of the second section.
        mAdapter.dismissItem(3, itemDismissedCallback);
        verify(itemDismissedCallback).onResult(anyString());
        verify(dataObserver).onItemRangeRemoved(3, 1);

        // Make sure the call with the updated position works properly.
        mAdapter.dismissItem(3, itemDismissedCallback);
        verify(itemDismissedCallback, times(2)).onResult(anyString());
        verify(dataObserver, times(2)).onItemRangeRemoved(3, 1);

        // Dismiss the last suggestion in the section. We should now show the status card.
        reset(dataObserver);
        mAdapter.dismissItem(2, itemDismissedCallback);
        verify(itemDismissedCallback, times(3)).onResult(anyString());
        verify(dataObserver).onItemRangeRemoved(2, 1); // Suggestion removed
        verify(dataObserver).onItemRangeInserted(2, 1); // Status card added

        // Adapter content:
        // Idx | Item
        // ----|----------------
        // 0   | Above-the-fold
        // 1   | Header
        // 2   | Status
        // 3   | Action
        // 4   | Progress Indicator
        // 5   | Footer

        final int newSuggestionCount = 7;
        reset(dataObserver);
        suggestionsSource.setSuggestionsForCategory(
                TEST_CATEGORY, createDummySuggestions(newSuggestionCount, TEST_CATEGORY));
        verify(dataObserver).onItemRangeInserted(2, newSuggestionCount);
        verify(dataObserver).onItemRangeRemoved(2 + newSuggestionCount, 1);

        // Adapter content:
        // Idx | Item
        // ----|----------------
        // 0   | Above-the-fold
        // 1   | Header
        // 2-8 | Sugg*7
        // 9   | Action
        // 10  | Footer

        reset(dataObserver);
        suggestionsSource.setSuggestionsForCategory(
                TEST_CATEGORY, createDummySuggestions(0, TEST_CATEGORY));
        mAdapter.getSectionListForTesting().onCategoryStatusChanged(
                TEST_CATEGORY, CategoryStatus.CATEGORY_EXPLICITLY_DISABLED);
        // All suggestions as well as the header and the action should be gone.
        verify(dataObserver).onItemRangeRemoved(1, newSuggestionCount + 2);
    }

    @Test
    @Feature({"Ntp"})
    public void testSigninPromo() {
        @CategoryInt
        final int remoteCategory = KnownCategories.REMOTE_CATEGORIES_OFFSET + TEST_CATEGORY;

        when(mMockSigninManager.isSignInAllowed()).thenReturn(true);
        when(mMockSigninManager.isSignedInOnNative()).thenReturn(false);
        resetUiDelegate();
        reloadNtp();

        assertItemsFor(signinPromo(), sectionWithStatusCard().withProgress());
        assertTrue(isSignInPromoVisible());

        List<DestructionObserver> observers = getDestructionObserver(mUiDelegate);
        SuggestionsSource.Observer suggestionsObserver =
                findFirstInstanceOf(observers, SuggestionsSource.Observer.class);
        assertNotNull(suggestionsObserver);

        SignInPromo signInPromo = mAdapter.getSignInPromoForTesting();
        assertNotNull(signInPromo);
        SigninObserver signinObserver = signInPromo.getSigninObserverForTesting();
        assertNotNull(signinObserver);

        signinObserver.onSignedIn();
        assertFalse(isSignInPromoVisible());

        signinObserver.onSignedOut();
        assertTrue(isSignInPromoVisible());

        when(mMockSigninManager.isSignInAllowed()).thenReturn(false);
        signinObserver.onSignInAllowedChanged();
        assertFalse(isSignInPromoVisible());

        when(mMockSigninManager.isSignInAllowed()).thenReturn(true);
        signinObserver.onSignInAllowedChanged();
        assertTrue(isSignInPromoVisible());

        mSource.setRemoteSuggestionsEnabled(false);
        suggestionsObserver.onCategoryStatusChanged(
                remoteCategory, CategoryStatus.CATEGORY_EXPLICITLY_DISABLED);
        assertFalse(isSignInPromoVisible());

        mSource.setRemoteSuggestionsEnabled(true);
        suggestionsObserver.onCategoryStatusChanged(remoteCategory, CategoryStatus.AVAILABLE);
        assertTrue(isSignInPromoVisible());
    }

    @Test
    @Feature({"Ntp"})
    public void testSigninPromoSuppressionActive() {
        when(mMockSigninManager.isSignInAllowed()).thenReturn(true);
        when(mMockSigninManager.isSignedInOnNative()).thenReturn(false);

        // Suppress promo.
        ChromePreferenceManager.getInstance().setNewTabPageSigninPromoSuppressionPeriodStart(
                System.currentTimeMillis());

        resetUiDelegate();
        reloadNtp();
        assertFalse(isSignInPromoVisible());
    }

    @Test
    @Feature({"Ntp"})
    public void testSigninPromoSuppressionExpired() {
        when(mMockSigninManager.isSignInAllowed()).thenReturn(true);
        when(mMockSigninManager.isSignedInOnNative()).thenReturn(false);

        // Suppress promo.
        ChromePreferenceManager preferenceManager = ChromePreferenceManager.getInstance();
        preferenceManager.setNewTabPageSigninPromoSuppressionPeriodStart(
                System.currentTimeMillis() - SignInPromo.SUPPRESSION_PERIOD_MS);

        resetUiDelegate();
        reloadNtp();
        assertTrue(isSignInPromoVisible());

        // SignInPromo should clear shared preference when suppression period ends.
        assertEquals(0, preferenceManager.getNewTabPageSigninPromoSuppressionPeriodStart());
    }

    @Test
    @Feature({"Ntp"})
    @Config(shadows = MyShadowResources.class)
    public void testSigninPromoDismissal() {
        final String signInPromoText = "sign in";
        when(MyShadowResources.sResources.getText(
                     R.string.signin_promo_description_ntp_content_suggestions_legacy))
                .thenReturn(signInPromoText);

        when(mMockSigninManager.isSignInAllowed()).thenReturn(true);
        when(mMockSigninManager.isSignedInOnNative()).thenReturn(false);
        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.NTP_SIGNIN_PROMO_DISMISSED, false);
        reloadNtp();

        final int signInPromoPosition = mAdapter.getFirstPositionForType(ItemViewType.PROMO);
        assertNotEquals(RecyclerView.NO_POSITION, signInPromoPosition);
        @SuppressWarnings("unchecked")
        Callback<String> itemDismissedCallback = mock(Callback.class);
        mAdapter.dismissItem(signInPromoPosition, itemDismissedCallback);

        verify(itemDismissedCallback).onResult(anyString());
        assertFalse(isSignInPromoVisible());
        assertTrue(ChromePreferenceManager.getInstance().readBoolean(
                ChromePreferenceManager.NTP_SIGNIN_PROMO_DISMISSED, false));
        reloadNtp();
        assertFalse(isSignInPromoVisible());
    }

    @Test
    @Feature({"Ntp"})
    public void testAllDismissedVisibility() {
        SignInPromo signInPromo = mAdapter.getSignInPromoForTesting();
        assertNotNull(signInPromo);
        SigninObserver signinObserver = signInPromo.getSigninObserverForTesting();
        assertNotNull(signinObserver);

        @SuppressWarnings("unchecked")
        Callback<String> itemDismissedCallback = mock(Callback.class);

        // By default, there is no All Dismissed item.
        // Adapter content:
        // Idx | Item
        // ----|--------------------
        // 0   | Above-the-fold
        // 1   | Header
        // 2   | Status
        // 3   | Progress Indicator
        // 4   | Footer
        assertEquals(4, mAdapter.getFirstPositionForType(ItemViewType.FOOTER));
        assertEquals(RecyclerView.NO_POSITION,
                mAdapter.getFirstPositionForType(ItemViewType.ALL_DISMISSED));

        // When we remove the section, the All Dismissed item should be there.
        mAdapter.dismissItem(2, itemDismissedCallback);

        verify(itemDismissedCallback).onResult(anyString());

        // Adapter content:
        // Idx | Item
        // ----|--------------------
        // 0   | Above-the-fold
        // 1   | All Dismissed
        assertEquals(
                RecyclerView.NO_POSITION, mAdapter.getFirstPositionForType(ItemViewType.FOOTER));
        assertEquals(1, mAdapter.getFirstPositionForType(ItemViewType.ALL_DISMISSED));

        // On Sign out, the sign in promo should come and the All Dismissed item be removed.
        when(mMockSigninManager.isSignedInOnNative()).thenReturn(false);
        signinObserver.onSignedOut();
        // Adapter content:
        // Idx | Item
        // ----|--------------------
        // 0   | Above-the-fold
        // 1   | Sign In Promo
        // 2   | Footer
        assertEquals(2, mAdapter.getFirstPositionForType(ItemViewType.FOOTER));
        assertEquals(RecyclerView.NO_POSITION,
                mAdapter.getFirstPositionForType(ItemViewType.ALL_DISMISSED));

        // When sign in is disabled, the promo is removed and the All Dismissed item can come back.
        when(mMockSigninManager.isSignInAllowed()).thenReturn(false);
        signinObserver.onSignInAllowedChanged();
        // Adapter content:
        // Idx | Item
        // ----|--------------------
        // 0   | Above-the-fold
        // 1   | All Dismissed
        assertEquals(
                RecyclerView.NO_POSITION, mAdapter.getFirstPositionForType(ItemViewType.FOOTER));
        assertEquals(1, mAdapter.getFirstPositionForType(ItemViewType.ALL_DISMISSED));

        // Re-enabling sign in should only bring the promo back, thus removing the AllDismissed item
        when(mMockSigninManager.isSignInAllowed()).thenReturn(true);
        signinObserver.onSignInAllowedChanged();
        // Adapter content:
        // Idx | Item
        // ----|--------------------
        // 0   | Above-the-fold
        // 1   | Sign In Promo
        // 2   | Footer
        assertEquals(ItemViewType.FOOTER, mAdapter.getItemViewType(2));
        assertEquals(RecyclerView.NO_POSITION,
                mAdapter.getFirstPositionForType(ItemViewType.ALL_DISMISSED));

        // Disabling remote suggestions should remove both the promo and the AllDismissed item
        mSource.setRemoteSuggestionsEnabled(false);
        mAdapter.getSuggestionsSourceObserverForTesting().onCategoryStatusChanged(
                KnownCategories.REMOTE_CATEGORIES_OFFSET + TEST_CATEGORY,
                CategoryStatus.CATEGORY_EXPLICITLY_DISABLED);
        // Adapter content:
        // Idx | Item
        // ----|--------------------
        // 0   | Above-the-fold
        assertEquals(
                RecyclerView.NO_POSITION, mAdapter.getFirstPositionForType(ItemViewType.FOOTER));
        assertEquals(RecyclerView.NO_POSITION,
                mAdapter.getFirstPositionForType(ItemViewType.ALL_DISMISSED));
        assertEquals(
                RecyclerView.NO_POSITION, mAdapter.getFirstPositionForType(ItemViewType.PROMO));
        assertEquals(1, mAdapter.getItemCount());

        // Prepare some suggestions. They should not load because the category is dismissed on
        // the current NTP.
        mSource.setRemoteSuggestionsEnabled(true);
        mAdapter.getSuggestionsSourceObserverForTesting().onCategoryStatusChanged(
                KnownCategories.REMOTE_CATEGORIES_OFFSET + TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
        mSource.setSuggestionsForCategory(TEST_CATEGORY, createDummySuggestions(1, TEST_CATEGORY));
        mSource.setInfoForCategory(TEST_CATEGORY, new CategoryInfoBuilder(TEST_CATEGORY).build());
        assertEquals(3, mAdapter.getItemCount()); // TODO(dgn): rewrite with section descriptors.

        // On Sign in, we should reset the sections, bring back suggestions instead of the All
        // Dismissed item.
        mAdapter.getSectionListForTesting().refreshSuggestions();
        when(mMockSigninManager.isSignInAllowed()).thenReturn(true);
        signinObserver.onSignedIn();
        // Adapter content:
        // Idx | Item
        // ----|--------------------
        // 0   | Above-the-fold
        // 1   | Header
        // 2   | Suggestion
        // 4   | Footer
        assertEquals(3, mAdapter.getFirstPositionForType(ItemViewType.FOOTER));
        assertEquals(RecyclerView.NO_POSITION,
                mAdapter.getFirstPositionForType(ItemViewType.ALL_DISMISSED));
    }

    /**
     * Robolectric shadow to mock out calls to {@link Resources#getString}.
     */
    @Implements(Resources.class)
    public static class MyShadowResources extends ShadowResources {
        public static final Resources sResources = mock(Resources.class);

        @Implementation
        public CharSequence getText(int id) {
            return sResources.getText(id);
        }
    }

    /**
     * Asserts that the given {@link TreeNode} is a {@link SuggestionsSection} that matches the
     * given {@link SectionDescriptor}.
     * @param descriptor The section descriptor to match against.
     * @param section The section from the adapter.
     */
    private void assertSectionMatches(SectionDescriptor descriptor, SuggestionsSection section) {
        ItemsMatcher matcher = new ItemsMatcher(section);
        matcher.expectSection(descriptor);
        matcher.finish();
    }

    /**
     * Asserts that {@link #mAdapter}.{@link NewTabPageAdapter#getItemCount()} corresponds to an NTP
     * with the given sections in it.
     *
     * @param descriptors A list of descriptors, each describing a section that should be present on
     *                    the UI.
     */
    private void assertItemsFor(SectionDescriptor... descriptors) {
        ItemsMatcher matcher = new ItemsMatcher(mAdapter.getRootForTesting());
        matcher.expectAboveTheFoldItem();
        for (SectionDescriptor descriptor : descriptors) matcher.expectSection(descriptor);
        if (descriptors.length == 0) {
            matcher.expectAllDismissedItem();
        } else {
            matcher.expectFooter();
        }
        matcher.finish();
    }

    /**
     * To be used with {@link #assertItemsFor(SectionDescriptor...)}, for a section with
     * {@code numSuggestions} cards in it.
     * @param suggestions The list of suggestions in the section. If the list is empty, use either
     *         no section at all (if it is not displayed) or {@link #sectionWithStatusCard()}.
     * @return A descriptor for the section.
     */
    private SectionDescriptor section(List<SnippetArticle> suggestions) {
        assert !suggestions.isEmpty();
        return new SectionDescriptor(suggestions);
    }

    private SectionDescriptor signinPromo() {
        return new SectionDescriptor().isSigninPromo();
    }

    /**
     * To be used with {@link #assertItemsFor(SectionDescriptor...)}, for a section that has no
     * suggestions, but a status card to be displayed.
     * @return A descriptor for the section.
     */
    private SectionDescriptor sectionWithStatusCard() {
        return new SectionDescriptor(Collections.emptyList()).withStatusCard();
    }

    /**
     * To be used with {@link #assertItemsFor(SectionDescriptor...)}, for a section that has no
     * suggestions. Should only be used with the modern layout; use {@link #sectionWithStatusCard()}
     * otherwise.
     * @return A descriptor for the section.
     */
    private SectionDescriptor emptySection() {
        assertTrue(false);
        return new SectionDescriptor(Collections.emptyList());
    }

    private void resetUiDelegate() {
        reset(mUiDelegate);
        when(mUiDelegate.getSuggestionsSource()).thenReturn(mSource);
        when(mUiDelegate.getEventReporter()).thenReturn(mock(SuggestionsEventReporter.class));
        when(mUiDelegate.getSuggestionsRanker()).thenReturn(mock(SuggestionsRanker.class));
    }

    private void reloadNtp() {
        mSource.removeObservers();
        mAdapter = new NewTabPageAdapter(mUiDelegate, mock(View.class), /* logoView = */
                makeUiConfig(), mOfflinePageBridge, mock(ContextMenuManager.class)
                /* tileGroupDelegate = */);
        mAdapter.refreshSuggestions();
    }

    private boolean isSignInPromoVisible() {
        return mAdapter.getFirstPositionForType(ItemViewType.PROMO) != RecyclerView.NO_POSITION;
    }

    private int getCategory(RecyclerViewAdapter.Delegate item) {
        return ((SuggestionsSection) item).getCategory();
    }

    /**
     * Note: Currently the observers need to be re-registered to be returned again if this method
     * has been called, as it relies on argument captors that don't repeatedly capture individual
     * calls.
     * @return The currently registered destruction observers.
     */
    private List<DestructionObserver> getDestructionObserver(SuggestionsUiDelegate delegate) {
        ArgumentCaptor<DestructionObserver> observers =
                ArgumentCaptor.forClass(DestructionObserver.class);
        verify(delegate, atLeast(0)).addDestructionObserver(observers.capture());
        return observers.getAllValues();
    }

    @Nullable
    @SuppressWarnings("unchecked")
    private static <T> T findFirstInstanceOf(Collection<?> collection, Class<T> clazz) {
        for (Object item : collection) {
            if (clazz.isAssignableFrom(item.getClass())) return (T) item;
        }
        return null;
    }
}
