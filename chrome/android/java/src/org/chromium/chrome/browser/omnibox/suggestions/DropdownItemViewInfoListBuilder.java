// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.answer.AnswerSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.clipboard.ClipboardSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.editurl.EditUrlSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.entity.EntitySuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.mostvisited.MostVisitedTilesProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.tail.TailSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.tiles.TileSuggestionProcessor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.AutocompleteResult.GroupDetails;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/** Builds DropdownItemViewInfo list from AutocompleteResult for the Suggestions list. */
class DropdownItemViewInfoListBuilder {
    private static final int MAX_IMAGE_CACHE_SIZE = 500 * ConversionUtils.BYTES_PER_KILOBYTE;
    @Px
    private static final int DROPDOWN_HEIGHT_UNKNOWN = -1;
    private static final int DEFAULT_SIZE_OF_VISIBLE_GROUP = 5;

    private final @NonNull List<SuggestionProcessor> mPriorityOrderedSuggestionProcessors;
    private @NonNull AutocompleteController mAutocompleteController;

    private @Nullable HeaderProcessor mHeaderProcessor;
    private @Nullable ActivityTabProvider mActivityTabProvider;
    private @Nullable Supplier<ShareDelegate> mShareDelegateSupplier;
    private @Nullable ImageFetcher mImageFetcher;
    private @Nullable LargeIconBridge mIconBridge;
    private @Nullable BookmarkBridge mBookmarkBridge;
    @Px
    private int mDropdownHeight;
    private boolean mEnableAdaptiveSuggestionsCount;
    private boolean mBuiltListHasFullyConcealedElements;

    DropdownItemViewInfoListBuilder(AutocompleteController controller) {
        mPriorityOrderedSuggestionProcessors = new ArrayList<>();
        mDropdownHeight = DROPDOWN_HEIGHT_UNKNOWN;
        mAutocompleteController = controller;
    }

    /**
     * Initialize the Builder with default set of suggestion processors.
     *
     * @param context Current context.
     * @param host Component creating suggestion view delegates and responding to suggestion events.
     * @param delegate Component facilitating interactions with UI and Autocomplete mechanism.
     * @param textProvider Provider of querying/editing the Omnibox.
     * @param queryTileSuggestionCallback Callback responding to QueryTile events.
     */
    void initDefaultProcessors(Context context, SuggestionHost host, AutocompleteDelegate delegate,
            UrlBarEditingTextStateProvider textProvider,
            Callback<List<QueryTile>> queryTileSuggestionCallback) {
        assert mPriorityOrderedSuggestionProcessors.size() == 0 : "Processors already initialized.";

        final Supplier<ImageFetcher> imageFetcherSupplier = () -> mImageFetcher;
        final Supplier<LargeIconBridge> iconBridgeSupplier = () -> mIconBridge;
        final Supplier<Tab> tabSupplier =
                () -> mActivityTabProvider == null ? null : mActivityTabProvider.get();
        final Supplier<ShareDelegate> shareSupplier =
                () -> mShareDelegateSupplier == null ? null : mShareDelegateSupplier.get();
        final Supplier<BookmarkBridge> bookmarkSupplier = () -> mBookmarkBridge;

        mHeaderProcessor = new HeaderProcessor(context, host, delegate);
        registerSuggestionProcessor(new EditUrlSuggestionProcessor(
                context, host, delegate, iconBridgeSupplier, tabSupplier, shareSupplier));
        registerSuggestionProcessor(
                new AnswerSuggestionProcessor(context, host, textProvider, imageFetcherSupplier));
        registerSuggestionProcessor(
                new ClipboardSuggestionProcessor(context, host, iconBridgeSupplier));
        registerSuggestionProcessor(
                new EntitySuggestionProcessor(context, host, imageFetcherSupplier));
        registerSuggestionProcessor(new TailSuggestionProcessor(context, host));
        registerSuggestionProcessor(
                new TileSuggestionProcessor(context, queryTileSuggestionCallback));
        registerSuggestionProcessor(
                new MostVisitedTilesProcessor(context, host, iconBridgeSupplier));
        registerSuggestionProcessor(new BasicSuggestionProcessor(
                context, host, textProvider, iconBridgeSupplier, bookmarkSupplier));
    }

    void destroy() {
        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }

