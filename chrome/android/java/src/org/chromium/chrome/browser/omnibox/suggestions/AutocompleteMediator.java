// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.os.Handler;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.Supplier;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.GlobalDiscardableReferencePool;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.omnibox.LocationBarVoiceRecognitionHandler;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.answer.AnswerSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.BasicSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.editurl.EditUrlSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.entity.EntitySuggestionProcessor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.util.ConversionUtils;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Handles updating the model state for the currently visible omnibox suggestions.
 */
class AutocompleteMediator
        implements OnSuggestionsReceivedListener, SuggestionHost, StartStopWithNativeObserver {
    /** A struct containing information about the suggestion and its view type. */
    private static class SuggestionViewInfo {
        /** Processor managing the suggestion. */
        public final SuggestionProcessor processor;

        /** The suggestion this info represents. */
        public final OmniboxSuggestion suggestion;

        /** The model the view uses to render the suggestion. */
        public final PropertyModel model;

        public SuggestionViewInfo(SuggestionProcessor suggestionProcessor,
                OmniboxSuggestion omniboxSuggestion, PropertyModel propertyModel) {
            processor = suggestionProcessor;
            suggestion = omniboxSuggestion;
            model = propertyModel;
        }
    }

    private static final String TAG = "Autocomplete";
    private static final int SUGGESTION_NOT_FOUND = -1;

    private static final int MAX_IMAGE_CACHE_SIZE = 500 * ConversionUtils.BYTES_PER_KILOBYTE;

    // Delay triggering the omnibox results upon key press to allow the location bar to repaint
    // with the new characters.
    private static final long OMNIBOX_SUGGESTION_START_DELAY_MS = 30;
    private static final int OMNIBOX_HISTOGRAMS_MAX_SUGGESTIONS = 10;

    private final Context mContext;
    private final AutocompleteDelegate mDelegate;
    private final UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;
    private final PropertyModel mListPropertyModel;
    private final List<SuggestionViewInfo> mCurrentModels;
    private final List<Runnable> mDeferredNativeRunnables = new ArrayList<Runnable>();
    private final Handler mHandler;
    private final BasicSuggestionProcessor mBasicSuggestionProcessor;
    private @Nullable EditUrlSuggestionProcessor mEditUrlProcessor;
    private AnswerSuggestionProcessor mAnswerSuggestionProcessor;
    private final EntitySuggestionProcessor mEntitySuggestionProcessor;

    private ToolbarDataProvider mDataProvider;
    private boolean mNativeInitialized;
    private AutocompleteController mAutocomplete;
    private long mUrlFocusTime;

    @IntDef({SuggestionVisibilityState.DISALLOWED, SuggestionVisibilityState.PENDING_ALLOW,
            SuggestionVisibilityState.ALLOWED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface SuggestionVisibilityState {
        int DISALLOWED = 0;
        int PENDING_ALLOW = 1;
        int ALLOWED = 2;
    }
    @SuggestionVisibilityState
    private int mSuggestionVisibilityState;

    // The timestamp (using SystemClock.elapsedRealtime()) at the point when the user started
    // modifying the omnibox with new input.
    private long mNewOmniboxEditSessionTimestamp = -1;
    // Set to true when the user has started typing new input in the omnibox, set to false
    // when the omnibox loses focus or becomes empty.
    private boolean mHasStartedNewOmniboxEditSession;

    /**
     * The text shown in the URL bar (user text + inline autocomplete) after the most recent set of
     * omnibox suggestions was received. When the user presses enter in the omnibox, this value is
     * compared to the URL bar text to determine whether the first suggestion is still valid.
     */
    private String mUrlTextAfterSuggestionsReceived;

    private Runnable mRequestSuggestions;
    private DeferredOnSelectionRunnable mDeferredOnSelection;

    private boolean mShowCachedZeroSuggestResults;
    private boolean mShouldPreventOmniboxAutocomplete;

    private boolean mPreventSuggestionListPropertyChanges;
    private long mLastActionUpTimestamp;
    private boolean mIgnoreOmniboxItemSelection = true;
    private float mMaxRequiredWidth;
    private float mMaxMatchContentsWidth;
    private boolean mUseDarkColors = true;
    private boolean mShowSuggestionFavicons;
    private int mLayoutDirection;

    private WindowAndroid mWindowAndroid;
    private ActivityLifecycleDispatcher mLifecycleDispatcher;
    private ActivityTabTabObserver mTabObserver;

    private ImageFetcher mImageFetcher;

    public AutocompleteMediator(Context context, AutocompleteDelegate delegate,
            UrlBarEditingTextStateProvider textProvider, PropertyModel listPropertyModel) {
        mContext = context;
        mDelegate = delegate;
        mUrlBarEditingTextProvider = textProvider;
        mListPropertyModel = listPropertyModel;
        mCurrentModels = new ArrayList<>();
        mAutocomplete = new AutocompleteController(this);
        mHandler = new Handler();
        Supplier<ImageFetcher> imageFetcherSupplier = this::getImageFetcher;
        mBasicSuggestionProcessor = new BasicSuggestionProcessor(mContext, this, textProvider);
        mAnswerSuggestionProcessor =
                new AnswerSuggestionProcessor(mContext, this, textProvider, imageFetcherSupplier);
        mEditUrlProcessor = new EditUrlSuggestionProcessor(
                mContext, this, delegate, (suggestion) -> onSelection(suggestion, 0));
        mEntitySuggestionProcessor =
                new EntitySuggestionProcessor(mContext, this, imageFetcherSupplier);
    }

    public void destroy() {
        if (mTabObserver != null) {
            mTabObserver.destroy();
        }
        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }
    }

    @Override
    public void onStartWithNative() {}

    @Override
    public void onStopWithNative() {
        recordSuggestionsShown();
    }

    /**
     * Clear all suggestions and update counter of whether AiS Answer was presented (and if so - of
     * what type). Does not notify any property observers of the change.
     */
    private void clearSuggestions() {
        mCurrentModels.clear();
        notifyPropertyModelsChanged();
    }

    private ImageFetcher getImageFetcher() {
        if (getCurrentProfile() == null) {
            return null;
        }
        if (mImageFetcher == null) {
            mImageFetcher = ImageFetcherFactory.createImageFetcher(
                    ImageFetcherConfig.IN_MEMORY_ONLY,
                    GlobalDiscardableReferencePool.getReferencePool(), MAX_IMAGE_CACHE_SIZE);
        }
        return mImageFetcher;
    }

    private Profile getCurrentProfile() {
        return mDataProvider != null ? mDataProvider.getProfile() : null;
    }

    /**
     * Check if the suggestion is created from clipboard.
     *
     * @param suggestion The OmniboxSuggestion to check.
     * @return Whether or not the suggestion is from clipboard.
     */
    private boolean isSuggestionFromClipboard(OmniboxSuggestion suggestion) {
        return suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_URL
                || suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_TEXT
                || suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_IMAGE;
    }

    /**
     * Record histograms for presented suggestions.
     */
    private void recordSuggestionsShown() {
        int richEntitiesCount = 0;
        for (SuggestionViewInfo info : mCurrentModels) {
            info.processor.recordSuggestionPresented(info.suggestion, info.model);

            if (info.processor.getViewTypeId() == OmniboxSuggestionUiType.ENTITY_SUGGESTION) {
                richEntitiesCount++;
            }
        }

        // Note: valid range for histograms must start with (at least) 1. This does not prevent us
        // from reporting 0 as a count though - values lower than 'min' fall in the 'underflow'
        // bucket, while values larger than 'max' will be reported in 'overflow' bucket.
        RecordHistogram.recordLinearCountHistogram("Omnibox.RichEntityShown", richEntitiesCount, 1,
                OMNIBOX_HISTOGRAMS_MAX_SUGGESTIONS, OMNIBOX_HISTOGRAMS_MAX_SUGGESTIONS + 1);
    }

    /**
     * @return The number of current autocomplete suggestions.
     */
    public int getSuggestionCount() {
        return mCurrentModels.size();
    }

    /**
     * Retrieve the omnibox suggestion at the specified index.  The index represents the ordering
     * in the underlying model.  The index does not represent visibility due to the current scroll
     * position of the list.
     *
     * @param index The index of the suggestion to fetch.
     * @return The suggestion at the given index.
     */
    public OmniboxSuggestion getSuggestionAt(int index) {
        return mCurrentModels.get(index).suggestion;
    }

    private void notifyPropertyModelsChanged() {
        if (mPreventSuggestionListPropertyChanges) return;
        ModelList suggestions = mListPropertyModel.get(SuggestionListProperties.SUGGESTION_MODELS);
        suggestions.clear();
        for (int i = 0; i < mCurrentModels.size(); i++) {
            PropertyModel model = mCurrentModels.get(i).model;
            suggestions.add(new ListItem(mCurrentModels.get(i).processor.getViewTypeId(), model));
        }
    }

    /**
     * Sets the data provider for the toolbar.
     */
    void setToolbarDataProvider(ToolbarDataProvider provider) {
        mDataProvider = provider;
    }

    /** Set the WindowAndroid instance associated with the containing Activity. */
    void setWindowAndroid(WindowAndroid window) {
        if (mLifecycleDispatcher != null) {
            mLifecycleDispatcher.unregister(this);
        }

        mWindowAndroid = window;
        if (window != null && window.getActivity().get() != null
                && window.getActivity().get() instanceof AsyncInitializationActivity) {
            mLifecycleDispatcher =
                    ((AsyncInitializationActivity) mWindowAndroid.getActivity().get())
                            .getLifecycleDispatcher();
        }

        if (mLifecycleDispatcher != null) {
            mLifecycleDispatcher.register(this);
        }
    }

    /**
     * Sets the layout direction to be used for any new suggestion views.
     * @see View#setLayoutDirection(int)
     */
    void setLayoutDirection(int layoutDirection) {
        if (mLayoutDirection == layoutDirection) return;
        mLayoutDirection = layoutDirection;
        for (int i = 0; i < mCurrentModels.size(); i++) {
            PropertyModel model = mCurrentModels.get(i).model;
            model.set(SuggestionCommonProperties.LAYOUT_DIRECTION, layoutDirection);
        }
    }

    /**
     * Specifies the visual state to be used by the suggestions.
     * @param useDarkColors Whether dark colors should be used for fonts and icons.
     * @param isIncognito Whether the UI is for incognito mode or not.
     */
    void updateVisualsForState(boolean useDarkColors, boolean isIncognito) {
        mUseDarkColors = useDarkColors;
        mListPropertyModel.set(SuggestionListProperties.IS_INCOGNITO, isIncognito);
        for (int i = 0; i < mCurrentModels.size(); i++) {
            PropertyModel model = mCurrentModels.get(i).model;
            model.set(SuggestionCommonProperties.USE_DARK_COLORS, useDarkColors);
        }
    }

    /**
     * Sets to show cached zero suggest results. This will start both caching zero suggest results
     * in shared preferences and also attempt to show them when appropriate without needing native
     * initialization.
     * @param showCachedZeroSuggestResults Whether cached zero suggest should be shown.
     */
    void setShowCachedZeroSuggestResults(boolean showCachedZeroSuggestResults) {
        mShowCachedZeroSuggestResults = showCachedZeroSuggestResults;
        if (mShowCachedZeroSuggestResults) mAutocomplete.startCachedZeroSuggest();
    }

    /** Notify the mediator that a item selection is pending and should be accepted. */
    void allowPendingItemSelection() {
        mIgnoreOmniboxItemSelection = false;
    }

    /**
     * Signals that native initialization has completed.
     */
    void onNativeInitialized() {
        mNativeInitialized = true;

        mShowSuggestionFavicons =
                ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_SHOW_SUGGESTION_FAVICONS);

        for (Runnable deferredRunnable : mDeferredNativeRunnables) {
            mHandler.post(deferredRunnable);
        }
        mDeferredNativeRunnables.clear();
        mAnswerSuggestionProcessor.onNativeInitialized();
        mBasicSuggestionProcessor.onNativeInitialized();
        mEntitySuggestionProcessor.onNativeInitialized();
        if (mEditUrlProcessor != null) mEditUrlProcessor.onNativeInitialized();
    }

    /**
     * @param provider A means of accessing the activity tab.
     */
    void setActivityTabProvider(ActivityTabProvider provider) {
        if (mEditUrlProcessor != null) mEditUrlProcessor.setActivityTabProvider(provider);

        if (mTabObserver != null) {
            mTabObserver.destroy();
            mTabObserver = null;
        }

        if (provider != null) {
            mTabObserver = new ActivityTabTabObserver(provider) {
                @Override
                public void onPageLoadFinished(Tab tab, String url) {
                    maybeTriggerCacheRefresh(url);
                }

                @Override
                protected void onObservingDifferentTab(Tab tab) {
                    if (tab == null) return;
                    maybeTriggerCacheRefresh(tab.getUrl());
                }

                /**
                 * Trigger ZeroSuggest cache refresh in case user is accessing a new tab page.
                 * Avoid issuing multiple concurrent server requests for the same event to reduce
                 * server pressure.
                 */
                private void maybeTriggerCacheRefresh(String url) {
                    if (url == null) return;
                    if (!UrlConstants.NTP_URL.equals(url)) return;
                    AutocompleteControllerJni.get().prefetchZeroSuggestResults();
                }
            };
        }
    }

    /** @see org.chromium.chrome.browser.omnibox.UrlFocusChangeListener#onUrlFocusChange(boolean) */
    void onUrlFocusChange(boolean hasFocus) {
        if (hasFocus) {
            mUrlFocusTime = System.currentTimeMillis();
            mSuggestionVisibilityState = SuggestionVisibilityState.PENDING_ALLOW;
            if (mNativeInitialized) {
                startZeroSuggest();
            } else {
                mDeferredNativeRunnables.add(() -> {
                    if (TextUtils.isEmpty(mUrlBarEditingTextProvider.getTextWithAutocomplete())) {
                        startZeroSuggest();
                    }
                });
            }
        } else {
            if (mNativeInitialized) recordSuggestionsShown();

            mSuggestionVisibilityState = SuggestionVisibilityState.DISALLOWED;
            mHasStartedNewOmniboxEditSession = false;
            mNewOmniboxEditSessionTimestamp = -1;
            // Prevent any upcoming omnibox suggestions from showing once a URL is loaded (and as
            // a consequence the omnibox is unfocused).
            hideSuggestions();

            if (mImageFetcher != null) mImageFetcher.clear();
        }
        if (mEditUrlProcessor != null) mEditUrlProcessor.onUrlFocusChange(hasFocus);
        mAnswerSuggestionProcessor.onUrlFocusChange(hasFocus);
        mBasicSuggestionProcessor.onUrlFocusChange(hasFocus);
        mEntitySuggestionProcessor.onUrlFocusChange(hasFocus);
    }

    /**
     * @see
     * org.chromium.chrome.browser.omnibox.UrlFocusChangeListener#onUrlAnimationFinished(boolean)
     */
    void onUrlAnimationFinished(boolean hasFocus) {
        mSuggestionVisibilityState =
                hasFocus ? SuggestionVisibilityState.ALLOWED : SuggestionVisibilityState.DISALLOWED;
        updateOmniboxSuggestionsVisibility();
    }

    /**
     * Updates the profile used for generating autocomplete suggestions.
     * @param profile The profile to be used.
     */
    void setAutocompleteProfile(Profile profile) {
        mAutocomplete.setProfile(profile);
        mBasicSuggestionProcessor.setProfile(profile);
        if (mEditUrlProcessor != null) mEditUrlProcessor.setProfile(profile);
    }

    /**
     * Whether omnibox autocomplete should currently be prevented from generating suggestions.
     */
    void setShouldPreventOmniboxAutocomplete(boolean prevent) {
        mShouldPreventOmniboxAutocomplete = prevent;
    }

    /**
     * @see AutocompleteController#onVoiceResults(List)
     */
    void onVoiceResults(@Nullable List<LocationBarVoiceRecognitionHandler.VoiceResult> results) {
        mAutocomplete.onVoiceResults(results);
    }

    /**
     * @return The current native pointer to the autocomplete results.
     */
    // TODO(tedchoc): Figure out how to remove this.
    long getCurrentNativeAutocompleteResult() {
        return mAutocomplete.getCurrentNativeAutocompleteResult();
    }

    // TODO(mdjones): This should only exist in the BasicSuggestionProcessor.
    @Override
    public SuggestionViewDelegate createSuggestionViewDelegate(
            OmniboxSuggestion suggestion, int position) {
        return new SuggestionViewDelegate() {
            @Override
            public void onSetUrlToSuggestion() {
                if (mIgnoreOmniboxItemSelection) return;
                mIgnoreOmniboxItemSelection = true;
                AutocompleteMediator.this.onSetUrlToSuggestion(suggestion);
            }

            @Override
            public void onSelection() {
                AutocompleteMediator.this.onSelection(suggestion, position);
            }

            @Override
            public void onRefineSuggestion() {
                AutocompleteMediator.this.onRefineSuggestion(suggestion);
            }

            @Override
            public void onLongPress() {
                AutocompleteMediator.this.onLongPress(suggestion, position);
            }

            @Override
            public void onGestureUp(long timestamp) {
                mLastActionUpTimestamp = timestamp;
            }

            @Override
            public void onGestureDown() {
                stopAutocomplete(false);
            }

            @Override
            public int getAdditionalTextLine1StartPadding(TextView line1, int maxTextWidth) {
                if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) return 0;
                if (suggestion.getType() != OmniboxSuggestionType.SEARCH_SUGGEST_TAIL) return 0;

                String fillIntoEdit = suggestion.getFillIntoEdit();
                float fullTextWidth =
                        line1.getPaint().measureText(fillIntoEdit, 0, fillIntoEdit.length());
                String query = line1.getText().toString();
                float abbreviatedTextWidth = line1.getPaint().measureText(query, 0, query.length());

                AutocompleteMediator.this.onTextWidthsUpdated(fullTextWidth, abbreviatedTextWidth);

                final float maxRequiredWidth = AutocompleteMediator.this.mMaxRequiredWidth;
                final float maxMatchContentsWidth =
                        AutocompleteMediator.this.mMaxMatchContentsWidth;
                return (int) ((maxTextWidth > maxRequiredWidth)
                                ? (fullTextWidth - abbreviatedTextWidth)
                                : Math.max(maxTextWidth - maxMatchContentsWidth, 0));
            }
        };
    }

    /**
     * Triggered when the user selects one of the omnibox suggestions to navigate to.
     * @param suggestion The OmniboxSuggestion which was selected.
     * @param position Position of the suggestion in the drop down view.
     */
    private void onSelection(OmniboxSuggestion suggestion, int position) {
        if (mShowCachedZeroSuggestResults && !mNativeInitialized) {
            mDeferredOnSelection = new DeferredOnSelectionRunnable(suggestion, position) {
                @Override
                public void run() {
                    onSelection(this.mSuggestion, this.mPosition);
                }
            };
            return;
        }

        // Note: this call is typically scheduled for execution, rather than invoked directly.
        // In some situations this means the content of mCurrentModels may change meanwhile.
        int verifiedIndex = findSuggestionInModel(suggestion, position);
        if (verifiedIndex != SUGGESTION_NOT_FOUND) {
            SuggestionViewInfo info = mCurrentModels.get(verifiedIndex);
            info.processor.recordSuggestionUsed(info.suggestion, info.model);
        }

        loadUrlFromOmniboxMatch(position, suggestion, mLastActionUpTimestamp, true);
        mDelegate.hideKeyboard();
    }

    /**
     * Triggered when the user selects to refine one of the omnibox suggestions.
     * @param suggestion The suggestion selected.
     */
    private void onRefineSuggestion(OmniboxSuggestion suggestion) {
        stopAutocomplete(false);
        boolean isUrlSuggestion = suggestion.isUrlSuggestion();
        String refineText = suggestion.getFillIntoEdit();
        if (!isUrlSuggestion) refineText = TextUtils.concat(refineText, " ").toString();

        mDelegate.setOmniboxEditingText(refineText);
        onTextChanged(mUrlBarEditingTextProvider.getTextWithoutAutocomplete(),
                mUrlBarEditingTextProvider.getTextWithAutocomplete());
        if (isUrlSuggestion) {
            RecordUserAction.record("MobileOmniboxRefineSuggestion.Url");
        } else {
            RecordUserAction.record("MobileOmniboxRefineSuggestion.Search");
        }
    }

    /**
     * Triggered when the user long presses the omnibox suggestion.
     * @param suggestion The suggestion selected.
     * @param position The position of the suggestion.
     */
    private void onLongPress(OmniboxSuggestion suggestion, int position) {
        RecordUserAction.record("MobileOmniboxDeleteGesture");
        if (!suggestion.isDeletable()) return;

        if (mWindowAndroid == null) return;
        Activity activity = mWindowAndroid.getActivity().get();
        if (activity == null || !(activity instanceof AsyncInitializationActivity)) return;
        ModalDialogManager manager =
                ((AsyncInitializationActivity) activity).getModalDialogManager();
        if (manager == null) {
            assert false : "No modal dialog manager registered for this activity.";
            return;
        }

        ModalDialogProperties.Controller dialogController = new ModalDialogProperties.Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {
                if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                    RecordUserAction.record("MobileOmniboxDeleteRequested");
                    mAutocomplete.deleteSuggestion(position, suggestion.hashCode());
                    manager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
                    manager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                }
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {}
        };

        Resources resources = mContext.getResources();
        @StringRes int dialogMessageId = R.string.omnibox_confirm_delete;
        if (isSuggestionFromClipboard(suggestion)) {
            dialogMessageId = R.string.omnibox_confirm_delete_from_clipboard;
        }

        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(ModalDialogProperties.TITLE, suggestion.getDisplayText())
                        .with(ModalDialogProperties.MESSAGE, resources, dialogMessageId)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources, R.string.ok)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.cancel)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();

        // Prevent updates to the shown omnibox suggestions list while the dialog is open.
        stopAutocomplete(false);
        manager.showDialog(model, ModalDialogManager.ModalDialogType.APP);
    }

    /**
     * Triggered when the user navigates to one of the suggestions without clicking on it.
     * @param suggestion The suggestion that was selected.
     */
    void onSetUrlToSuggestion(OmniboxSuggestion suggestion) {
        mDelegate.setOmniboxEditingText(suggestion.getFillIntoEdit());
    }

    /**
     * Triggered when text width information is updated.
     * These values should be used to calculate max text widths.
     * @param requiredWidth a new required width.
     * @param matchContentsWidth a new match contents width.
     */
    private void onTextWidthsUpdated(float requiredWidth, float matchContentsWidth) {
        mMaxRequiredWidth = Math.max(mMaxRequiredWidth, requiredWidth);
        mMaxMatchContentsWidth = Math.max(mMaxMatchContentsWidth, matchContentsWidth);
    }

    /**
     * Updates the maximum widths required to render the suggestions.
     * This is needed for infinite suggestions where we try to vertically align the leading
     * ellipsis.
     */
    private void resetMaxTextWidths() {
        mMaxRequiredWidth = 0;
        mMaxMatchContentsWidth = 0;
    }

    /**
     * Updates the URL we will navigate to from suggestion, if needed. This will update the search
     * URL to be of the corpus type if query in the omnibox is displayed and update aqs= parameter
     * on regular web search URLs.
     *
     * @param suggestion The chosen omnibox suggestion.
     * @param selectedIndex The index of the chosen omnibox suggestion.
     * @param skipCheck Whether to skip an out of bounds check.
     * @return The url to navigate to.
     */
    private String updateSuggestionUrlIfNeeded(
            OmniboxSuggestion suggestion, int selectedIndex, boolean skipCheck) {
        // Only called once we have suggestions, and don't have a listener though which we can
        // receive suggestions until the native side is ready, so this is safe
        assert mNativeInitialized
            : "updateSuggestionUrlIfNeeded called before native initialization";

        if (suggestion.getType() == OmniboxSuggestionType.VOICE_SUGGEST) return suggestion.getUrl();

        int verifiedIndex = SUGGESTION_NOT_FOUND;
        if (!skipCheck) {
            verifiedIndex = findSuggestionInModel(suggestion, selectedIndex);
        }

        // If we do not have the suggestion as part of our results, skip the URL update.
        if (verifiedIndex == SUGGESTION_NOT_FOUND) return suggestion.getUrl();

        // TODO(mariakhomenko): Ideally we want to update match destination URL with new aqs
        // for query in the omnibox and voice suggestions, but it's currently difficult to do.
        long elapsedTimeSinceInputChange = mNewOmniboxEditSessionTimestamp > 0
                ? (SystemClock.elapsedRealtime() - mNewOmniboxEditSessionTimestamp)
                : -1;
        String updatedUrl = mAutocomplete.updateMatchDestinationUrlWithQueryFormulationTime(
                verifiedIndex, suggestion.hashCode(), elapsedTimeSinceInputChange);

        return updatedUrl == null ? suggestion.getUrl() : updatedUrl;
    }

    /**
     * Check if the supplied suggestion is still in the current model and return its index.
     *
     * This call should be used to confirm that model has not been changed ahead of an event being
     * called by all the methods that are dispatched rather than called directly.
     *
     * @param suggestion Suggestion to look for.
     * @param index Last known position of the suggestion.
     * @return Current index of the supplied suggestion, or SUGGESTION_NOT_FOUND if it is no longer
     *         part of the model.
     */
    @SuppressWarnings("ReferenceEquality")
    private int findSuggestionInModel(OmniboxSuggestion suggestion, int position) {
        if (getSuggestionCount() > position && getSuggestionAt(position) == suggestion) {
            return position;
        }

        // Underlying omnibox results may have changed since the selection was made,
        // find the suggestion item, if possible.
        for (int index = 0; index < getSuggestionCount(); index++) {
            if (suggestion.equals(getSuggestionAt(index))) {
                return index;
            }
        }

        return SUGGESTION_NOT_FOUND;
    }

    /**
     * Notifies the autocomplete system that the text has changed that drives autocomplete and the
     * autocomplete suggestions should be updated.
     */
    public void onTextChanged(String textWithoutAutocomplete, String textWithAutocomplete) {
        // crbug.com/764749
        Log.w(TAG, "onTextChangedForAutocomplete");

        if (mShouldPreventOmniboxAutocomplete) return;

        mIgnoreOmniboxItemSelection = true;
        cancelPendingAutocompleteStart();

        if (!mHasStartedNewOmniboxEditSession && mNativeInitialized) {
            mAutocomplete.resetSession();
            mNewOmniboxEditSessionTimestamp = SystemClock.elapsedRealtime();
            mHasStartedNewOmniboxEditSession = true;
        }

        stopAutocomplete(false);
        if (TextUtils.isEmpty(textWithoutAutocomplete)) {
            // crbug.com/764749
            Log.w(TAG, "onTextChangedForAutocomplete: url is empty");
            hideSuggestions();
            startZeroSuggest();
        } else {
            assert mRequestSuggestions == null : "Multiple omnibox requests in flight.";
            mRequestSuggestions = () -> {
                boolean preventAutocomplete = !mUrlBarEditingTextProvider.shouldAutocomplete();
                mRequestSuggestions = null;

                if (!mDataProvider.hasTab()) {
                    // crbug.com/764749
                    Log.w(TAG, "onTextChangedForAutocomplete: no tab");
                    return;
                }

                Profile profile = mDataProvider.getProfile();
                int cursorPosition = -1;
                if (mUrlBarEditingTextProvider.getSelectionStart()
                        == mUrlBarEditingTextProvider.getSelectionEnd()) {
                    // Conveniently, if there is no selection, those two functions return -1,
                    // exactly the same value needed to pass to start() to indicate no cursor
                    // position.  Hence, there's no need to check for -1 here explicitly.
                    cursorPosition = mUrlBarEditingTextProvider.getSelectionStart();
                }
                int pageClassification =
                        mDataProvider.getPageClassification(mDelegate.didFocusUrlFromFakebox());
                mAutocomplete.start(profile, mDataProvider.getCurrentUrl(), pageClassification,
                        textWithoutAutocomplete, cursorPosition, preventAutocomplete);
            };
            if (mNativeInitialized) {
                mHandler.postDelayed(mRequestSuggestions, OMNIBOX_SUGGESTION_START_DELAY_MS);
            } else {
                mDeferredNativeRunnables.add(mRequestSuggestions);
            }
        }

        mDelegate.onUrlTextChanged();
    }

    /**
     * @param suggestion The suggestion to be processed.
     * @return The appropriate suggestion processor for the provided suggestion.
     */
    private SuggestionProcessor getProcessorForSuggestion(OmniboxSuggestion suggestion) {
        if (mAnswerSuggestionProcessor.doesProcessSuggestion(suggestion)) {
            return mAnswerSuggestionProcessor;
        } else if (mEntitySuggestionProcessor.doesProcessSuggestion(suggestion)) {
            return mEntitySuggestionProcessor;
        } else if (mEditUrlProcessor != null
                && mEditUrlProcessor.doesProcessSuggestion(suggestion)) {
            return mEditUrlProcessor;
        }
        return mBasicSuggestionProcessor;
    }

    @Override
    public void onSuggestionsReceived(
            List<OmniboxSuggestion> newSuggestions, String inlineAutocompleteText) {
        if (mShouldPreventOmniboxAutocomplete
                || mSuggestionVisibilityState == SuggestionVisibilityState.DISALLOWED) {
            return;
        }

        // This is a callback from a listener that is set up by onNativeLibraryReady,
        // so can only be called once the native side is set up unless we are showing
        // cached java-only suggestions.
        assert mNativeInitialized
                || mShowCachedZeroSuggestResults
            : "Native suggestions received before native side intialialized";

        if (mDeferredOnSelection != null) {
            mDeferredOnSelection.setShouldLog(newSuggestions.size() > mDeferredOnSelection.mPosition
                    && mDeferredOnSelection.mSuggestion.equals(
                            newSuggestions.get(mDeferredOnSelection.mPosition)));
            mDeferredOnSelection.run();
            mDeferredOnSelection = null;
        }
        String userText = mUrlBarEditingTextProvider.getTextWithoutAutocomplete();
        mUrlTextAfterSuggestionsReceived = userText + inlineAutocompleteText;

        if (mCurrentModels.size() == newSuggestions.size()) {
            boolean sameSuggestions = true;
            for (int i = 0; i < mCurrentModels.size(); i++) {
                if (!mCurrentModels.get(i).suggestion.equals(newSuggestions.get(i))) {
                    sameSuggestions = false;
                    break;
                }
            }
            if (sameSuggestions) return;
        }

        // Show the suggestion list.
        resetMaxTextWidths();
        // Ensure the list is fully replaced before broadcasting any change notifications.
        mPreventSuggestionListPropertyChanges = true;
        mCurrentModels.clear();
        for (int i = 0; i < newSuggestions.size(); i++) {
            OmniboxSuggestion suggestion = newSuggestions.get(i);
            SuggestionProcessor processor = getProcessorForSuggestion(suggestion);
            PropertyModel model = processor.createModelForSuggestion(suggestion);
            model.set(SuggestionCommonProperties.LAYOUT_DIRECTION, mLayoutDirection);
            model.set(SuggestionCommonProperties.USE_DARK_COLORS, mUseDarkColors);
            model.set(SuggestionCommonProperties.SHOW_SUGGESTION_ICONS,
                    mShowSuggestionFavicons
                            || DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext));

            // Before populating the model, add it to the list of current models.  If the suggestion
            // has an image and the image was already cached, it will be updated synchronously and
            // the model will only have the image populated if it is tracked as a current model.
            mCurrentModels.add(new SuggestionViewInfo(processor, suggestion, model));

            processor.populateModel(suggestion, model, i);
        }
        mPreventSuggestionListPropertyChanges = false;
        notifyPropertyModelsChanged();

        if (mListPropertyModel.get(SuggestionListProperties.VISIBLE) && getSuggestionCount() == 0) {
            hideSuggestions();
        }
        mDelegate.onSuggestionsChanged(inlineAutocompleteText);

        updateOmniboxSuggestionsVisibility();
    }

    /**
     * Load the url corresponding to the typed omnibox text.
     * @param eventTime The timestamp the load was triggered by the user.
     */
    void loadTypedOmniboxText(long eventTime) {
        mDelegate.hideKeyboard();

        final String urlText = mUrlBarEditingTextProvider.getTextWithAutocomplete();
        if (mNativeInitialized) {
            findMatchAndLoadUrl(urlText, eventTime);
        } else {
            mDeferredNativeRunnables.add(() -> findMatchAndLoadUrl(urlText, eventTime));
        }
    }

    private void findMatchAndLoadUrl(String urlText, long inputStart) {
        OmniboxSuggestion suggestionMatch;
        boolean inSuggestionList = true;

        if (getSuggestionCount() > 0
                && urlText.trim().equals(mUrlTextAfterSuggestionsReceived.trim())) {
            // Common case: the user typed something, received suggestions, then pressed enter.
            suggestionMatch = getSuggestionAt(0);
        } else {
            // Less common case: there are no valid omnibox suggestions. This can happen if the
            // user tapped the URL bar to dismiss the suggestions, then pressed enter. This can
            // also happen if the user presses enter before any suggestions have been received
            // from the autocomplete controller.
            suggestionMatch = mAutocomplete.classify(urlText, mDelegate.didFocusUrlFromFakebox());
            // Classify matches don't propagate to java, so skip the OOB check.
            inSuggestionList = false;

            // If urlText couldn't be classified, bail.
            if (suggestionMatch == null) return;
        }

        loadUrlFromOmniboxMatch(0, suggestionMatch, inputStart, inSuggestionList);
    }

    /**
     * Loads the specified omnibox suggestion.
     *
     * @param matchPosition The position of the selected omnibox suggestion.
     * @param suggestion The suggestion selected.
     * @param inputStart The timestamp the input was started.
     * @param inVisibleSuggestionList Whether the suggestion is in the visible suggestion list.
     */
    private void loadUrlFromOmniboxMatch(int matchPosition, OmniboxSuggestion suggestion,
            long inputStart, boolean inVisibleSuggestionList) {
        final long activationTime = System.currentTimeMillis();
        RecordHistogram.recordMediumTimesHistogram(
                "Omnibox.FocusToOpenTimeAnyPopupState3", activationTime - mUrlFocusTime);

        String url =
                updateSuggestionUrlIfNeeded(suggestion, matchPosition, !inVisibleSuggestionList);

        // loadUrl modifies AutocompleteController's state clearing the native
        // AutocompleteResults needed by onSuggestionsSelected. Therefore,
        // loadUrl should should be invoked last.
        int transition = suggestion.getTransition();
        int type = suggestion.getType();
        String currentPageUrl = mDataProvider.getCurrentUrl();
        WebContents webContents =
                mDataProvider.hasTab() ? mDataProvider.getTab().getWebContents() : null;
        long elapsedTimeSinceModified = mNewOmniboxEditSessionTimestamp > 0
                ? (SystemClock.elapsedRealtime() - mNewOmniboxEditSessionTimestamp)
                : -1;
        boolean shouldSkipNativeLog = mShowCachedZeroSuggestResults
                && (mDeferredOnSelection != null) && !mDeferredOnSelection.shouldLog();
        if (!shouldSkipNativeLog) {
            int autocompleteLength = mUrlBarEditingTextProvider.getTextWithAutocomplete().length()
                    - mUrlBarEditingTextProvider.getTextWithoutAutocomplete().length();
            int pageClassification =
                    mDataProvider.getPageClassification(mDelegate.didFocusUrlFromFakebox());
            mAutocomplete.onSuggestionSelected(matchPosition, suggestion.hashCode(), type,
                    currentPageUrl, pageClassification, elapsedTimeSinceModified,
                    autocompleteLength, webContents);
        }
        if (((transition & PageTransition.CORE_MASK) == PageTransition.TYPED)
                && TextUtils.equals(url, mDataProvider.getCurrentUrl())) {
            // When the user hit enter on the existing permanent URL, treat it like a
            // reload for scoring purposes.  We could detect this by just checking
            // user_input_in_progress_, but it seems better to treat "edits" that end
            // up leaving the URL unchanged (e.g. deleting the last character and then
            // retyping it) as reloads too.  We exclude non-TYPED transitions because if
            // the transition is GENERATED, the user input something that looked
            // different from the current URL, even if it wound up at the same place
            // (e.g. manually retyping the same search query), and it seems wrong to
            // treat this as a reload.
            transition = PageTransition.RELOAD;
        } else if (((transition & PageTransition.CORE_MASK) == PageTransition.GENERATED)
                && TextUtils.equals(
                        suggestion.getFillIntoEdit(), mDataProvider.getDisplaySearchTerms())) {
            // When the omnibox is displaying the default search provider search terms,
            // the user focuses the omnibox, and hits Enter without refining the search
            // terms, we should classify this transition as a RELOAD.
            transition = PageTransition.RELOAD;
        } else if (type == OmniboxSuggestionType.URL_WHAT_YOU_TYPED
                && mUrlBarEditingTextProvider.wasLastEditPaste()) {
            // It's important to use the page transition from the suggestion or we might end
            // up saving generated URLs as typed URLs, which would then pollute the subsequent
            // omnibox results. There is one special case where the suggestion text was pasted,
            // where we want the transition type to be LINK.

            transition = PageTransition.LINK;
        }
        mDelegate.loadUrl(url, transition, inputStart);
    }

    /**
     * Make a zero suggest request if:
     * - Native is loaded.
     * - The URL bar has focus.
     * - The current tab is not incognito.
     */
    private void startZeroSuggest() {
        // Reset "edited" state in the omnibox if zero suggest is triggered -- new edits
        // now count as a new session.
        mHasStartedNewOmniboxEditSession = false;
        mNewOmniboxEditSessionTimestamp = -1;
        if (mNativeInitialized && mDelegate.isUrlBarFocused() && mDataProvider.hasTab()) {
            int pageClassification =
                    mDataProvider.getPageClassification(mDelegate.didFocusUrlFromFakebox());
            mAutocomplete.startZeroSuggest(mDataProvider.getProfile(),
                    mUrlBarEditingTextProvider.getTextWithAutocomplete(),
                    mDataProvider.getCurrentUrl(), pageClassification, mDataProvider.getTitle());
        }
    }

    /**
     * Update whether the omnibox suggestions are visible.
     */
    private void updateOmniboxSuggestionsVisibility() {
        boolean shouldBeVisible = mSuggestionVisibilityState == SuggestionVisibilityState.ALLOWED
                && getSuggestionCount() > 0;
        boolean wasVisible = mListPropertyModel.get(SuggestionListProperties.VISIBLE);
        mListPropertyModel.set(SuggestionListProperties.VISIBLE, shouldBeVisible);
        if (shouldBeVisible && !wasVisible) {
            mIgnoreOmniboxItemSelection = true; // Reset to default value.
        }
    }

    /**
     * Hides the omnibox suggestion popup.
     *
     * <p>
     * Signals the autocomplete controller to stop generating omnibox suggestions.
     *
     * @see AutocompleteController#stop(boolean)
     */
    private void hideSuggestions() {
        if (mAutocomplete == null || !mNativeInitialized) return;

        stopAutocomplete(true);

        clearSuggestions();
        updateOmniboxSuggestionsVisibility();
    }

    /**
     * Signals the autocomplete controller to stop generating omnibox suggestions and cancels the
     * queued task to start the autocomplete controller, if any.
     *
     * @param clear Whether to clear the most recent autocomplete results.
     */
    private void stopAutocomplete(boolean clear) {
        if (mAutocomplete != null) mAutocomplete.stop(clear);
        cancelPendingAutocompleteStart();
    }

    /**
     * Cancels the queued task to start the autocomplete controller, if any.
     */
    @VisibleForTesting
    void cancelPendingAutocompleteStart() {
        if (mRequestSuggestions != null) {
            // There is a request for suggestions either waiting for the native side
            // to start, or on the message queue. Remove it from wherever it is.
            if (!mDeferredNativeRunnables.remove(mRequestSuggestions)) {
                mHandler.removeCallbacks(mRequestSuggestions);
            }
            mRequestSuggestions = null;
        }
    }

    /**
     * Trigger autocomplete for the given query.
     */
    void startAutocompleteForQuery(String query) {
        stopAutocomplete(false);
        if (mDataProvider.hasTab()) {
            mAutocomplete.start(mDataProvider.getProfile(), mDataProvider.getCurrentUrl(),
                    mDataProvider.getPageClassification(false), query, -1, false);
        }
    }

    /**
     * Sets the autocomplete controller for the location bar.
     *
     * @param controller The controller that will handle autocomplete/omnibox suggestions.
     * @note Only used for testing.
     */
    @VisibleForTesting
    public void setAutocompleteController(AutocompleteController controller) {
        if (mAutocomplete != null) stopAutocomplete(true);
        mAutocomplete = controller;
    }

    private abstract static class DeferredOnSelectionRunnable implements Runnable {
        protected final OmniboxSuggestion mSuggestion;
        protected final int mPosition;
        protected boolean mShouldLog;

        public DeferredOnSelectionRunnable(OmniboxSuggestion suggestion, int position) {
            this.mSuggestion = suggestion;
            this.mPosition = position;
        }

        /**
         * Set whether the selection matches with native results for logging to make sense.
         * @param log Whether the selection should be logged in native code.
         */
        public void setShouldLog(boolean log) {
            mShouldLog = log;
        }

        /**
         * @return Whether the selection should be logged in native code.
         */
        public boolean shouldLog() {
            return mShouldLog;
        }
    }
}
