// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.os.Handler;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionListViewBinder.SuggestionListViewHolder;
import org.chromium.chrome.browser.omnibox.suggestions.answer.AnswerSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.editurl.EditUrlSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.editurl.EditUrlSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.entity.EntitySuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderView;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderViewBinder;
import org.chromium.chrome.browser.omnibox.suggestions.tail.TailSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.tail.TailSuggestionViewBinder;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.ui.ViewProvider;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Coordinator that handles the interactions with the autocomplete system.
 */
public class AutocompleteCoordinatorImpl implements AutocompleteCoordinator {
    private final ViewGroup mParent;
    private OmniboxQueryTileCoordinator mQueryTileCoordinator;
    private AutocompleteMediator mMediator;

    private OmniboxSuggestionsDropdown mDropdown;

    /**
     * See {@link AutocompleteCoordinatorFactory#createAutocompleteCoordinator}.
     *
     * Keep this constructor protected so clients use the factory instead.
     */
    @VisibleForTesting
    protected AutocompleteCoordinatorImpl(ViewGroup parent, AutocompleteDelegate delegate,
            OmniboxSuggestionsDropdown.Embedder dropdownEmbedder,
            UrlBarEditingTextStateProvider urlBarEditingTextProvider) {
        mParent = parent;
        Context context = parent.getContext();

        PropertyModel listModel = new PropertyModel(SuggestionListProperties.ALL_KEYS);
        ModelList listItems = new ModelList();

        listModel.set(SuggestionListProperties.EMBEDDER, dropdownEmbedder);
        listModel.set(SuggestionListProperties.VISIBLE, false);
        listModel.set(SuggestionListProperties.SUGGESTION_MODELS, listItems);

        mQueryTileCoordinator = new OmniboxQueryTileCoordinator(context, this::onTileSelected);
        mMediator = new AutocompleteMediator(context, delegate, urlBarEditingTextProvider,
                new AutocompleteController(), listModel, new Handler());
        mMediator.initDefaultProcessors(mQueryTileCoordinator::setTiles);

        listModel.set(SuggestionListProperties.OBSERVER, mMediator);

        ViewProvider<SuggestionListViewHolder> viewProvider =
                createViewProvider(context, listItems);
        viewProvider.whenLoaded((holder) -> { mDropdown = holder.dropdown; });
        LazyConstructionPropertyMcp.create(listModel, SuggestionListProperties.VISIBLE,
                viewProvider, SuggestionListViewBinder::bind);

        // https://crbug.com/966227 Set initial layout direction ahead of inflating the suggestions.
        updateSuggestionListLayoutDirection();
    }

    @Override
    public void destroy() {
        mQueryTileCoordinator.destroy();
        mQueryTileCoordinator = null;
        mMediator.destroy();
        mMediator = null;
    }

    private ViewProvider<SuggestionListViewHolder> createViewProvider(
            Context context, MVCListAdapter.ModelList modelList) {
        return new ViewProvider<SuggestionListViewHolder>() {
            private List<Callback<SuggestionListViewHolder>> mCallbacks = new ArrayList<>();
            private SuggestionListViewHolder mHolder;

            @Override
            public void inflate() {
                ViewGroup container = (ViewGroup) ((ViewStub) mParent.getRootView().findViewById(
                                                           R.id.omnibox_results_container_stub))
                                              .inflate();
                Pair<OmniboxSuggestionsDropdown, MVCListAdapter> dropdownAndAdapter =
                        OmniboxSuggestionsDropdownFactory.provideDropdownAndAdapter(
                                context, modelList);

                OmniboxSuggestionsDropdown dropdown = dropdownAndAdapter.first;
                MVCListAdapter adapter = dropdownAndAdapter.second;

                // Start with visibility GONE to ensure that show() is called.
                // http://crbug.com/517438
                dropdown.getViewGroup().setVisibility(View.GONE);
                dropdown.getViewGroup().setClipToPadding(false);

                // Register a view type for a default omnibox suggestion.
                // Note: clang-format does a bad job formatting lambdas so we turn it off here.
                // clang-format off
                adapter.registerType(
                        OmniboxSuggestionUiType.DEFAULT,
                        parent -> new BaseSuggestionView<View>(
                                parent.getContext(), R.layout.omnibox_basic_suggestion),
                        new BaseSuggestionViewBinder<View>(SuggestionViewViewBinder::bind));

                adapter.registerType(
                        OmniboxSuggestionUiType.EDIT_URL_SUGGESTION,
                        parent -> new EditUrlSuggestionView(parent.getContext()),
                        new EditUrlSuggestionViewBinder());

                adapter.registerType(
                        OmniboxSuggestionUiType.ANSWER_SUGGESTION,
                        parent -> new BaseSuggestionView<View>(
                                parent.getContext(), R.layout.omnibox_answer_suggestion),
                        new BaseSuggestionViewBinder<View>(AnswerSuggestionViewBinder::bind));

                adapter.registerType(
                        OmniboxSuggestionUiType.ENTITY_SUGGESTION,
                        parent -> new BaseSuggestionView<View>(
                                parent.getContext(), R.layout.omnibox_entity_suggestion),
                        new BaseSuggestionViewBinder<View>(EntitySuggestionViewBinder::bind));

                adapter.registerType(
                        OmniboxSuggestionUiType.TAIL_SUGGESTION,
                        parent -> new BaseSuggestionView<TailSuggestionView>(
                                new TailSuggestionView(parent.getContext())),
                        new BaseSuggestionViewBinder<TailSuggestionView>(
                                TailSuggestionViewBinder::bind));

                adapter.registerType(
                        OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION,
                        parent -> new BaseSuggestionView<View>(
                                parent.getContext(), R.layout.omnibox_basic_suggestion),
                        new BaseSuggestionViewBinder<View>(SuggestionViewViewBinder::bind));

                adapter.registerType(
                        OmniboxSuggestionUiType.TILE_SUGGESTION,
                        parent -> mQueryTileCoordinator.createView(parent.getContext()),
                        mQueryTileCoordinator::bind);

                adapter.registerType(
                        OmniboxSuggestionUiType.HEADER,
                        parent -> new HeaderView(parent.getContext()),
                        HeaderViewBinder::bind);
                // clang-format on

                mHolder = new SuggestionListViewHolder(container, dropdown);

                for (int i = 0; i < mCallbacks.size(); i++) {
                    mCallbacks.get(i).onResult(mHolder);
                }
                mCallbacks = null;
            }

            @Override
            public void whenLoaded(Callback<SuggestionListViewHolder> callback) {
                if (mHolder != null) {
                    callback.onResult(mHolder);
                    return;
                }
                mCallbacks.add(callback);
            }
        };
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        mMediator.onUrlFocusChange(hasFocus);
    }

