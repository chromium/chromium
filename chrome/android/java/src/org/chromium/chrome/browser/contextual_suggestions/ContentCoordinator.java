// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.OnScrollListener;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.ui.base.WindowAndroid;

/**
 * Coordinator for the content sub-component. Responsible for communication with the parent
 * {@link ContextualSuggestionsCoordinator} and lifecycle of sub-component objects.
 */
class ContentCoordinator {
    private static final String CONTEXT_MENU_USER_ACTION_PREFIX = "ContextualSuggestions";
    private final SuggestionsRecyclerView mRecyclerView;

    private ContextualSuggestionsModel mModel;
    private WindowAndroid mWindowAndroid;
    private ContextMenuManager mContextMenuManager;
    private ContextualSuggestionsAdapter mAdapter;

    /**
     * Construct a new {@link ContentCoordinator}.
     * @param context The {@link Context} used to retrieve resources.
     * @param parentView The parent {@link View} to which the content will eventually be attached.
     */
    ContentCoordinator(Context context, ViewGroup parentView) {
        mRecyclerView = (SuggestionsRecyclerView) LayoutInflater.from(context).inflate(
                R.layout.contextual_suggestions_layout, parentView, false);
    }

    /** @return The content {@link View}. */
    View getView() {
        return mRecyclerView;
    }

    /** @return The vertical scroll offset of the content view. */
    int getVerticalScrollOffset() {
        return mRecyclerView.computeVerticalScrollOffset();
    }

    /**
     * Show suggestions, retrieved from the model, in the content view.
     *
     * @param context The {@link Context} used to retrieve resources.
     * @param profile The regular {@link Profile}.
     * @param uiDelegate The {@link SuggestionsUiDelegate} used to help construct items in the
     *                   content view.
     * @param model The {@link ContextualSuggestionsModel} for the component.
     * @param windowAndroid The {@link WindowAndroid} for attaching a context menu listener.
     * @param closeContextMenuCallback The callback when a context menu is closed.
     */
    void showSuggestions(Context context, Profile profile, SuggestionsUiDelegate uiDelegate,
            ContextualSuggestionsModel model, WindowAndroid windowAndroid,
            Runnable closeContextMenuCallback) {
        mModel = model;
        mWindowAndroid = windowAndroid;

        mContextMenuManager = new ContextMenuManager(uiDelegate.getNavigationDelegate(),
                mRecyclerView::setTouchEnabled, closeContextMenuCallback,
                CONTEXT_MENU_USER_ACTION_PREFIX);
        mWindowAndroid.addContextMenuCloseListener(mContextMenuManager);

        ClusterList clusterList = mModel.getClusterList();
        mAdapter = new ContextualSuggestionsAdapter(
                profile, new UiConfig(mRecyclerView), uiDelegate, mContextMenuManager, clusterList);
        mRecyclerView.setAdapter(mAdapter);

        // TODO(twellington): Should this be a proper model property, set by the mediator and bound
        // to the RecyclerView?
        mRecyclerView.addOnScrollListener(new OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                mModel.setToolbarShadowVisibility(mRecyclerView.canScrollVertically(-1));
            }
        });
    }

    /** Destroy the content component. */
    void destroy() {
        // The model outlives the content sub-component. Remove the observer so that this object
        // can be garbage collected.
        if (mAdapter != null) {
            mModel.getClusterList().removeObserver(mAdapter);
        }
        if (mWindowAndroid != null) {
            mWindowAndroid.removeContextMenuCloseListener(mContextMenuManager);
        }
    }
}
