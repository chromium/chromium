// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.common;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
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

    /** Tracks whether the list is currently expanded to show all hidden items. */
    private boolean mIsExpanded;

    /**
     * The items to display when the list is expanded. Subclasses are responsible for populating
     * this list during {@link #refreshList()} or lazy-loading them via {@link
     * #prepareHiddenItemsIfNeeded()}.
     */
    private final List<ListItem> mHiddenItems = new ArrayList<>();

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

    @Override
    public void onTemplateURLServiceChanged() {
        mIsExpanded = false;
        // Triggers refreshList() in the base class.
        super.onTemplateURLServiceChanged();
    }

    public boolean isExpandedForTesting() {
        return mIsExpanded;
    }

    /** Clears all items currently staged in the hidden items list. */
    protected void clearHiddenItems() {
        mHiddenItems.clear();
    }

    /**
     * Adds a new item to the hidden items list, which will be displayed when the user clicks
     * "More".
     *
     * @param item The {@link ListItem} to append to the hidden list.
     */
    protected void addHiddenItem(ListItem item) {
        mHiddenItems.add(item);
    }

    /**
     * Checks if there are no items currently staged in the hidden items list.
     *
     * @return True if the hidden items list is empty, false otherwise.
     */
    protected boolean areHiddenItemsEmpty() {
        return mHiddenItems.isEmpty();
    }

    /**
     * Evaluates whether a "More" button is needed and appends it to the ModelList if so. Subclasses
     * should call this at the end of their {@link #refreshList()} implementation.
     *
     * @param numUrls The total number of available URLs for this category.
     */
    protected void setUpMoreButtonIfNeeded(int numUrls) {
        if (numUrls <= DEFAULT_MAX_ROWS) {
            return;
        }
        PropertyModel moreButtonModel =
                new PropertyModel.Builder(SiteSearchProperties.ALL_KEYS)
                        .with(SiteSearchProperties.IS_EXPANDED, mIsExpanded)
                        .build();
        moreButtonModel.set(
                SiteSearchProperties.ON_CLICK, v -> onMoreButtonClicked(moreButtonModel));
        mModelList.add(new ListItem(SiteSearchProperties.ViewType.MORE, moreButtonModel));
    }

    /**
     * Handles the click event on the "More" button, toggling the expanded state and updating the
     * ModelList accordingly.
     *
     * @param moreButtonModel The property model of the clicked button to update its UI state.
     */
    protected void onMoreButtonClicked(PropertyModel moreButtonModel) {
        // TODO: We're using a single RecyclerView + ModelList to display the list + More button +
        // the hidden list items. Consider splitting into one RecyclerView for the list + More
        // button and another RecyclerView for the hidden list items.
        mIsExpanded = !mIsExpanded;
        moreButtonModel.set(SiteSearchProperties.IS_EXPANDED, mIsExpanded);

        if (mIsExpanded) {
            prepareHiddenItemsIfNeeded();
            mModelList.addAll(mHiddenItems);
        } else {
            // Dynamically calculate where the hidden items start by removing them from the tail.
            // This safely accounts for any buttons (Add, More) that might precede the hidden items.
            int startIndex = mModelList.size() - mHiddenItems.size();
            mModelList.removeRange(startIndex, mHiddenItems.size());
        }
    }

    /**
     * A lifecycle hook called right before the hidden items are added to the list. Subclasses can
     * override this to lazy-load their hidden ListItems to save memory and processing time during
     * the initial layout.
     */
    protected void prepareHiddenItemsIfNeeded() {}
}