    @Override
    public void onUrlAnimationFinished(boolean hasFocus) {
        mMediator.onUrlAnimationFinished(hasFocus);
    }

    @Override
    public void setToolbarDataProvider(ToolbarDataProvider toolbarDataProvider) {
        mMediator.setToolbarDataProvider(toolbarDataProvider);
    }

    @Override
    public void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        mMediator.setOverviewModeBehavior(overviewModeBehavior);
    }

    @Override
    public void setAutocompleteProfile(Profile profile) {
        mMediator.setAutocompleteProfile(profile);
        mQueryTileCoordinator.setProfile(profile);
    }

    @Override
    public void setWindowAndroid(WindowAndroid windowAndroid) {
        mMediator.setWindowAndroid(windowAndroid);
    }

    @Override
    public void setActivityTabProvider(ActivityTabProvider provider) {
        mMediator.setActivityTabProvider(provider);
    }

    @Override
    public void setShareDelegateSupplier(Supplier<ShareDelegate> shareDelegateSupplier) {
        mMediator.setShareDelegateSupplier(shareDelegateSupplier);
    }

    @Override
    public void setShouldPreventOmniboxAutocomplete(boolean prevent) {
        mMediator.setShouldPreventOmniboxAutocomplete(prevent);
    }

    @Override
    public int getSuggestionCount() {
        return mMediator.getSuggestionCount();
    }

    @Override
    public OmniboxSuggestion getSuggestionAt(int index) {
        return mMediator.getSuggestionAt(index);
    }

    @Override
    public void onNativeInitialized() {
        mMediator.onNativeInitialized();
    }

    @Override
    public void onVoiceResults(@Nullable List<VoiceRecognitionHandler.VoiceResult> results) {
        mMediator.onVoiceResults(results);
    }

    @Override
    public long getCurrentNativeAutocompleteResult() {
        return mMediator.getCurrentNativeAutocompleteResult();
    }

    @Override
    public void updateSuggestionListLayoutDirection() {
        mMediator.setLayoutDirection(ViewCompat.getLayoutDirection(mParent));
    }

    @Override
    public void updateVisualsForState(boolean useDarkColors, boolean isIncognito) {
        mMediator.updateVisualsForState(useDarkColors, isIncognito);
    }

    @Override
    public void setShowCachedZeroSuggestResults(boolean showCachedZeroSuggestResults) {
        mMediator.setShowCachedZeroSuggestResults(showCachedZeroSuggestResults);
    }

    @Override
    public boolean handleKeyEvent(int keyCode, KeyEvent event) {
        boolean isShowingList = mDropdown != null && mDropdown.getViewGroup().isShown();

        boolean isUpOrDown = KeyNavigationUtil.isGoUpOrDown(event);
        if (isShowingList && mMediator.getSuggestionCount() > 0 && isUpOrDown) {
            mMediator.allowPendingItemSelection();
        }
        boolean isValidListKey = isUpOrDown || KeyNavigationUtil.isGoRight(event)
                || KeyNavigationUtil.isGoLeft(event) || KeyNavigationUtil.isEnter(event);
        if (isShowingList && isValidListKey && mDropdown.getViewGroup().onKeyDown(keyCode, event)) {
            return true;
        }
        if (KeyNavigationUtil.isEnter(event) && mParent.getVisibility() == View.VISIBLE) {
            mMediator.loadTypedOmniboxText(event.getEventTime());
            return true;
        }
        return false;
    }

    @Override
    public void onTextChanged(String textWithoutAutocomplete, String textWithAutocomplete) {
        mMediator.onTextChanged(textWithoutAutocomplete, textWithAutocomplete);
    }

    @Override
    public void startAutocompleteForQuery(String query) {
        mMediator.startAutocompleteForQuery(query);
    }

    @Override
    public String qualifyPartialURLQuery(String query) {
        return AutocompleteControllerJni.get().qualifyPartialURLQuery(query);
    }

    @Override
    public void prefetchZeroSuggestResults() {
        AutocompleteControllerJni.get().prefetchZeroSuggestResults();
    }

    @VisibleForTesting
    OmniboxSuggestionsDropdown getSuggestionsDropdown() {
        return mDropdown;
    }

    @VisibleForTesting
    void setAutocompleteController(AutocompleteController controller) {
        mMediator.setAutocompleteController(controller);
    }

    @VisibleForTesting
    OnSuggestionsReceivedListener getSuggestionsReceivedListenerForTest() {
        return mMediator;
    }

    @VisibleForTesting
    ModelList getSuggestionModelList() {
        return mMediator.getSuggestionModelList();
    }

    private void onTileSelected(QueryTile queryTile) {
        mMediator.onQueryTileSelected(queryTile);
    }
}