        if (mIconBridge != null) {
            mIconBridge.destroy();
            mIconBridge = null;
        }
    }

    /**
     * Register new processor to process OmniboxSuggestions.
     * Processors will be tried in the same order as they were added.
     *
     * @param processor SuggestionProcessor that handles OmniboxSuggestions.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void registerSuggestionProcessor(SuggestionProcessor processor) {
        mPriorityOrderedSuggestionProcessors.add(processor);
    }

    /**
     * Specify instance of the HeaderProcessor that will be used to run tests.
     *
     * @param processor Header processor used to build suggestion headers.
     */
    void setHeaderProcessorForTest(HeaderProcessor processor) {
        mHeaderProcessor = processor;
    }

    /**
     * Notify that the current User profile has changed.
     *
     * @param profile Current user profile.
     */
    void setProfile(Profile profile) {
        if (mIconBridge != null) {
            mIconBridge.destroy();
            mIconBridge = null;
        }

        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }

        if (mBookmarkBridge != null) {
            mBookmarkBridge.destroy();
            mBookmarkBridge = null;
        }

        mIconBridge = new LargeIconBridge(profile);
        mImageFetcher = ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.IN_MEMORY_ONLY,
                profile, GlobalDiscardableReferencePool.getReferencePool(), MAX_IMAGE_CACHE_SIZE);
        mBookmarkBridge = new BookmarkBridge(profile);
    }

    /**
     * Notify that the current Share delegate supplier has changed.
     *
     * @param shareDelegateSupplier Share facility supplier.
     */
    void setShareDelegateSupplier(Supplier<ShareDelegate> shareDelegateSupplier) {
        mShareDelegateSupplier = shareDelegateSupplier;
    }

    /**
     * Specify new Activity tab provider.
     *
     * @param provider Tab provider.
     */
    void setActivityTabProvider(ActivityTabProvider provider) {
        mActivityTabProvider = provider;
    }

    /**
     * Specify dropdown list height in pixels.
     * The height is subsequentially used to determine number of visible suggestions and perform
     * partial suggestion ordering based on their visibility.
     *
     * Note that this mechanism is effective as long as grouping is not in use in zero-prefix
     * context. At the time this mechanism was created, zero-prefix context never presented mixed
     * URL and (non-reactive) search suggestions, but instead presented either a list of specialized
     * suggestions (eg. clipboard, query tiles) mixed with reactive suggestions, a plain list of
     * search suggestions, or a plain list of recent URLs.
     * This gives us the chance to measure the height of the dropdown list before the actual
     * grouping takes effect.
     * If the above situation changes, we may need to revisit the logic here, and possibly cache the
     * heights in different states (eg. portrait mode, split screen etc) to get better results.
     *
     * @param dropdownHeight Updated height of the dropdown item list.
     */
    void setDropdownHeightWithKeyboardActive(@Px int dropdownHeight) {
        mDropdownHeight = dropdownHeight;
    }

    /**
     * Respond to URL bar focus change.
     *
     * @param hasFocus Indicates whether URL bar is now focused.
     */
    void onUrlFocusChange(boolean hasFocus) {
        if (!hasFocus && mImageFetcher != null) {
            mImageFetcher.clear();
        }

        if (!hasFocus) {
            mBuiltListHasFullyConcealedElements = false;
        }

        mHeaderProcessor.onUrlFocusChange(hasFocus);
        for (int index = 0; index < mPriorityOrderedSuggestionProcessors.size(); index++) {
            mPriorityOrderedSuggestionProcessors.get(index).onUrlFocusChange(hasFocus);
        }
    }

    /** Signals that native initialization has completed. */
    void onNativeInitialized() {
        mEnableAdaptiveSuggestionsCount =
                ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT);

        mHeaderProcessor.onNativeInitialized();
        for (int index = 0; index < mPriorityOrderedSuggestionProcessors.size(); index++) {
            mPriorityOrderedSuggestionProcessors.get(index).onNativeInitialized();
        }
    }

    /**
     * Build ListModel for new set of Omnibox suggestions.
     *
     * @param autocompleteResult New set of suggestions.
     * @return List of DropdownItemViewInfo representing the corresponding content of the
     *          suggestions list.
     */
    @NonNull
    List<DropdownItemViewInfo> buildDropdownViewInfoList(AutocompleteResult autocompleteResult) {
        mHeaderProcessor.onSuggestionsReceived();
        for (int index = 0; index < mPriorityOrderedSuggestionProcessors.size(); index++) {
            mPriorityOrderedSuggestionProcessors.get(index).onSuggestionsReceived();
        }

        final List<AutocompleteMatch> newSuggestions = autocompleteResult.getSuggestionsList();
        final int newSuggestionsCount = newSuggestions.size();
        final List<DropdownItemViewInfo> viewInfoList = new ArrayList<>();

        // Match suggestions with their corresponding processors.
        final List<Pair<AutocompleteMatch, SuggestionProcessor>> suggestionsPairedWithProcessors =
                new ArrayList<>();
        for (int index = 0; index < newSuggestionsCount; index++) {
            final AutocompleteMatch suggestion = newSuggestions.get(index);
            final SuggestionProcessor processor = getProcessorForSuggestion(suggestion, index);
            suggestionsPairedWithProcessors.add(new Pair<>(suggestion, processor));
        }

        // When Adaptive Suggestions are set, perform partial grouping by search vs url.
        if (mEnableAdaptiveSuggestionsCount) {
            int numVisibleSuggestions = getVisibleSuggestionsCount(suggestionsPairedWithProcessors);
            mBuiltListHasFullyConcealedElements =
                    (numVisibleSuggestions < suggestionsPairedWithProcessors.size());
            // TODO(crbug.com/1073169): this should either infer the count from UI height or supply
            // the default value if height is not known. For the time being we group the entire list
            // to mimic the native behavior.
            groupSuggestionsBySearchVsURL(suggestionsPairedWithProcessors, numVisibleSuggestions);
        }

        // Build ViewInfo structures.
        int currentGroup = AutocompleteMatch.INVALID_GROUP;
        for (int index = 0; index < newSuggestionsCount; index++) {
            final Pair<AutocompleteMatch, SuggestionProcessor> suggestionAndProcessorPair =
                    suggestionsPairedWithProcessors.get(index);
            final AutocompleteMatch suggestion = suggestionAndProcessorPair.first;
            final SuggestionProcessor processor = suggestionAndProcessorPair.second;

            if (currentGroup != suggestion.getGroupId()) {
                currentGroup = suggestion.getGroupId();
                final GroupDetails details =
                        autocompleteResult.getGroupsDetails().get(currentGroup);

                // Only add the Header Group when both ID and details are specified.
                // Note that despite GroupsDetails map not holding <null> values,
                // a group definition for specific ID may be unavailable.
                if (details != null) {
                    final PropertyModel model = mHeaderProcessor.createModel();
                    mHeaderProcessor.populateModel(model, currentGroup, details.title);
                    viewInfoList.add(
                            new DropdownItemViewInfo(mHeaderProcessor, model, currentGroup));
                }
            }

            final PropertyModel model = processor.createModel();
            processor.populateModel(suggestion, model, index);
            viewInfoList.add(new DropdownItemViewInfo(processor, model, currentGroup));
        }
        return viewInfoList;
    }

    /**
     * @param suggestionsPairedWithProcessors List of suggestions and their matching processors.
     * @return Number of suggestions immediately visible to the user upon presenting the list.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    int getVisibleSuggestionsCount(
            List<Pair<AutocompleteMatch, SuggestionProcessor>> suggestionsPairedWithProcessors) {
        // For cases where we don't know how many suggestions can fit in the visile screen area,
        // make an assumption regarding the group size.
        if (mDropdownHeight == DROPDOWN_HEIGHT_UNKNOWN) {
            return Math.min(suggestionsPairedWithProcessors.size(), DEFAULT_SIZE_OF_VISIBLE_GROUP);
        }

        @Px
        int calculatedSuggestionsHeight = 0;
        int currentGroup = AutocompleteMatch.INVALID_GROUP;
        int lastVisibleIndex;
        for (lastVisibleIndex = 0; lastVisibleIndex < suggestionsPairedWithProcessors.size();
                lastVisibleIndex++) {
            if (calculatedSuggestionsHeight >= mDropdownHeight) break;

            final Pair<AutocompleteMatch, SuggestionProcessor> pair =
                    suggestionsPairedWithProcessors.get(lastVisibleIndex);
            final AutocompleteMatch suggestion = pair.first;

            // Include the height of the group header view.
            if (currentGroup != suggestion.getGroupId()) {
                currentGroup = suggestion.getGroupId();
                calculatedSuggestionsHeight += mHeaderProcessor.getMinimumViewHeight();
            }

            // Include the height of the suggestion view.
            final SuggestionProcessor processor = pair.second;
            calculatedSuggestionsHeight += processor.getMinimumViewHeight();
        }

        return lastVisibleIndex;
    }

    /** @return Whether built list contains fully concealed elements. */
    boolean hasFullyConcealedElements() {
        return mBuiltListHasFullyConcealedElements;
    }

    /**
     * Group suggestions in-place by Search vs URL.
     * Creates two subgroups:
     * - Group 1 contains items visible, or partially visible to the user,
     * - Group 2 contains items that are not visible at the time user interacts with the
     * suggestions list.
     *
     * @param suggestionsPairedWithProcessors List of suggestions and their matching processors.
     * @param numVisibleSuggestions Number of suggestions that are visible to the user.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void groupSuggestionsBySearchVsURL(
            List<Pair<AutocompleteMatch, SuggestionProcessor>> suggestionsPairedWithProcessors,
            int numVisibleSuggestions) {
        final int firstIndexWithHeader = findFirstIndexWithHeader(suggestionsPairedWithProcessors);
        final int firstIndexForGrouping =
                findFirstIndexForGrouping(suggestionsPairedWithProcessors);

        // Check if we have any suggestions we can group.
        if (firstIndexWithHeader <= firstIndexForGrouping) return;

        // Compute the index of first concealed element as a start of the second group.
        // This addresses the situation, where all visible and some concealed suggestions are
        // specialized (eg. visible default match, query tiles and concealed clipboard suggestion).
        final int firstIndexInConcealedGroup = Math.max(
                Math.min(numVisibleSuggestions, firstIndexWithHeader), firstIndexForGrouping);

        // Comparator addressing the suggestion grouping.
        final Comparator<Pair<AutocompleteMatch, SuggestionProcessor>> comparator =
                (pair1, pair2) -> {
            if (pair1.first.isSearchSuggestion() != pair2.first.isSearchSuggestion()) {
                return pair1.first.isSearchSuggestion() ? -1 : 1;
            }
            return pair2.first.getRelevance() - pair1.first.getRelevance();
        };

        // Sort visible part of suggestions list.
        if (firstIndexForGrouping < firstIndexInConcealedGroup) {
            Collections.sort(suggestionsPairedWithProcessors.subList(
                                     firstIndexForGrouping, firstIndexInConcealedGroup),
                    comparator);
            mAutocompleteController.groupSuggestionsBySearchVsURL(
                    firstIndexForGrouping, firstIndexInConcealedGroup);
        }

        // Sort the concealed part of suggestions list.
        if (firstIndexInConcealedGroup < firstIndexWithHeader) {
            Collections.sort(suggestionsPairedWithProcessors.subList(
                                     firstIndexInConcealedGroup, firstIndexWithHeader),
                    comparator);
            mAutocompleteController.groupSuggestionsBySearchVsURL(
                    firstIndexInConcealedGroup, firstIndexWithHeader);
        }
    }

    /** @return Index of the first suggestion decorated with a suggestion header. */
    private int findFirstIndexWithHeader(
            List<Pair<AutocompleteMatch, SuggestionProcessor>> suggestionsPairedWithProcessors) {
        // Native counterpart ensures that suggestion with group headers always end up at the
        // end of the list. This guarantees that these suggestions are both grouped at the end
        // of the list and that there's nothing more we should do about them. See
        // AutocompleteController::UpdateHeaders().
        int firstIndexWithHeader;
        for (firstIndexWithHeader = 0;
                firstIndexWithHeader < suggestionsPairedWithProcessors.size();
                firstIndexWithHeader++) {
            if (suggestionsPairedWithProcessors.get(firstIndexWithHeader).first.getGroupId()
                    != AutocompleteMatch.INVALID_GROUP) {
                break;
            }
        }
        return firstIndexWithHeader;
    }

    /**
     * @return Index of the first element that should be used to group suggestions by
     *         search vs URL.
     */
    private int findFirstIndexForGrouping(
            List<Pair<AutocompleteMatch, SuggestionProcessor>> suggestionsPairedWithProcessors) {
        int firstIndexForGrouping;
        // Find the first suggestion that will be the subject for grouping by search vs url.
        // Note that the first suggestion is the default match and we never change it.
        for (firstIndexForGrouping = 1;
                firstIndexForGrouping < suggestionsPairedWithProcessors.size();
                firstIndexForGrouping++) {
            final @OmniboxSuggestionType int type =
                    suggestionsPairedWithProcessors.get(firstIndexForGrouping).first.getType();

            if (type != OmniboxSuggestionType.TILE_SUGGESTION
                    && type != OmniboxSuggestionType.CLIPBOARD_TEXT
                    && type != OmniboxSuggestionType.CLIPBOARD_URL
                    && type != OmniboxSuggestionType.CLIPBOARD_IMAGE) {
                break;
            }
        }
        return firstIndexForGrouping;
    }

    /**
     * Search for Processor that will handle the supplied suggestion at specific position.
     *
     * @param suggestion The suggestion to be processed.
     * @param position Position of the suggestion in the list.
     */
    private SuggestionProcessor getProcessorForSuggestion(
            AutocompleteMatch suggestion, int position) {
        for (int index = 0; index < mPriorityOrderedSuggestionProcessors.size(); index++) {
            SuggestionProcessor processor = mPriorityOrderedSuggestionProcessors.get(index);
            if (processor.doesProcessSuggestion(suggestion, position)) return processor;
        }
        assert false : "No default handler for suggestions";
        return null;
    }

    /**
     * Change the AutocompleteController instance that will be used by this class.
     *
     * @param controller New AutocompleteController to use.
     */
    void setAutocompleteControllerForTest(@NonNull AutocompleteController controller) {
        mAutocompleteController = controller;
    }
}
