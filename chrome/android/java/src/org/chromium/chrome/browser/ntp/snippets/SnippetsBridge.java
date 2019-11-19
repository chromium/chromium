// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ntp.cards.SuggestionsCategoryInfo;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.ContentSuggestionsAdditionalAction;

import java.util.ArrayList;
import java.util.List;

/**
 * Provides access to the snippets to display on the NTP using the C++ ContentSuggestionsService.
 */
public class SnippetsBridge implements SuggestionsSource {
    private long mNativeSnippetsBridge;
    private final ObserverList<Observer> mObserverList = new ObserverList<>();

    public static boolean isCategoryStatusAvailable(@CategoryStatus int status) {
        // Note: This code is duplicated in category_status.cc.
        return status == CategoryStatus.AVAILABLE_LOADING || status == CategoryStatus.AVAILABLE;
    }

    public static boolean isCategoryRemote(@CategoryInt int category) {
        return category > KnownCategories.REMOTE_CATEGORIES_OFFSET;
    }

    /** Returns whether the category is considered "enabled", and can show content suggestions. */
    public static boolean isCategoryEnabled(@CategoryStatus int status) {
        switch (status) {
            case CategoryStatus.INITIALIZING:
            case CategoryStatus.AVAILABLE:
            case CategoryStatus.AVAILABLE_LOADING:
                return true;
        }
        return false;
    }

    public static boolean isCategoryLoading(@CategoryStatus int status) {
        return status == CategoryStatus.AVAILABLE_LOADING || status == CategoryStatus.INITIALIZING;
    }

    /**
     * Creates a SnippetsBridge for getting snippet data for the current user.
     *
     * @param profile Profile of the user that we will retrieve snippets for.
     */
    public SnippetsBridge(Profile profile) {
        mNativeSnippetsBridge = SnippetsBridgeJni.get().init(SnippetsBridge.this, profile);
    }

    @Override
    public void destroy() {
        assert mNativeSnippetsBridge != 0;
        SnippetsBridgeJni.get().destroy(mNativeSnippetsBridge, SnippetsBridge.this);
        mNativeSnippetsBridge = 0;
        mObserverList.clear();
    }

    /**
     * Notifies that Chrome on Android has been upgraded.
     */
    public static void onBrowserUpgraded() {
        SnippetsBridgeJni.get().remoteSuggestionsSchedulerOnBrowserUpgraded();
    }

    /**
     * Notifies that the persistent fetching scheduler woke up.
     */
    public static void onPersistentSchedulerWakeUp() {
        SnippetsBridgeJni.get().remoteSuggestionsSchedulerOnPersistentSchedulerWakeUp();
    }

    @Override
    public boolean areRemoteSuggestionsEnabled() {
        assert mNativeSnippetsBridge != 0;
        return SnippetsBridgeJni.get().areRemoteSuggestionsEnabled(
                mNativeSnippetsBridge, SnippetsBridge.this);
    }

    @Override
    public void fetchRemoteSuggestions() {
        assert mNativeSnippetsBridge != 0;
        SnippetsBridgeJni.get().reloadSuggestions(mNativeSnippetsBridge, SnippetsBridge.this);
    }

    @Override
    public int[] getCategories() {
        assert mNativeSnippetsBridge != 0;
        return SnippetsBridgeJni.get().getCategories(mNativeSnippetsBridge, SnippetsBridge.this);
    }

    @Override
    @CategoryStatus
    public int getCategoryStatus(int category) {
        assert mNativeSnippetsBridge != 0;
        return SnippetsBridgeJni.get().getCategoryStatus(
                mNativeSnippetsBridge, SnippetsBridge.this, category);
    }

    @Override
    public SuggestionsCategoryInfo getCategoryInfo(int category) {
        assert mNativeSnippetsBridge != 0;
        return SnippetsBridgeJni.get().getCategoryInfo(
                mNativeSnippetsBridge, SnippetsBridge.this, category);
    }

    @Override
    public List<SnippetArticle> getSuggestionsForCategory(int category) {
        assert mNativeSnippetsBridge != 0;
        return SnippetsBridgeJni.get().getSuggestionsForCategory(
                mNativeSnippetsBridge, SnippetsBridge.this, category);
    }

    @Override
    public void fetchSuggestionImage(SnippetArticle suggestion, Callback<Bitmap> callback) {
        assert mNativeSnippetsBridge != 0;
        SnippetsBridgeJni.get().fetchSuggestionImage(mNativeSnippetsBridge, SnippetsBridge.this,
                suggestion.mCategory, suggestion.mIdWithinCategory, callback);
    }

    @Override
    public void fetchSuggestionFavicon(SnippetArticle suggestion, int minimumSizePx,
            int desiredSizePx, Callback<Bitmap> callback) {
        assert mNativeSnippetsBridge != 0;
        SnippetsBridgeJni.get().fetchSuggestionFavicon(mNativeSnippetsBridge, SnippetsBridge.this,
                suggestion.mCategory, suggestion.mIdWithinCategory, minimumSizePx, desiredSizePx,
                callback);
    }

    @Override
    public void dismissSuggestion(SnippetArticle suggestion) {
        assert mNativeSnippetsBridge != 0;
        SnippetsBridgeJni.get().dismissSuggestion(mNativeSnippetsBridge, SnippetsBridge.this,
                suggestion.mUrl, suggestion.getGlobalRank(), suggestion.mCategory,
                suggestion.getPerSectionRank(), suggestion.mIdWithinCategory);
    }

