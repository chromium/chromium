// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.editurl;

import android.content.Context;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.CachedMetrics;
import org.chromium.base.metrics.CachedMetrics.EnumeratedHistogramSample;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.UrlBar.OmniboxAction;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class controls the interaction of the "edit url" suggestion item with the rest of the
 * suggestions list. This class also serves as a mediator, containing logic that interacts with
 * the rest of Chrome.
 */
public class EditUrlSuggestionProcessor implements OnClickListener, SuggestionProcessor {
    /** An interface for handling taps on the suggestion rather than its buttons. */
    public interface SuggestionSelectionHandler {
        /**
         * Handle the edit URL suggestion selection.
         * @param suggestion The selected suggestion.
         */
        void onEditUrlSuggestionSelected(OmniboxSuggestion suggestion);
    }

    /** An interface for modifying the location bar and it's contents. */
    public interface LocationBarDelegate {
        /** Remove focus from the omnibox. */
        void clearOmniboxFocus();

        /**
         * Set the text in the omnibox.
         * @param text The text that should be displayed in the omnibox.
         */
        void setOmniboxEditingText(String text);
    }

    /** The actions that can be performed on the suggestion view provided by this class. */
    @IntDef({SuggestionAction.EDIT, SuggestionAction.COPY, SuggestionAction.SHARE,
            SuggestionAction.TAP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SuggestionAction {
        int EDIT = 0;
        int COPY = 1;
        int SHARE = 2;
        int TAP = 3;
        int NUM_ENTRIES = 4;
    }

    /** Cached metrics in the event this code is triggered prior to native being initialized. */
    private static final EnumeratedHistogramSample ENUMERATED_SUGGESTION_ACTION =
            new EnumeratedHistogramSample(
                    "Omnibox.EditUrlSuggestionAction", SuggestionAction.NUM_ENTRIES);
    private static final CachedMetrics.ActionEvent ACTION_EDIT_URL_SUGGESTION_TAP =
            new CachedMetrics.ActionEvent("Omnibox.EditUrlSuggestion.Tap");
    private static final CachedMetrics.ActionEvent ACTION_EDIT_URL_SUGGESTION_COPY =
            new CachedMetrics.ActionEvent("Omnibox.EditUrlSuggestion.Copy");
    private static final CachedMetrics.ActionEvent ACTION_EDIT_URL_SUGGESTION_EDIT =
            new CachedMetrics.ActionEvent("Omnibox.EditUrlSuggestion.Edit");
    private static final CachedMetrics.ActionEvent ACTION_EDIT_URL_SUGGESTION_SHARE =
            new CachedMetrics.ActionEvent("Omnibox.EditUrlSuggestion.Share");

    /** The delegate for accessing the location bar for observation and modification. */
    private final LocationBarDelegate mLocationBarDelegate;

    /** A means of accessing the activity's tab. */
    private ActivityTabProvider mTabProvider;

    /** Whether the omnibox has already cleared its content for the focus event. */
    private boolean mHasClearedOmniboxForFocus;

    /** The last omnibox suggestion to be processed. */
    private OmniboxSuggestion mLastProcessedSuggestion;

    /** A handler for suggestion selection. */
    private SuggestionSelectionHandler mSelectionHandler;

    /** The original URL that was in the omnibox when it was focused. */
    private String mOriginalUrl;

    /** The original title of the page. */
    private String mOriginalTitle;

    /** The last time that the omnibox was focused. */
    private long mLastOmniboxFocusTime;

    /** Whether a timing event should be recorded. This will be true once per omnibox focus. */
    private boolean mShouldRecordTimingEvent;

    /** Whether the first suggestion after an omnibox focus event has been processed. */
    private boolean mFirstSuggestionProcessedForCurrentOmniboxFocus;

    /** Whether this processor should ignore all subsequent suggestion. */
    private boolean mIgnoreSuggestions;

    /** Whether suggestion site favicons are enabled. */
    private boolean mEnableSuggestionFavicons;

    /** Edge size (in pixels) of the favicon. Used to request best matching favicon from cache. */
    private final int mDesiredFaviconWidthPx;

    /** Supplies Profile information. */
    private Profile mCurrentUserProfile;

    /** Supplies site favicons. */
    private LargeIconBridge mLargeIconBridge;

    /** Supplies additional control over suggestion model. */
    private final SuggestionHost mSuggestionHost;

    /**
     * @param locationBarDelegate A means of modifying the location bar.
     * @param selectionHandler A mechanism for handling selection of the edit URL suggestion item.
     */
    public EditUrlSuggestionProcessor(Context context, SuggestionHost suggestionHost,
            LocationBarDelegate locationBarDelegate, SuggestionSelectionHandler selectionHandler) {
        mLocationBarDelegate = locationBarDelegate;
        mSelectionHandler = selectionHandler;
        mDesiredFaviconWidthPx = context.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_favicon_size);
        mSuggestionHost = suggestionHost;
    }

    /**
     * Create the view specific to the suggestion this processor is responsible for.
     * @param context An Android context.
     * @return An edit-URL suggestion view.
     */
    public static ViewGroup createView(Context context) {
        LayoutInflater inflater =
                (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        return (ViewGroup) inflater.inflate(R.layout.edit_url_suggestion_layout, null);
    }

    @Override
    public boolean doesProcessSuggestion(OmniboxSuggestion suggestion) {
        Tab activeTab = mTabProvider != null ? mTabProvider.get() : null;

        // The what-you-typed suggestion can potentially appear as the second suggestion in some
        // cases. If the first suggestion isn't the one we want, ignore all subsequent suggestions.
        if (!mFirstSuggestionProcessedForCurrentOmniboxFocus) {
            mFirstSuggestionProcessedForCurrentOmniboxFocus = true;
            mIgnoreSuggestions = activeTab == null || activeTab.isNativePage()
                    || activeTab.isIncognito()
                    || OmniboxSuggestionType.URL_WHAT_YOU_TYPED != suggestion.getType()
                    || !TextUtils.equals(suggestion.getUrl(), activeTab.getUrl());
        }

        if (OmniboxSuggestionType.URL_WHAT_YOU_TYPED != suggestion.getType()
                || mIgnoreSuggestions) {
            return false;
        }
        mLastProcessedSuggestion = suggestion;

        // Only use the URL provided by the "what you typed" suggestion on first omnibox focus.
        // Subsequent suggestions will provide partial URLs which we do not want. If the suggestion
        // URL matches the original, show the suggestion item.
        if (mOriginalUrl == null) mOriginalUrl = mLastProcessedSuggestion.getUrl();

        if (!TextUtils.equals(mLastProcessedSuggestion.getUrl(), mOriginalUrl)) return false;

        if (!mHasClearedOmniboxForFocus) {
            mHasClearedOmniboxForFocus = true;
            mLocationBarDelegate.setOmniboxEditingText("");
        }
        return true;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.EDIT_URL_SUGGESTION;
    }

    @Override
    public PropertyModel createModelForSuggestion(OmniboxSuggestion suggestion) {
        return new PropertyModel(EditUrlSuggestionProperties.ALL_KEYS);
    }

    @Override
    public void populateModel(OmniboxSuggestion suggestion, PropertyModel model, int position) {
        model.set(EditUrlSuggestionProperties.TEXT_CLICK_LISTENER, this);
        model.set(EditUrlSuggestionProperties.BUTTON_CLICK_LISTENER, this);

        // Lazily create LargeIconBridge in case Profile is reported ahead on Native initialized.
        if (mEnableSuggestionFavicons && mLargeIconBridge == null && mCurrentUserProfile != null) {
            mLargeIconBridge = new LargeIconBridge(mCurrentUserProfile);
        }

        if (mLargeIconBridge != null) {
            mLargeIconBridge.getLargeIconForUrl(mLastProcessedSuggestion.getUrl(),
                    mDesiredFaviconWidthPx,
                    (Bitmap icon, int fallbackColor, boolean isFallbackColorDefault,
                            int iconType) -> {
                        model.set(EditUrlSuggestionProperties.SITE_FAVICON, icon);
                    });
        }

        if (mOriginalTitle == null) mOriginalTitle = mTabProvider.get().getTitle();
        model.set(EditUrlSuggestionProperties.TITLE_TEXT, mOriginalTitle);
        model.set(EditUrlSuggestionProperties.URL_TEXT, mLastProcessedSuggestion.getUrl());
    }

    @Override
    public void onNativeInitialized() {
        mEnableSuggestionFavicons =
                ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_SHOW_SUGGESTION_FAVICONS);
    }

    @Override
    public void recordSuggestionPresented(OmniboxSuggestion suggestion, PropertyModel model) {}

    @Override
    public void recordSuggestionUsed(OmniboxSuggestion suggestion, PropertyModel model) {}

    /**
     * Updates the profile used for extracting website favicons.
     * @param profile The profile to be used.
     */
    public void setProfile(Profile profile) {
        if (mCurrentUserProfile == profile) return;
        mCurrentUserProfile = profile;
        mLargeIconBridge = null;
    }

    /**
     * @param provider A means of accessing the activity's tab.
     */
    public void setActivityTabProvider(ActivityTabProvider provider) {
        mTabProvider = provider;
    }

    /**
     * Clean up any state that this coordinator has.
     */
    public void destroy() {
        mLastProcessedSuggestion = null;
        mSelectionHandler = null;
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        if (hasFocus) {
            mLastOmniboxFocusTime = System.currentTimeMillis();
        } else {
            mOriginalUrl = null;
            mOriginalTitle = null;
            mHasClearedOmniboxForFocus = false;
            mLastProcessedSuggestion = null;
            mFirstSuggestionProcessedForCurrentOmniboxFocus = false;
            mIgnoreSuggestions = false;
        }
        mShouldRecordTimingEvent = hasFocus;
    }

    @Override
    public void onClick(View view) {
        Tab activityTab = mTabProvider.get();
        assert activityTab != null : "A tab is required to make changes to the location bar.";

        if (R.id.url_copy_icon == view.getId()) {
            ENUMERATED_SUGGESTION_ACTION.record(SuggestionAction.COPY);
            ACTION_EDIT_URL_SUGGESTION_COPY.record();
            if (mShouldRecordTimingEvent) {
                UrlBar.recordTimedActionForMetrics(OmniboxAction.COPY, mLastOmniboxFocusTime);
                mShouldRecordTimingEvent = false;
            }
            Clipboard.getInstance().copyUrlToClipboard(mLastProcessedSuggestion.getUrl());
        } else if (R.id.url_share_icon == view.getId()) {
            ENUMERATED_SUGGESTION_ACTION.record(SuggestionAction.SHARE);
            ACTION_EDIT_URL_SUGGESTION_SHARE.record();
            if (mShouldRecordTimingEvent) {
                UrlBar.recordTimedActionForMetrics(OmniboxAction.SHARE, mLastOmniboxFocusTime);
                mShouldRecordTimingEvent = false;
            }
            mLocationBarDelegate.clearOmniboxFocus();
            // TODO(mdjones): This should only share the displayed URL instead of the background
            //                tab.
            activityTab.getActivity().getShareDelegate().share(activityTab, false);
        } else if (R.id.url_edit_icon == view.getId()) {
            ENUMERATED_SUGGESTION_ACTION.record(SuggestionAction.EDIT);
            ACTION_EDIT_URL_SUGGESTION_EDIT.record();
            mLocationBarDelegate.setOmniboxEditingText(mLastProcessedSuggestion.getUrl());
        } else {
            ENUMERATED_SUGGESTION_ACTION.record(SuggestionAction.TAP);
            ACTION_EDIT_URL_SUGGESTION_TAP.record();
            // If the event wasn't on any of the buttons, treat is as a tap on the general
            // suggestion.
            if (mSelectionHandler != null) {
                mSelectionHandler.onEditUrlSuggestionSelected(mLastProcessedSuggestion);
            }
        }
    }
}
