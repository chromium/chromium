// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Objects;

/** Defines a toolbar button to add the current web page to bookmarks. */
public class AddToBookmarksToolbarButtonController extends BaseButtonDataProvider
        implements ConfigurationChangedObserver {
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final Supplier<TabBookmarker> mTabBookmarkerSupplier;
    private final Supplier<Tracker> mTrackerSupplier;
    private final ObservableSupplier<BookmarkModel> mBookmarkModelSupplier;
    private final ButtonSpec mFilledButtonSpec;
    private final ButtonSpec mEmptyButtonSpec;
    private final Context mContext;
    private CurrentTabObserver mCurrentTabObserver;
    private BookmarkModel mObservedBookmarkModel;
    private boolean mIsTablet;

    private final Callback<BookmarkModel> mBookmarkModelSupplierObserver =
            new Callback<BookmarkModel>() {
                @Override
                public void onResult(BookmarkModel result) {
                    if (mObservedBookmarkModel != null) {
                        mObservedBookmarkModel.removeObserver(mBookmarkModelObserver);
                    }

                    mObservedBookmarkModel = result;
                    if (mObservedBookmarkModel != null) {
                        mObservedBookmarkModel.addObserver(mBookmarkModelObserver);
                    }
                }
            };

    private final BookmarkModelObserver mBookmarkModelObserver =
            new BookmarkModelObserver() {
                @Override
                public void bookmarkModelChanged() {
                    refreshBookmarkIcon();
                }
            };

    /**
     * Creates a new instance of {@code AddToBookmarksToolbarButtonController}
     *
     * @param activeTabSupplier Supplier for the current active tab.
     * @param context Android context, used to retrieve resources.
     * @param tabBookmarkerSupplier Supplier of a {@code TabBookmarker} instance.
     * @param trackerSupplier Supplier for the current profile tracker. Used for IPH.
     * @param bookmarkModelSupplier Supplier for bookmark model, used to observe for bookmark
     *     changes and checking if the current tab is bookmarked.
     */
    public AddToBookmarksToolbarButtonController(
            ObservableSupplier<Tab> activeTabSupplier,
            Context context,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            Supplier<TabBookmarker> tabBookmarkerSupplier,
            Supplier<Tracker> trackerSupplier,
            ObservableSupplier<BookmarkModel> bookmarkModelSupplier) {
        // By default use the empty star drawable with an "Add to bookmarks" description.
        super(
                activeTabSupplier,
                /* modalDialogManager= */ null,
                AppCompatResources.getDrawable(context, R.drawable.star_outline_24dp),
                context.getString(R.string.accessibility_menu_bookmark),
                /* actionChipLabelResId= */ Resources.ID_NULL,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS,
                /* tooltipTextResId= */ Resources.ID_NULL,
                /* showHoverHighlight= */ true);
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mTabBookmarkerSupplier = tabBookmarkerSupplier;
        mTrackerSupplier = trackerSupplier;
        mContext = context;

        mBookmarkModelSupplier = bookmarkModelSupplier;
        mActivityLifecycleDispatcher.register(this);
        mBookmarkModelSupplier.addObserver(mBookmarkModelSupplierObserver);
        mCurrentTabObserver =
                new CurrentTabObserver(
                        activeTabSupplier,
                        new EmptyTabObserver() {
                            @Override
                            public void onUrlUpdated(Tab tab) {
                                refreshBookmarkIcon();
                            }
                        },
                        result -> refreshBookmarkIcon());

        mEmptyButtonSpec = mButtonData.getButtonSpec();
        // Create another ButtonSpec with a filled star icon and a "Edit bookmark" description.
        mFilledButtonSpec =
                new ButtonSpec(
                        AppCompatResources.getDrawable(context, R.drawable.btn_star_filled),
                        this,
                        null,
                        context.getString(R.string.menu_edit_bookmark),
                        true,
                        /* iphCommandBuilder= */ null,
                        AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS,
                        /* actionChipLabelResId= */ Resources.ID_NULL,
                        /* tooltipTextResId= */ Resources.ID_NULL,
                        /* showHoverHighlight= */ true);

        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
    }

    private void refreshBookmarkIcon() {
        if (!mActiveTabSupplier.hasValue()
                || !mBookmarkModelSupplier.hasValue()
                || !mBookmarkModelSupplier.get().isBookmarkModelLoaded()) {
            return;
        }

        boolean isCurrentTabBookmarked =
                mBookmarkModelSupplier.get().hasBookmarkIdForTab(mActiveTabSupplier.get());
        ButtonSpec buttonSpecForCurrentTab =
                isCurrentTabBookmarked ? mFilledButtonSpec : mEmptyButtonSpec;
        if (!Objects.equals(mButtonData.getButtonSpec(), buttonSpecForCurrentTab)) {
            mButtonData.setButtonSpec(buttonSpecForCurrentTab);
            notifyObservers(mButtonData.canShow());
        }
    }

    @Override
    protected boolean shouldShowButton(Tab tab) {
        if (mIsTablet) return false;

        return super.shouldShowButton(tab);
    }

    @Override
    protected IPHCommandBuilder getIphCommandBuilder(Tab tab) {
        return new IPHCommandBuilder(
                tab.getContext().getResources(),
                FeatureConstants
                        .ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_ADD_TO_BOOKMARKS_FEATURE,
                /* stringId= */ R.string.adaptive_toolbar_button_add_to_bookmarks_iph,
                /* accessibilityStringId= */ R.string.adaptive_toolbar_button_add_to_bookmarks_iph);
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        if (mIsTablet == isTablet) {
            return;
        }
        mIsTablet = isTablet;

        mButtonData.setCanShow(shouldShowButton(mActiveTabSupplier.get()));
    }

    @Override
    public void onClick(View view) {
        if (!mTabBookmarkerSupplier.hasValue() || !mActiveTabSupplier.hasValue()) return;

        if (mTrackerSupplier.hasValue()) {
            mTrackerSupplier
                    .get()
                    .notifyEvent(
                            EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_ADD_TO_BOOKMARKS_OPENED);
        }

        RecordUserAction.record("MobileTopToolbarAddToBookmarksButton");
        mTabBookmarkerSupplier.get().addOrEditBookmark(mActiveTabSupplier.get());
    }

    @Override
    public void destroy() {
        if (mObservedBookmarkModel != null) {
            mObservedBookmarkModel.removeObserver(mBookmarkModelObserver);
            mObservedBookmarkModel = null;
        }

        if (mBookmarkModelSupplier != null) {
            mBookmarkModelSupplier.removeObserver(mBookmarkModelSupplierObserver);
        }

        if (mCurrentTabObserver != null) {
            mCurrentTabObserver.destroy();
            mCurrentTabObserver = null;
        }

        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
        }

        super.destroy();
    }
}
