// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_component;

import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.ACTIVE_TAB_INDEX;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.HEIGHT;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.NO_ACTIVE_TAB;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.PAGE_CHANGE_LISTENER;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.TABS;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.TOP_SHADOW_VISIBLE;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.VISIBLE;

import android.view.ViewStub;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.viewpager.widget.PagerAdapter;
import androidx.viewpager.widget.ViewPager;

import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.ui.DeferredViewStubInflationProvider;
import org.chromium.ui.ViewProvider;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Creates and owns all elements which are part of the accessory sheet component.
 * It's part of the controller but will mainly forward events (like showing the sheet) and handle
 * communication with the ManualFillingCoordinator (e.g. add a tab to trigger the sheet).
 * to the {@link AccessorySheetMediator}.
 */
public class AccessorySheetCoordinator {
    private final AccessorySheetMediator mMediator;

    /**
     * Creates the sheet component by instantiating Model, View and Controller before wiring these
     * parts up.
     * @param sheetStub A {@link ViewStub} for the accessory sheet layout.
     */
    public AccessorySheetCoordinator(ViewStub sheetStub) {
        this(new DeferredViewStubInflationProvider<>(sheetStub));
    }

    /**
     * Constructor that allows to mock the {@link DeferredViewStubInflationProvider}.
     * @param viewProvider A provider for the accessory.
     */
    @VisibleForTesting
    AccessorySheetCoordinator(ViewProvider<AccessorySheetView> viewProvider) {
        PropertyModel model = new PropertyModel
                                      .Builder(TABS, ACTIVE_TAB_INDEX, VISIBLE, HEIGHT,
                                              TOP_SHADOW_VISIBLE, PAGE_CHANGE_LISTENER)
                                      .with(TABS, new ListModel<>())
                                      .with(ACTIVE_TAB_INDEX, NO_ACTIVE_TAB)
                                      .with(VISIBLE, false)
                                      .with(TOP_SHADOW_VISIBLE, false)
                                      .build();

        LazyConstructionPropertyMcp.create(
                model, VISIBLE, viewProvider, AccessorySheetViewBinder::bind);

        AccessorySheetMetricsRecorder.registerAccessorySheetModelMetricsObserver(model);
        mMediator = new AccessorySheetMediator(model);
    }

    /**
     * Creates the {@link PagerAdapter} for the newly inflated {@link ViewPager}.
     * The created adapter observes the given model for item changes and updates the view pager.
     * @param tabList The list of tabs to be displayed.
     * @param viewPager The newly inflated {@link ViewPager}.
     * @return A fully initialized {@link PagerAdapter}.
     */
    static PagerAdapter createTabViewAdapter(
            ListModel<KeyboardAccessoryData.Tab> tabList, ViewPager viewPager) {
        AccessoryPagerAdapter adapter = new AccessoryPagerAdapter(tabList);
        tabList.addObserver(new ListModelChangeProcessor<>(tabList, viewPager, adapter));
        return adapter;
    }

    public void setTabs(KeyboardAccessoryData.Tab[] tabs) {
        mMediator.setTabs(tabs);
    }

    public RecyclerView.OnScrollListener getScrollListener() {
        return mMediator.getScrollListener();
    }

    /**
     * Returns a {@link KeyboardAccessoryData.Tab} object that is used to display this bottom sheet.
     * @return Returns a {@link KeyboardAccessoryData.Tab}.
     */
    @Nullable
    public KeyboardAccessoryData.Tab getTab() {
        return mMediator.getTab();
    }

    /**
     * Sets the height of the accessory sheet (i.e. adapts to keyboard heights).
     * @param height The height of the sheet in pixels.
     */
    public void setHeight(@Px int height) {
        mMediator.setHeight(height);
    }

    /**
     * Gets the height of the accessory sheet (even if not visible).
     * @return The height of the sheet in pixels.
     */
    public @Px int getHeight() {
        return mMediator.getHeight();
    }

    /**
     * Shows the Accessory Sheet.
     */
    public void show() {
        mMediator.show();
    }

    /**
     * Hides the Accessory Sheet.
     */
    public void hide() {
        mMediator.hide();
    }

    /**
     * Returns whether the accessory sheet is currently visible.
     * @return True, if the accessory sheet is visible.
     */
    public boolean isShown() {
        return mMediator.isShown();
    }

    /**
     * Calling this function changes the active tab to the tab at the given |position|.
     * @param position The index of the tab (starting with 0) that should be set active.
     */
    public void setActiveTab(int position) {
        mMediator.setActiveTab(position);
    }

    public void setOnPageChangeListener(ViewPager.OnPageChangeListener onPageChangeListener) {
        mMediator.setOnPageChangeListener(onPageChangeListener);
    }

    @VisibleForTesting
    AccessorySheetMediator getMediatorForTesting() {
        return mMediator;
    }
}
