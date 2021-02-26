// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.SystemClock;
import android.util.SparseArray;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteMediator.EditSessionState;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderProcessor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for {@link AutocompleteMediator}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AutocompleteMediatorUnitTest {
    private static final int MINIMUM_NUMBER_OF_SUGGESTIONS_TO_SHOW = 5;
    private static final int SUGGESTION_MIN_HEIGHT = 20;
    private static final int HEADER_MIN_HEIGHT = 15;

    /**
     * Empty AutocompleteDelegate implementation for test. This is to work around an issue that
     * mock doesn't work on inherited methods in some builds.
     */
    static class AutocompleteDelegateForTest implements AutocompleteDelegate {
        @Override
        public void setOmniboxEditingText(String text) {}

        @Override
        public void clearOmniboxFocus() {}

        @Override
        public void onUrlTextChanged() {}

        @Override
        public void onSuggestionsChanged(String autocompleteText, boolean defaultMatchIsSearch) {}

        @Override
        public void setKeyboardVisibility(boolean shouldShow, boolean delayHide) {}

        @Override
        public boolean isKeyboardActive() {
            return true;
        }

        @Override
        public void loadUrl(String url, @PageTransition int transition, long inputStart) {}

        @Override
        public void loadUrlWithPostData(String url, @PageTransition int transition, long inputStart,
                String postDataType, byte[] postData) {}

        @Override
        public boolean didFocusUrlFromFakebox() {
            return false;
        }

        @Override
        public boolean isUrlBarFocused() {
            return false;
        }

        @Override
        public boolean didFocusUrlFromQueryTiles() {
            return false;
        }
    }

    /**
     * A variant of Handler, that schedules all delayed tasks for an immediate execution.
     * The handler does not invoke the calls right away; instead, the test method should call
     * `runQueuedTasks()` to flush the queue.
     */
    static class ImmediatePostingHandler extends Handler {
        ImmediatePostingHandler() {
            super(Looper.getMainLooper());
        }

        /**
         * Schedule message for execution.
         * Unlike normal loopers, this will place a supplied Message for immediate execution
         * removing the need to wait for an arbitrary amount of time until all the delayed actions
         * complete.
         *
         * @param msg Message to be scheduled.
         * @param uptimeMillis The absolute time at which the message should be delivered. This
         *         parameter is ignored and replaced with current time.
         */
        @Override
        public boolean sendMessageAtTime(Message msg, long uptimeMillis) {
            return super.sendMessageAtTime(msg, SystemClock.uptimeMillis());
        }

        /**
         * Executes all tasks posted on this Handler and returns.
         * Must be called from UI Thread.
         */
        public void runQueuedTasks() {
            AtomicBoolean ranQueuedTasks = new AtomicBoolean(false);
            post(() -> ranQueuedTasks.set(true));
            CriteriaHelper.pollUiThreadNested(() -> ranQueuedTasks.get(), 100, 0);
        }
    }

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    AutocompleteDelegateForTest mAutocompleteDelegate;

    @Mock
    UrlBarEditingTextStateProvider mTextStateProvider;

    @Mock
    SuggestionProcessor mMockProcessor;

    @Mock
    HeaderProcessor mMockHeaderProcessor;

    @Mock
    AutocompleteController mAutocompleteController;

    @Mock
    LocationBarDataProvider mLocationBarDataProvider;

    @Mock
    ActivityLifecycleDispatcher mLifecycleDispatcher;

    @Mock
    ModalDialogManager mModalDialogManager;

    @Mock
    Profile mProfile;

    private ImmediatePostingHandler mHandler;
    private PropertyModel mListModel;
    private AutocompleteMediator mMediator;
    private List<AutocompleteMatch> mSuggestionsList;
    private ModelList mSuggestionModels;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        mHandler = new ImmediatePostingHandler();

        mSuggestionModels = new ModelList();
        mListModel = new PropertyModel(SuggestionListProperties.ALL_KEYS);
        mListModel.set(SuggestionListProperties.SUGGESTION_MODELS, mSuggestionModels);

        mMediator = new AutocompleteMediator(ContextUtils.getApplicationContext(),
                mAutocompleteDelegate, mTextStateProvider, mAutocompleteController, mListModel,
                mHandler, mLifecycleDispatcher,
                () -> mModalDialogManager, null, null, mLocationBarDataProvider);
        mMediator.getDropdownItemViewInfoListBuilderForTest().registerSuggestionProcessor(
                mMockProcessor);
        mMediator.getDropdownItemViewInfoListBuilderForTest().setHeaderProcessorForTest(
                mMockHeaderProcessor);
        mMediator.setSuggestionVisibilityState(
                AutocompleteMediator.SuggestionVisibilityState.ALLOWED);

        doReturn(SUGGESTION_MIN_HEIGHT).when(mMockProcessor).getMinimumViewHeight();
        doReturn(true).when(mMockProcessor).doesProcessSuggestion(any(), anyInt());
        doAnswer((invocation) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS))
                .when(mMockProcessor)
                .createModel();
        doReturn(OmniboxSuggestionUiType.DEFAULT).when(mMockProcessor).getViewTypeId();

        doAnswer((invocation) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS))
                .when(mMockHeaderProcessor)
                .createModel();
        doReturn(HEADER_MIN_HEIGHT).when(mMockHeaderProcessor).getMinimumViewHeight();
        doReturn(OmniboxSuggestionUiType.HEADER).when(mMockHeaderProcessor).getViewTypeId();

        mSuggestionsList = buildDummySuggestionsList(10, "Suggestion");
        doReturn(true).when(mAutocompleteDelegate).isKeyboardActive();
    }

    /**
     * Build a fake suggestions list with elements named 'Suggestion #', where '#' is the suggestion
     * index (1-based).
     *
     * @return List of suggestions.
     */
    private List<AutocompleteMatch> buildDummySuggestionsList(int count, String prefix) {
        List<AutocompleteMatch> list = new ArrayList<>();
        for (int index = 0; index < count; ++index) {
            list.add(AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                             .setDisplayText(prefix + (index + 1))
                             .build());
        }

        return list;
    }

    /**
     * Build a fake group headers map with elements named 'Header #', where '#' is the group header
     * index (1-based) and 'Header' is the supplied prefix. Each header has a corresponding key
     * computed as baseKey + #.
     *
     * @param count Number of group headers to build.
     * @param baseKey Key of the first group header.
     * @param prefix Name prefix for each group.
     * @return Map of group headers (populated in random order).
     */
    private SparseArray<String> buildDummyGroupHeaders(int count, int baseKey, String prefix) {
        SparseArray<String> headers = new SparseArray<>(count);
        for (int index = 0; index < count; index++) {
            headers.put(baseKey + index, prefix + " " + (index + 1));
        }

        return headers;
    }

    /**
     * Set up LocationBarDataProvider to report supplied values.
     *
     * @param url The URL to report as a current URL.
     * @param title The Page Title to report.
     * @param pageClassification The Page classification to report.
     */
    void setUpLocationBarDataProvider(String url, String title, int pageClassification) {
        when(mLocationBarDataProvider.hasTab()).thenReturn(true);
        when(mLocationBarDataProvider.getProfile()).thenReturn(mProfile);
        when(mLocationBarDataProvider.getCurrentUrl()).thenReturn(url);
        when(mLocationBarDataProvider.getTitle()).thenReturn(title);
        when(mLocationBarDataProvider.getPageClassification(false)).thenReturn(pageClassification);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER)
    public void updateSuggestionsList_notEffectiveWhenDisabled() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 2;

        mMediator.onSuggestionDropdownHeightChanged(maximumListHeight);
        mMediator.onSuggestionsReceived(new AutocompleteResult(mSuggestionsList, null), "");

        Assert.assertEquals(mSuggestionsList.size(), mSuggestionModels.size());
        Assert.assertTrue(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER})
    public void updateSuggestionsList_worksWithNullList() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 7;

        mMediator.onSuggestionDropdownHeightChanged(maximumListHeight);
        mMediator.onSuggestionsReceived(new AutocompleteResult(null, null), "");

        Assert.assertEquals(0, mSuggestionModels.size());
        Assert.assertFalse(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER})
    public void updateSuggestionsList_worksWithEmptyList() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 7;

        mMediator.onSuggestionDropdownHeightChanged(maximumListHeight);
        mMediator.onSuggestionsReceived(new AutocompleteResult(new ArrayList<>(), null), "");

        Assert.assertEquals(0, mSuggestionModels.size());
        Assert.assertFalse(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER})
    public void updateSuggestionsList_scrolEventsWithConcealedItemsTogglesKeyboardVisibility() {
        mMediator.onNativeInitialized();

        final int heightWithOneConcealedItem =
                (mSuggestionsList.size() - 2) * SUGGESTION_MIN_HEIGHT;

        mMediator.onSuggestionDropdownHeightChanged(heightWithOneConcealedItem);
        mMediator.onSuggestionsReceived(new AutocompleteResult(mSuggestionsList, null), "");
        Assert.assertTrue(
                mMediator.getDropdownItemViewInfoListBuilderForTest().hasFullyConcealedElements());

        // With fully concealed elements, scroll should trigger keyboard hide.
        reset(mAutocompleteDelegate);
        mMediator.onSuggestionDropdownScroll();
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(false), anyBoolean());
        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(true), anyBoolean());

        // Pretend that the user scrolled back to top with an overscroll.
        // This should bring back the soft keyboard.
        reset(mAutocompleteDelegate);
        mMediator.onSuggestionDropdownOverscrolledToTop();
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(true), anyBoolean());
        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(false), anyBoolean());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER})
    public void updateSuggestionsList_updateHeightWhenHardwareKeyboardIsConnected() {
        // Simulates behavior of physical keyboard being attached to the device.
        // In this scenario, requesting keyboard to come up will not result with an actual
        // keyboard showing on the screen. As a result, the updated height should be used
        // when estimating presence of fully concealed items on the suggestions list.
        //
        // Attaching and detaching physical keyboard will affect the space on the screen, but since
        // the list of suggestions does not change, we are keeping them in exactly the same order
        // (and keep the grouping prior to the change).
        // The grouping is only affected, when the new list is provided (as a result of user's
        // input).
        mMediator.onNativeInitialized();

        final int heightOfOAllSuggestions = mSuggestionsList.size() * SUGGESTION_MIN_HEIGHT;
        final int heightWithOneConcealedItem =
                (mSuggestionsList.size() - 1) * SUGGESTION_MIN_HEIGHT;

        // This will request keyboard to show up upon receiving next suggestions list.
        when(mAutocompleteDelegate.isKeyboardActive()).thenReturn(true);
        // Report the height of the suggestions list, indicating that the keyboard is not visible.
        // In both cases, the updated suggestions list height should be used to estimate presence of
        // fully concealed items on the suggestions list.
        mMediator.onSuggestionDropdownHeightChanged(heightOfOAllSuggestions);
        mMediator.onSuggestionsReceived(new AutocompleteResult(mSuggestionsList, null), "");
        Assert.assertFalse(
                mMediator.getDropdownItemViewInfoListBuilderForTest().hasFullyConcealedElements());

        // Build separate list of suggestions so that these are accepted as a new set.
        // We want to follow the same restrictions as the original list (specifically: have a
        // resulting list of suggestions taller than the space in dropdown view), so make sure
        // the list sizes are same.
        List<AutocompleteMatch> newList =
                buildDummySuggestionsList(mSuggestionsList.size(), "SuggestionB");
        mMediator.onSuggestionDropdownHeightChanged(heightWithOneConcealedItem);
        mMediator.onSuggestionsReceived(new AutocompleteResult(newList, null), "");
        Assert.assertTrue(
                mMediator.getDropdownItemViewInfoListBuilderForTest().hasFullyConcealedElements());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER})
    public void updateSuggestionsList_rejectsHeightUpdatesWhenKeyboardIsHidden() {
        // Simulates scenario where we receive dropdown height update after software keyboard is
        // explicitly hidden. In this scenario the updates should be rejected when estimating
        // presence of fully concealed items on the suggestions list.
        mMediator.onNativeInitialized();

        final int heightOfOAllSuggestions = mSuggestionsList.size() * SUGGESTION_MIN_HEIGHT;
        final int heightWithOneConcealedItem =
                (mSuggestionsList.size() - 1) * SUGGESTION_MIN_HEIGHT;

        // Report height change with keyboard visible
        mMediator.onSuggestionDropdownHeightChanged(heightWithOneConcealedItem);
        mMediator.onSuggestionsReceived(new AutocompleteResult(mSuggestionsList, null), "");
        Assert.assertTrue(
                mMediator.getDropdownItemViewInfoListBuilderForTest().hasFullyConcealedElements());

        // "Hide keyboard", report larger area and re-evaluate the results. We should see no
        // difference, as the logic should only evaluate presence of items concealed when keyboard
        // is active.
        when(mAutocompleteDelegate.isKeyboardActive()).thenReturn(false);
        mMediator.onSuggestionDropdownHeightChanged(heightOfOAllSuggestions);
        mMediator.onSuggestionsReceived(new AutocompleteResult(mSuggestionsList, null), "");
        Assert.assertTrue(
                mMediator.getDropdownItemViewInfoListBuilderForTest().hasFullyConcealedElements());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER)
    public void onTextChanged_emptyTextTriggersZeroSuggest() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        mMediator.onNativeInitialized();
        mMediator.onTextChanged("", "");
        verify(mAutocompleteController)
                .startZeroSuggest(mProfile, "", url, pageClassification, title);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER)
    public void onTextChanged_nonEmptyTextTriggersSuggestions() {
        String url = "http://www.example.com";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, url, pageClassification);

        when(mTextStateProvider.shouldAutocomplete()).thenReturn(true);
        when(mTextStateProvider.getSelectionStart()).thenReturn(4);
        when(mTextStateProvider.getSelectionEnd()).thenReturn(4);

        mMediator.onNativeInitialized();
        mMediator.onTextChanged("test", "testing");
        mHandler.runQueuedTasks();
        verify(mAutocompleteController)
                .start(mProfile, url, pageClassification, "test", 4, false, null, false);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER)
    public void onTextChanged_cancelsPendingRequests() {
        String url = "http://www.example.com";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, url, pageClassification);

        when(mTextStateProvider.shouldAutocomplete()).thenReturn(true);
        when(mTextStateProvider.getSelectionStart()).thenReturn(4);
        when(mTextStateProvider.getSelectionEnd()).thenReturn(4);

        mMediator.onNativeInitialized();
        mMediator.onTextChanged("test", "testing");
        mMediator.onTextChanged("nottest", "nottesting");
        mHandler.runQueuedTasks();
        verify(mAutocompleteController, times(1))
                .start(mProfile, url, pageClassification, "nottest", 4, false, null, false);
        verify(mAutocompleteController, times(1))
                .start(any(), any(), anyInt(), any(), anyInt(), anyBoolean(), any(), anyBoolean());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER)
    public void onUrlFocusChange_onlyOneZeroSuggestRequestIsInvoked() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        // Simulate URL being focus changes.
        mMediator.onUrlFocusChange(true);
        mMediator.onUrlFocusChange(false);
        mMediator.onUrlFocusChange(true);
        mHandler.runQueuedTasks();
        verify(mAutocompleteController, never())
                .startZeroSuggest(any(), any(), any(), anyInt(), any());

        // Simulate native being inititalized. Make sure we only ever issue one request.
        mMediator.onNativeInitialized();
        mHandler.runQueuedTasks();
        verify(mAutocompleteController, times(1))
                .startZeroSuggest(mProfile, "", url, pageClassification, title);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER)
    public void onUrlFocusChange_preventsZeroSuggestRequestOnFocusLost() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        // Simulate URL being focus changes.
        mMediator.onUrlFocusChange(true);
        mMediator.onUrlFocusChange(false);
        mHandler.runQueuedTasks();
        verify(mAutocompleteController, never())
                .startZeroSuggest(any(), any(), any(), anyInt(), any());

        // Simulate native being inititalized. Make sure no suggest requests are sent.
        mMediator.onNativeInitialized();
        mHandler.runQueuedTasks();
        verify(mAutocompleteController, never())
                .startZeroSuggest(any(), any(), any(), anyInt(), any());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER)
    public void onUrlFocusChange_textChangeCancelsOustandingZeroSuggestRequest() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("A");

        // Simulate URL being focus changes, and that user typed text and deleted it.
        mMediator.onUrlFocusChange(true);
        mMediator.onTextChanged("A", "Abc");
        mMediator.onTextChanged("", "");
        mMediator.onTextChanged("A", "Abc");

        mHandler.runQueuedTasks();
        verify(mAutocompleteController, never())
                .start(any(), any(), anyInt(), any(), anyInt(), anyBoolean(), any(), anyBoolean());
        verify(mAutocompleteController, never())
                .startZeroSuggest(any(), any(), any(), anyInt(), any());

        mMediator.onNativeInitialized();
        mHandler.runQueuedTasks();
        verify(mAutocompleteController, times(1))
                .start(mProfile, url, pageClassification, "A", 0, true, null, false);
        verify(mAutocompleteController, times(1))
                .start(any(), any(), anyInt(), any(), anyInt(), anyBoolean(), any(), anyBoolean());
        verify(mAutocompleteController, never())
                .startZeroSuggest(any(), any(), any(), anyInt(), any());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER)
    public void onUrlFocusChange_textChangeCancelsIntermediateZeroSuggestRequests() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        // Simulate URL being focus changes, and that user typed text and deleted it.
        mMediator.onTextChanged("A", "Abc");
        mMediator.onTextChanged("", "");

        mHandler.runQueuedTasks();
        verify(mAutocompleteController, never())
                .start(any(), any(), anyInt(), any(), anyInt(), anyBoolean(), any(), anyBoolean());
        verify(mAutocompleteController, never())
                .startZeroSuggest(any(), any(), any(), anyInt(), any());

        mMediator.onNativeInitialized();
        mHandler.runQueuedTasks();
        verify(mAutocompleteController, never())
                .start(any(), any(), anyInt(), any(), anyInt(), anyBoolean(), any(), anyBoolean());
        verify(mAutocompleteController, times(1))
                .startZeroSuggest(any(), any(), any(), anyInt(), any());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER)
    public void onSuggestionsReceived_sendsOnSuggestionsChanged() {
        mMediator.onNativeInitialized();
        mMediator.onSuggestionsReceived(
                new AutocompleteResult(mSuggestionsList, null), "inline_autocomplete");
        verify(mAutocompleteDelegate).onSuggestionsChanged("inline_autocomplete", true);

        // Ensure duplicate requests are suppressed.
        mMediator.onSuggestionsReceived(
                new AutocompleteResult(mSuggestionsList, null), "inline_autocomplete2");
        verifyNoMoreInteractions(mAutocompleteDelegate);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER)
    public void setLayoutDirection_beforeInitialization() {
        mMediator.onNativeInitialized();
        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        mMediator.onSuggestionDropdownHeightChanged(Integer.MAX_VALUE);
        mMediator.onSuggestionsReceived(new AutocompleteResult(mSuggestionsList, null), "");
        Assert.assertEquals(mSuggestionsList.size(), mSuggestionModels.size());
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            Assert.assertEquals(i + "th model does not have the expected layout direction.",
                    View.LAYOUT_DIRECTION_RTL,
                    mSuggestionModels.get(i).model.get(
                            SuggestionCommonProperties.LAYOUT_DIRECTION));
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER)
    public void setLayoutDirection_afterInitialization() {
        mMediator.onNativeInitialized();
        mMediator.onSuggestionDropdownHeightChanged(Integer.MAX_VALUE);
        mMediator.onSuggestionsReceived(new AutocompleteResult(mSuggestionsList, null), "");
        Assert.assertEquals(mSuggestionsList.size(), mSuggestionModels.size());

        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            Assert.assertEquals(i + "th model does not have the expected layout direction.",
                    View.LAYOUT_DIRECTION_RTL,
                    mSuggestionModels.get(i).model.get(
                            SuggestionCommonProperties.LAYOUT_DIRECTION));
        }

        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_LTR);
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            Assert.assertEquals(i + "th model does not have the expected layout direction.",
                    View.LAYOUT_DIRECTION_LTR,
                    mSuggestionModels.get(i).model.get(
                            SuggestionCommonProperties.LAYOUT_DIRECTION));
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER)
    public void onUrlFocusChange_triggersZeroSuggest_nativeInitialized() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn(url);

        mMediator.onNativeInitialized();
        mMediator.onUrlFocusChange(true);
        mHandler.runQueuedTasks();
        verify(mAutocompleteController)
                .startZeroSuggest(mProfile, url, url, pageClassification, title);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER)
    public void onUrlFocusChange_triggersZeroSuggest_nativeNotInitialized() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = PageClassification.BLANK_VALUE;
        setUpLocationBarDataProvider(url, title, pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        // Signal focus prior to initializing native; confirm that zero suggest is not triggered.
        mMediator.onUrlFocusChange(true);
        mHandler.runQueuedTasks();
        verify(mAutocompleteController, never())
                .startZeroSuggest(any(), any(), any(), anyInt(), any());

        // Initialize native and ensure zero suggest is triggered.
        mMediator.onNativeInitialized();
        mHandler.runQueuedTasks();
        verify(mAutocompleteController)
                .startZeroSuggest(mProfile, "", url, pageClassification, title);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER)
    public void onQueryTilesSelected_thenTextChanged_editSessionActivatedByQueryTile() {
        mMediator.onNativeInitialized();
        QueryTile childTile = new QueryTile("sports", "sports", "sports", "sports",
                new String[] {"http://foo/sports.jpg"}, null /* searchParams */,
                null /* children */);
        QueryTile tile =
                new QueryTile("news", "news", "news", "news", new String[] {"http://foo/news.jpg"},
                        null /* searchParams */, Arrays.asList(childTile));
        mMediator.onUrlFocusChange(true);
        Assert.assertEquals(mMediator.getEditSessionStateForTest(), EditSessionState.INACTIVE);

        mMediator.onQueryTileSelected(tile);
        Assert.assertEquals(
                mMediator.getEditSessionStateForTest(), EditSessionState.ACTIVATED_BY_QUERY_TILE);
        verify(mAutocompleteDelegate).setOmniboxEditingText("news ");
        mMediator.onTextChanged("news s", "news sports");
        Assert.assertEquals(
                mMediator.getEditSessionStateForTest(), EditSessionState.ACTIVATED_BY_QUERY_TILE);

        mMediator.onUrlFocusChange(false);
        Assert.assertEquals(mMediator.getEditSessionStateForTest(), EditSessionState.INACTIVE);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @EnableFeatures(ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER)
    public void onTextChanged_editSessionActivatedByUserInput() {
        mMediator.onNativeInitialized();
        mMediator.onUrlFocusChange(true);
        Assert.assertEquals(mMediator.getEditSessionStateForTest(), EditSessionState.INACTIVE);
        mMediator.onTextChanged("n", "news");
        Assert.assertEquals(
                mMediator.getEditSessionStateForTest(), EditSessionState.ACTIVATED_BY_USER_INPUT);

        mMediator.onUrlFocusChange(false);
        Assert.assertEquals(mMediator.getEditSessionStateForTest(), EditSessionState.INACTIVE);
    }
}
