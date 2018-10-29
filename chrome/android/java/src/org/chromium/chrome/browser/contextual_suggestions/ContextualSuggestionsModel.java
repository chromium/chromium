// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.view.View.OnClickListener;

import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.modelutil.PropertyObservable;
import org.chromium.chrome.browser.widget.ListMenuButton;

import java.util.Arrays;
import java.util.Collection;
import java.util.List;

import javax.inject.Inject;

/** A model for the contextual suggestions UI component. */
@ActivityScope
class ContextualSuggestionsModel
        extends PropertyObservable<ContextualSuggestionsModel.PropertyKey> {
    /** Keys uniquely identifying model properties. */
    static class PropertyKey {
        static final PropertyKey CLOSE_BUTTON_ON_CLICK_LISTENER = new PropertyKey();
        static final PropertyKey MENU_BUTTON_DELEGATE = new PropertyKey();
        static final PropertyKey TITLE = new PropertyKey();
        static final PropertyKey TOOLBAR_SHADOW_VISIBILITY = new PropertyKey();

        private PropertyKey() {}
    }

    private final ClusterList mClusterList = new ClusterList();

    private OnClickListener mCloseButtonOnClickListener;
    private ListMenuButton.Delegate mMenuButtonDelegate;
    private String mTitle;
    private boolean mToolbarShadowVisibility;
    @Inject
    ContextualSuggestionsModel() {}

    @Override
    public Collection<PropertyKey> getAllSetProperties() {
        // This is only the list of initially set properties and doesn't reflect changes after the
        // object has been created. but currently this method is only called initially.
        // Once this model is migrated to PropertyModel, the implementation will be correct.
        return Arrays.asList(PropertyKey.CLOSE_BUTTON_ON_CLICK_LISTENER,
                PropertyKey.MENU_BUTTON_DELEGATE, PropertyKey.TITLE);
    }

    /** @param clusters The current list of clusters. */
    void setClusterList(List<ContextualSuggestionsCluster> clusters) {
        mClusterList.setClusters(clusters);
    }

    /** @return The current list of clusters. */
    ClusterList getClusterList() {
        return mClusterList;
    }

    /** @param listener The {@link OnClickListener} for the close button. */
    void setCloseButtonOnClickListener(OnClickListener listener) {
        mCloseButtonOnClickListener = listener;
        notifyPropertyChanged(PropertyKey.CLOSE_BUTTON_ON_CLICK_LISTENER);
    }

    /** @return The {@link OnClickListener} for the close button. */
    OnClickListener getCloseButtonOnClickListener() {
        return mCloseButtonOnClickListener;
    }

    /** @param delegate The delegate for handles actions for the menu. */
    void setMenuButtonDelegate(ListMenuButton.Delegate delegate) {
        mMenuButtonDelegate = delegate;
        notifyPropertyChanged(PropertyKey.MENU_BUTTON_DELEGATE);
    }

    /** @return The delegate that handles actions for the menu. */
    ListMenuButton.Delegate getMenuButtonDelegate() {
        return mMenuButtonDelegate;
    }

    /** @param title The title to display in the toolbar. */
    void setTitle(String title) {
        mTitle = title;
        notifyPropertyChanged(PropertyKey.TITLE);
    }

    /** @return title The title to display in the toolbar. */
    String getTitle() {
        return mTitle;
    }

    /**
     * @return Whether there are any suggestions to be shown.
     */
    boolean hasSuggestions() {
        return getClusterList().getItemCount() > 0;
    }

    /** @param visible Whether the toolbar shadow should be visible. */
    void setToolbarShadowVisibility(boolean visible) {
        mToolbarShadowVisibility = visible;
        notifyPropertyChanged(PropertyKey.TOOLBAR_SHADOW_VISIBILITY);
    }

    /** @return Whether the toolbar shadow should be visible. */
    boolean getToolbarShadowVisibility() {
        return mToolbarShadowVisibility;
    }
}