    @Override
    public void dismissCategory(@CategoryInt int category) {
        assert mNativeSnippetsBridge != 0;
        SnippetsBridgeJni.get().dismissCategory(
                mNativeSnippetsBridge, SnippetsBridge.this, category);
    }

    @Override
    public void restoreDismissedCategories() {
        assert mNativeSnippetsBridge != 0;
        SnippetsBridgeJni.get().restoreDismissedCategories(
                mNativeSnippetsBridge, SnippetsBridge.this);
    }

    @Override
    public void addObserver(Observer observer) {
        assert observer != null;
        mObserverList.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObserverList.removeObserver(observer);
    }

    @Override
    public void fetchSuggestions(@CategoryInt int category, String[] displayedSuggestionIds,
            Callback<List<SnippetArticle>> successCallback, Runnable failureRunnable) {
        assert mNativeSnippetsBridge != 0;
        // We have nice JNI support for Callbacks but not for Runnables, so wrap the Runnable
        // in a Callback and discard the parameter.
        // TODO(peconn): Use a Runnable here if they get nice JNI support.
        SnippetsBridgeJni.get().fetch(mNativeSnippetsBridge, SnippetsBridge.this, category,
                displayedSuggestionIds, successCallback, ignored -> failureRunnable.run());
    }

    @CalledByNative
    private static List<SnippetArticle> createSuggestionList() {
        return new ArrayList<>();
    }

    @CalledByNative
    private static SnippetArticle addSuggestion(List<SnippetArticle> suggestions, int category,
            String id, String title, String publisher, String url, long timestamp, float score,
            long fetchTime, boolean isVideoSuggestion, int thumbnailDominantColor,
            boolean hasThumbnail) {
        int position = suggestions.size();
        // thumbnailDominantColor equal to 0 encodes absence of the value. 0 is not a valid color,
        // because the passed color cannot be fully transparent.
        suggestions.add(new SnippetArticle(category, id, title, /*snippet=*/"", publisher, url,
                timestamp, score, fetchTime, isVideoSuggestion,
                thumbnailDominantColor == 0 ? null : thumbnailDominantColor, hasThumbnail));
        return suggestions.get(position);
    }

    @CalledByNative
    private static SuggestionsCategoryInfo createSuggestionsCategoryInfo(int category, String title,
            @ContentSuggestionsCardLayout int cardLayout,
            @ContentSuggestionsAdditionalAction int additionalAction, boolean showIfEmpty,
            String noSuggestionsMessage) {
        return new SuggestionsCategoryInfo(
                category, title, cardLayout, additionalAction, showIfEmpty, noSuggestionsMessage);
    }

    @CalledByNative
    private void onNewSuggestions(@CategoryInt int category) {
        for (Observer observer : mObserverList) observer.onNewSuggestions(category);
    }

    @CalledByNative
    private void onCategoryStatusChanged(@CategoryInt int category, @CategoryStatus int newStatus) {
        for (Observer observer : mObserverList) {
            observer.onCategoryStatusChanged(category, newStatus);
        }
    }

    @CalledByNative
    private void onSuggestionInvalidated(@CategoryInt int category, String idWithinCategory) {
        for (Observer observer : mObserverList) {
            observer.onSuggestionInvalidated(category, idWithinCategory);
        }
    }

    @CalledByNative
    private void onFullRefreshRequired() {
        for (Observer observer : mObserverList) observer.onFullRefreshRequired();
    }

    @CalledByNative
    private void onSuggestionsVisibilityChanged(@CategoryInt int category) {
        for (Observer observer : mObserverList) observer.onSuggestionsVisibilityChanged(category);
    }

    @NativeMethods
    interface Natives {
        long init(SnippetsBridge caller, Profile profile);
        void destroy(long nativeNTPSnippetsBridge, SnippetsBridge caller);
        void reloadSuggestions(long nativeNTPSnippetsBridge, SnippetsBridge caller);
        void remoteSuggestionsSchedulerOnPersistentSchedulerWakeUp();
        void remoteSuggestionsSchedulerOnBrowserUpgraded();
        boolean areRemoteSuggestionsEnabled(long nativeNTPSnippetsBridge, SnippetsBridge caller);
        int[] getCategories(long nativeNTPSnippetsBridge, SnippetsBridge caller);
        int getCategoryStatus(long nativeNTPSnippetsBridge, SnippetsBridge caller, int category);
        SuggestionsCategoryInfo getCategoryInfo(
                long nativeNTPSnippetsBridge, SnippetsBridge caller, int category);
        List<SnippetArticle> getSuggestionsForCategory(
                long nativeNTPSnippetsBridge, SnippetsBridge caller, int category);
        void fetchSuggestionImage(long nativeNTPSnippetsBridge, SnippetsBridge caller, int category,
                String idWithinCategory, Callback<Bitmap> callback);
        void fetchSuggestionFavicon(long nativeNTPSnippetsBridge, SnippetsBridge caller,
                int category, String idWithinCategory, int minimumSizePx, int desiredSizePx,
                Callback<Bitmap> callback);
        void fetch(long nativeNTPSnippetsBridge, SnippetsBridge caller, int category,
                String[] knownSuggestions, Callback<List<SnippetArticle>> successCallback,
                Callback<Integer> failureCallback);
        void dismissSuggestion(long nativeNTPSnippetsBridge, SnippetsBridge caller, String url,
                int globalPosition, int category, int positionInCategory, String idWithinCategory);
        void dismissCategory(long nativeNTPSnippetsBridge, SnippetsBridge caller, int category);
        void restoreDismissedCategories(long nativeNTPSnippetsBridge, SnippetsBridge caller);
    }
}
