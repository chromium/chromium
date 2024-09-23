// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_component;

import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.VISIBLE;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.viewpager.widget.PagerAdapter;
import androidx.viewpager.widget.ViewPager;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.keyboard_accessory.AccessorySheetVisualStateProvider;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.ui.AsyncViewProvider;
import org.chromium.ui.AsyncViewStub;
import org.chromium.ui.ViewProvider;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Creates and owns all elements which are part of the accessory sheet component. It's part of the
 * controller but will mainly forward events (like showing the sheet) and handle communication with
 * the ManualFillingCoordinator (e.g. add a tab to trigger the sheet). to the {@link
 * AccessorySheetMediator}.
 */
public class AccessorySheetCoordinator implements AccessorySheetVisualStateProvider {
    private final AccessorySheetMediator mMediator;

    /**
     * Describes the events that are emitted when an accessory sheet is closed / changed. A class
     * implementing this interface takes the responsibility control the sheet, i.e.
     * ManualFillingCoordinator.
     */
    public interface SheetVisibilityDelegate {
        /**
         * Is triggered when a tab in the accessory was selected and the sheet needs to change.
         * @param sheetIndex The index of the selected sheet in the sheet openers / tab bar.
         */
        void onChangeAccessorySheet(int sheetIndex);

        /** Called when the sheet needs to be hidden. */
        void onCloseAccessorySheet();
    }

    /**
     * Creates the sheet component by instantiating Model, View and Controller before wiring these
     * parts up.
     *
     * @param sheetStub A {@link AsyncViewStub} for the accessory sheet layout.
     */
    public AccessorySheetCoordinator(
            AsyncViewStub sheetStub, SheetVisibilityDelegate sheetVisibilityDelegate) {
        this(
                sheetStub.getContext(),
                AsyncViewProvider.of(sheetStub, R.id.keyboard_accessory_sheet_container),
                sheetVisibilityDelegate);
    }

    /**
     * Constructor that allows to mock the {@link AsyncViewProvider}.
     *
     * @param context The {@link Context} for accessing color resources.
     * @param viewProvider A provider for the accessory.
     */
    @VisibleForTesting
    AccessorySheetCoordinator(
            Context context,
            ViewProvider<AccessorySheetView> viewProvider,
            SheetVisibilityDelegate sheetVisibilityDelegate) {
        PropertyModel model = AccessorySheetProperties.defaultPropertyModel().build();

        LazyConstructionPropertyMcp.create(
                model, VISIBLE, viewProvider, AccessorySheetViewBinder::bind);

        AccessorySheetMetricsRecorder.registerAccessorySheetModelMetricsObserver(model);
        mMediator = new AccessorySheetMediator(context, model, sheetVisibilityDelegate);
    }

    /**
     * Creates the {@link PagerAdapter} for the newly inflated {@link ViewPager}. The created
     * adapter observes the given model for item changes and updates the view pager.
     *
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

    /** Shows the Accessory Sheet. */
    public void show() {
        TraceEvent.begin("AccessorySheetCoordinator#show");
        mMediator.show();
        TraceEvent.end("AccessorySheetCoordinator#show");
    }

    /** Hides the Accessory Sheet. */
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

    @Override
    public void addObserver(Observer observer) {
        mMediator.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mMediator.removeObserver(observer);
    }

    AccessorySheetMediator getMediatorForTesting() {
        return mMediator;
    }
}
