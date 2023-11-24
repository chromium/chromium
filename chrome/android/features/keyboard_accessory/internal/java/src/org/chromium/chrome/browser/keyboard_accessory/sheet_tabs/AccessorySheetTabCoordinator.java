// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabProperties.IS_DEFAULT_A11Y_FOCUS_REQUESTED;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabProperties.ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabProperties.SCROLL_LISTENER;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.ViewGroup;

import androidx.annotation.CallSuper;
import androidx.annotation.DrawableRes;
import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.keyboard_accessory.data.Provider;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * This coordinator aims to be the base class for sheets to be added to the ManualFillingCoordinator
 * It mainly enforces a consistent use of scroll listeners in {@link RecyclerView}s.
 */
public abstract class AccessorySheetTabCoordinator implements KeyboardAccessoryData.Tab.Listener {
    private final KeyboardAccessoryData.Tab mTab;
    private final RecyclerView.OnScrollListener mScrollListener;

    protected final PropertyModel mModel;

    /** Provides the icon used for a sheet. Simplifies mocking in controller tests. */
    public static class IconProvider {
        private static Drawable sIconForTesting;

        /**
         * Loads the icon used for this class. Used to mock icons in unit tests.
         * @param context The context containing the icon resources.
         * @param resource The icon resources.
         * @return The icon as {@link Drawable}.
         */
        static Drawable getIcon(Context context, @DrawableRes int resource) {
            if (sIconForTesting != null) return sIconForTesting;
            return AppCompatResources.getDrawable(context, resource);
        }

        public static void setIconForTesting(Drawable icon) {
            sIconForTesting = icon;
            ResettersForTesting.register(() -> sIconForTesting = null);
        }
    }

    /**
     * Creates a keyboard accessory sheet tab coordinator.
     * @param title A {@link String} permanently displayed in the bar above the keyboard.
     * @param icon The icon that represents this sheet in the keyboard accessory tab switcher.
     * @param contentDescription A description for this sheet used in the tab switcher.
     * @param layout The layout containing all views that are used by this sheet.
     * @param tabType The type of this tab as used in histograms.
     * @param scrollListener An optional listener that will be bound to an inflated recycler view.
     */
    AccessorySheetTabCoordinator(
            String title,
            Drawable icon,
            String contentDescription,
            @LayoutRes int layout,
            @AccessoryTabType int tabType,
            @Nullable RecyclerView.OnScrollListener scrollListener) {
        mTab =
                new KeyboardAccessoryData.Tab(
                        title, icon, contentDescription, layout, tabType, this);
        mScrollListener = scrollListener;
        mModel =
                new PropertyModel.Builder(AccessorySheetTabProperties.ALL_KEYS)
                        .with(ITEMS, new AccessorySheetTabItemsModel())
                        .with(SCROLL_LISTENER, scrollListener)
                        .with(IS_DEFAULT_A11Y_FOCUS_REQUESTED, false)
                        .build();
    }

    @CallSuper
    @Override
    public void onTabCreated(ViewGroup view) {
        AccessorySheetTabViewBinder.initializeView((RecyclerView) view, mScrollListener);

        PropertyModelChangeProcessor.create(
                mModel, (AccessorySheetTabView) view, AccessorySheetTabViewBinder::bind);
    }

    @CallSuper
    @Override
    public void onTabShown() {
        getMediator().onTabShown();
    }

    /**
     * Returns the Tab object that describes the appearance of this class in the keyboard accessory
     * or its accessory sheet. The returned object doesn't change for this instance.
     * @return Returns a stable {@link KeyboardAccessoryData.Tab} that is connected to this sheet.
     */
    public KeyboardAccessoryData.Tab getTab() {
        return mTab;
    }

    /**
     * The mediator that is used to process the data pushed from sources added with
     * {@link #registerDataProvider(Provider)}.
     * @return A {@link Provider.Observer<AccessorySheetData>}.
     */
    abstract AccessorySheetTabMediator getMediator();

    /**
     * Registers the provider pushing a complete new instance of {@link AccessorySheetData} that
     * should be displayed as sheet for this tab.
     * @param sheetDataProvider A {@link Provider <AccessorySheetData>}.
     */
    public void registerDataProvider(Provider<AccessorySheetData> sheetDataProvider) {
        sheetDataProvider.addObserver(getMediator());
    }

    AccessorySheetTabItemsModel getSheetDataPiecesForTesting() {
        return mModel.get(ITEMS);
    }
}
