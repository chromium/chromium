// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.common;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * An extension of {@link BaseSiteSearchMediator} that provides built-in expansion logic.
 * Automatically handles the state of a "More" button and toggling hidden items in the list.
 */
@NullMarked
public abstract class ExpandableSiteSearchMediator extends BaseSiteSearchMediator {
    /** The default maximum number of items to show before displaying a "More" button. */
    protected static final int DEFAULT_MAX_ROWS = 5;

    /** Tracks whether the list is currently expanded to show all items. */
    private boolean mIsExpanded;

    /** Staged URLs that are lazy-loaded into mExpandableItems when the list is expanded. */
    private final List<TemplateUrl> mStagedUrls = new ArrayList<>();

    /**
     * The items to display when the list is expanded. These are controlled by {@link
     * #onMoreButtonClicked(PropertyModel)}.
     */
    private final List<ListItem> mExpandableItems = new ArrayList<>();

    /**
     * Constructs the expandable mediator.
     *
     * @param context The current context.
     * @param modelList The UI {@link ModelList}.
     * @param profile The current user {@link Profile}.
     */
    public ExpandableSiteSearchMediator(Context context, ModelList modelList, Profile profile) {
        super(context, modelList, profile);
    }

    public boolean isExpandedForTesting() {
        return mIsExpanded;
    }

    /**
     * Clears all items from the model list, the expandable items list, and the staged urls list.
     */
    protected void clearAllItems() {
        mModelList.clear();
        mExpandableItems.clear();
        mStagedUrls.clear();
    }

    /**
     * Populates the model list with the given TemplateUrls, adding the default maximum number of
     * rows directly and staging the remaining URLs for lazy-loading.
     *
     * @param urls The list of TemplateUrls to populate the model list with.
     */
    protected void populateTemplateUrls(List<TemplateUrl> urls) {
        for (int i = 0; i < urls.size(); i++) {
            TemplateUrl url = urls.get(i);
            if (i < DEFAULT_MAX_ROWS) {
                mModelList.add(createListItem(url));
            } else {
                mStagedUrls.add(url);
            }
        }
    }

    /**
     * Evaluates whether a "More" button is needed and appends it to the ModelList if so. Subclasses
     * should call this at the end of their {@link #refreshList()} implementation.
     *
     * @param text The text to display on the "More" button.
     */
    protected void setUpMoreButtonIfNeeded(String text) {
        if (mStagedUrls.isEmpty()) {
            // If the user originally had more than DEFAULT_MAX_ROWS items and the state is
            // expanded, after refresh, the number of items becomes less than or equal to
            // DEFAULT_MAX_ROWS (mStagedUrls thus becomes empty), we should collapse the list.
            mIsExpanded = false;
            return;
        }
        PropertyModel moreButtonModel =
                new PropertyModel.Builder(SiteSearchProperties.ALL_KEYS)
                        .with(SiteSearchProperties.IS_EXPANDED, mIsExpanded)
                        .with(SiteSearchProperties.TEXT, text)
                        .build();
        moreButtonModel.set(
                SiteSearchProperties.ON_CLICK, v -> onMoreButtonClicked(moreButtonModel));
        mModelList.add(new ListItem(SiteSearchProperties.ViewType.MORE, moreButtonModel));
    }

    /**
     * Checks the current expanded state and expands the list if necessary. This method is typically
     * called by subclasses after refreshList() or when handling click events that toggle the list's
     * expanded state.
     */
    protected void maybeExpandListFromPreviousState() {
        if (mIsExpanded) {
            prepareExpandableItemsIfNeeded();
            maybeAddExpandableItemsToModelList();
        }
    }

    /**
     * Handles the click event on the "More" button, toggling the expanded state and updating the
     * ModelList accordingly.
     *
     * @param moreButtonModel The property model of the clicked button to update its UI state.
     */
    @VisibleForTesting
    void onMoreButtonClicked(PropertyModel moreButtonModel) {
        // TODO: We're using a single RecyclerView + ModelList to display the list + More button +
        // the expandable list items. Consider splitting into one RecyclerView for the list + More
        // button and another RecyclerView for the expandable list items.
        mIsExpanded = !mIsExpanded;
        moreButtonModel.set(SiteSearchProperties.IS_EXPANDED, mIsExpanded);

        if (mIsExpanded) {
            maybeExpandListFromPreviousState();
        } else {
            // Dynamically calculate where the expandable items start by removing them from the
            // tail. This safely accounts for any buttons (Add, More) that might precede the
            // expandable items.
            int startIndex = mModelList.size() - mExpandableItems.size();
            mModelList.removeRange(startIndex, mExpandableItems.size());
        }
        updatePositions(mModelList);
    }

    /**
     * Lazy-loads the staged URLs into list items when the list is expanded for the first time.
     * Clears the staged URLs afterwards to free up memory.
     */
    private void prepareExpandableItemsIfNeeded() {
        if (mExpandableItems.isEmpty()) {
            for (TemplateUrl url : mStagedUrls) {
                mExpandableItems.add(createListItem(url));
            }
            mStagedUrls.clear();
        }
    }

    private void maybeAddExpandableItemsToModelList() {
        if (!mExpandableItems.isEmpty()) {
            mModelList.addAll(mExpandableItems);
        }
    }

    void addExpandableItemForTesting(ListItem item) {
        mExpandableItems.add(item);
    }

    void addStagedUrlForTesting(TemplateUrl url) {
        mStagedUrls.add(url);
    }

    boolean areExpandableItemsEmptyForTesting() {
        return mExpandableItems.isEmpty();
    }

    boolean areStagedUrlsEmptyForTesting() {
        return mStagedUrls.isEmpty();
    }
}
