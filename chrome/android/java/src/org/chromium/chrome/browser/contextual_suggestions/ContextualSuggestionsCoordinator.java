// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.content.Intent;
import android.support.annotation.Nullable;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.preferences.ContextualSuggestionsPreference;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegateImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetObserver;

import javax.inject.Inject;

/**
 * The coordinator for the contextual suggestions UI component. Manages communication with other
 * parts of the UI-layer and lifecycle of shared component objects.
 *
 * This parent coordinator manages two sub-components, controlled by {@link ContentCoordinator}
 * and {@link ToolbarCoordinator}. These sub-components each have their own views and view binders.
 * They share a {@link ContextualSuggestionsMediator} and {@link ContextualSuggestionsModel}.
 */
@ActivityScope
public class ContextualSuggestionsCoordinator implements Destroyable {
    private static final String FEEDBACK_CONTEXT = "contextual_suggestions";

    private final Profile mProfile = Profile.getLastUsedProfile().getOriginalProfile();
    private final ContextualSuggestionsModel mModel;
    private final ChromeActivity mActivity;
    private final BottomSheetController mBottomSheetController;
    private final TabModelSelector mTabModelSelector;
    private final ContextualSuggestionsMediator mMediator;

    private @Nullable ToolbarCoordinator mToolbarCoordinator;
    private @Nullable ContentCoordinator mContentCoordinator;
    private @Nullable ContextualSuggestionsBottomSheetContent mBottomSheetContent;

    /**
     * @param activity The containing {@link ChromeActivity}.
     * @param bottomSheetController The {@link BottomSheetController} to request content be shown.
     * @param tabModelSelector The {@link TabModelSelector} for the activity.
     * @param model The model of the component.
     * @param mediator The mediator of the component
     */
    @Inject
    ContextualSuggestionsCoordinator(ChromeActivity activity,
            BottomSheetController bottomSheetController, TabModelSelector tabModelSelector,
            ContextualSuggestionsModel model, ContextualSuggestionsMediator mediator,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mActivity = activity;
        mModel = model;
        mBottomSheetController = bottomSheetController;
        mTabModelSelector = tabModelSelector;
        mMediator = mediator;
        mediator.initialize(this);
        lifecycleDispatcher.register(this);
    }

    /** Called when the containing activity is destroyed. */
    @Override
    public void destroy() {
        mModel.getClusterList().destroy();
        mMediator.destroy();

        if (mToolbarCoordinator != null) mToolbarCoordinator.destroy();
        if (mContentCoordinator != null) mContentCoordinator.destroy();
        if (mBottomSheetContent != null) mBottomSheetContent.destroy();
    }

    /**
     * Show the contextual suggestions content in the {@link BottomSheet}.
     * Only the views needed for peeking the bottom sheet will be constructed. Another
     * call to {@link #displaySuggestions()} is needed to finish inflating views for the
     * suggestions cards.
     */
    void showContentInSheet() {
        mToolbarCoordinator =
                new ToolbarCoordinator(mActivity, mBottomSheetController.getBottomSheet(), mModel);
        mContentCoordinator =
                new ContentCoordinator(mActivity, mBottomSheetController.getBottomSheet());
        mBottomSheetContent = new ContextualSuggestionsBottomSheetContent(
                mContentCoordinator, mToolbarCoordinator);
        assert mBottomSheetContent != null;
        mBottomSheetController.requestShowContent(mBottomSheetContent, false);
    }

    /**
     * Finish showing the contextual suggestions in the {@link BottomSheet}.
     * {@link #showContentInSheet()} must be called prior to calling this method.
     *
     * @param suggestionsSource The {@link ContextualSuggestionsSource} used to retrieve additional
     *                          things needed to display suggestions (e.g. favicons, thumbnails).
     */
    void showSuggestions(ContextualSuggestionsSource suggestionsSource) {
        // If the content coordinator has already been destroyed when this method is called, return
        // early. See https://crbug.com/873052.
        if (mContentCoordinator == null) {
            assert false : "ContentCoordinator false when #showSuggestions was called.";
            return;
        }

        SuggestionsNavigationDelegate navigationDelegate = new SuggestionsNavigationDelegate(
                mActivity, mProfile, mBottomSheetController.getBottomSheet(), mTabModelSelector);
        SuggestionsUiDelegateImpl uiDelegate = new SuggestionsUiDelegateImpl(suggestionsSource,
                new ContextualSuggestionsEventReporter(mTabModelSelector, suggestionsSource),
                navigationDelegate, mProfile, mBottomSheetController.getBottomSheet(),
                mActivity.getChromeApplication().getReferencePool(),
                mBottomSheetController.getSnackbarManager());

        mContentCoordinator.showSuggestions(mActivity, mProfile, uiDelegate, mModel,
                mActivity.getWindowAndroid(), mActivity::closeContextMenu);
    }

    /**
     * Add an observer to the {@link BottomSheet}.
     * @param observer The observer to add.
     */
    void addBottomSheetObserver(BottomSheetObserver observer) {
        mBottomSheetController.getBottomSheet().addObserver(observer);
    }

    /**
     * Remove an observer to the {@link BottomSheet}.
     * @param observer The observer to remove.
     */
    void removeBottomSheetObserver(BottomSheetObserver observer) {
        mBottomSheetController.getBottomSheet().removeObserver(observer);
    }

    /**
     * Expand the {@link BottomSheet}.
     */
    void expandBottomSheet() {
        mBottomSheetController.expandSheet();
    }

    /** Removes contextual suggestions from the {@link BottomSheet}. */
    void removeSuggestions() {
        if (mToolbarCoordinator != null) {
            mToolbarCoordinator.destroy();
            mToolbarCoordinator = null;
        }

        if (mContentCoordinator != null) {
            mContentCoordinator.destroy();
            mContentCoordinator = null;
        }

        if (mBottomSheetContent != null) {
            mBottomSheetController.hideContent(mBottomSheetContent, true);
            mBottomSheetContent.destroy();
            mBottomSheetContent = null;
        }
    }

    /** Show the settings page for contextual suggestions. */
    void showSettings() {
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                mActivity, ContextualSuggestionsPreference.class.getName());
        IntentUtils.safeStartActivity(mActivity, intent);
    }

    /** Show the feedback page. */
    void showFeedback() {
        Tab currentTab = mActivity.getActivityTab();
        HelpAndFeedback.getInstance(mActivity).showFeedback(mActivity, mProfile,
                currentTab != null ? currentTab.getUrl() : null, null, FEEDBACK_CONTEXT);
    }

    /** @return The height of the bottom sheet when it's peeking. */
    float getSheetPeekHeight() {
        return mActivity.getBottomSheet().getSheetHeightForState(BottomSheet.SheetState.PEEK);
    }

    @VisibleForTesting
    ContextualSuggestionsMediator getMediatorForTesting() {
        return mMediator;
    }

    @VisibleForTesting
    ContextualSuggestionsModel getModelForTesting() {
        return mModel;
    }
}
