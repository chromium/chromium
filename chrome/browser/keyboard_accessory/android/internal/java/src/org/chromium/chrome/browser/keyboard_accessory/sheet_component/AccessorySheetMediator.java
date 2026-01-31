// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_component;

import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.ACTIVE_TAB_INDEX;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.BACKGROUND;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.BAR_SHADOW_VISIBLE;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.ELEVATION;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.GRAVITY;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.HEIGHT;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.HORIZONTAL_PADDING;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.MAX_WIDTH;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.NO_ACTIVE_TAB;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.PAGE_CHANGE_LISTENER;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.SHOW_KEYBOARD_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.TABS;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.TOP_OFFSET;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.TOP_SHADOW_VISIBLE;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.VISIBLE;

import android.content.Context;
import android.content.res.Resources;
import android.view.Gravity;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.viewpager.widget.ViewPager;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.keyboard_accessory.AccessorySheetTrigger;
import org.chromium.chrome.browser.keyboard_accessory.AccessorySheetVisualStateProvider;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetCoordinator.SheetVisibilityDelegate;
import org.chromium.chrome.browser.keyboard_accessory.utils.ManualFillingMetricsRecorder;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;

import java.util.function.Supplier;

/**
 * Contains the controller logic of the AccessorySheet component. It communicates with data
 * providers and native backends to update a model based on {@link AccessorySheetProperties}.
 */
@NullMarked
class AccessorySheetMediator implements PropertyObservable.PropertyObserver<PropertyKey> {
    private final PropertyModel mModel;
    private final RecyclerView.OnScrollListener mScrollListener =
            new RecyclerView.OnScrollListener() {
                @Override
                public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                    if (recyclerView == null) return;
                    mModel.set(TOP_SHADOW_VISIBLE, recyclerView.canScrollVertically(-1));
                }
            };
    private final SheetVisibilityDelegate mSheetVisibilityDelegate;
    private final Context mContext;
    private final ObserverList<AccessorySheetVisualStateProvider.Observer> mVisualObservers =
            new ObserverList<>();
    private @Nullable Supplier<Integer> mContentOffsetSupplier;

    AccessorySheetMediator(
            Context context, PropertyModel model, SheetVisibilityDelegate sheetVisibilityDelegate) {
        mContext = context;
        mModel = model;
        mModel.addObserver(this);
        mSheetVisibilityDelegate = sheetVisibilityDelegate;
        mModel.set(SHOW_KEYBOARD_CALLBACK, this::onKeyboardRequested);
    }

    KeyboardAccessoryData.@Nullable Tab getTab() {
        if (mModel.get(ACTIVE_TAB_INDEX) == NO_ACTIVE_TAB) return null;
        return mModel.get(TABS).get(mModel.get(ACTIVE_TAB_INDEX));
    }

    PropertyModel getModelForTesting() {
        return mModel;
    }

    void show() {
        mModel.set(VISIBLE, true);
    }

    void setHeight(int height) {
        mModel.set(AccessorySheetProperties.HEIGHT, height);
    }

    @Px
    int getHeight() {
        return mModel.get(AccessorySheetProperties.HEIGHT);
    }

    void hide() {
        mModel.set(VISIBLE, false);
    }

    boolean isShown() {
        return mModel.get(VISIBLE);
    }

    RecyclerView.OnScrollListener getScrollListener() {
        return mScrollListener;
    }

    void setStyle(boolean isDocked) {
        if (isDocked) {
            mModel.set(MAX_WIDTH, null);
            mModel.set(HORIZONTAL_PADDING, 0);
            mModel.set(GRAVITY, Gravity.START | Gravity.BOTTOM);
            mModel.set(ELEVATION, 0);
            mModel.set(TOP_OFFSET, 0);
            mModel.set(BACKGROUND, R.color.default_bg_color_baseline);
            mModel.set(BAR_SHADOW_VISIBLE, true);
            return;
        }

        Resources res = mContext.getResources();
        @Px
        int maxWidth =
                res.getDimensionPixelSize(
                        R.dimen.keyboard_accessory_sheet_dynamic_positioning_max_width);
        @Px
        int padding =
                res.getDimensionPixelSize(
                        R.dimen.keyboard_accessory_sheet_dynamic_positioning_padding);
        @Px int elevation = res.getDimensionPixelSize(R.dimen.keyboard_accessory_elevation);
        @Px int contentOffset = (mContentOffsetSupplier == null ? 0 : mContentOffsetSupplier.get());
        @Px
        int topOffset =
                contentOffset
                        - res.getDimensionPixelSize(R.dimen.keyboard_accessory_top_inset_overlap);
        topOffset = Math.max(topOffset, 0);

        mModel.set(MAX_WIDTH, maxWidth);
        mModel.set(HORIZONTAL_PADDING, padding);
        mModel.set(GRAVITY, Gravity.CENTER | Gravity.TOP);
        mModel.set(ELEVATION, elevation);
        mModel.set(TOP_OFFSET, topOffset);
        mModel.set(BACKGROUND, R.drawable.keyboard_accessory_shadow_shape);
        mModel.set(BAR_SHADOW_VISIBLE, false);
    }

    void setTabs(KeyboardAccessoryData.Tab[] tabs) {
        mModel.get(TABS).set(tabs);
        mModel.set(ACTIVE_TAB_INDEX, mModel.get(TABS).size() - 1);
    }

    void setActiveTab(int position) {
        assert position < mModel.get(TABS).size() || position >= 0
                : position + " is not a valid tab index!";
        mModel.set(ACTIVE_TAB_INDEX, position);
    }

    private void onKeyboardRequested() {
        // Return early if the button was clicked twice and the active tab was already reset.
        if (mModel.get(ACTIVE_TAB_INDEX) == NO_ACTIVE_TAB) return;
        ManualFillingMetricsRecorder.recordSheetTrigger(
                mModel.get(TABS).get(mModel.get(ACTIVE_TAB_INDEX)).getRecordingType(),
                AccessorySheetTrigger.MANUAL_CLOSE);
        mModel.set(ACTIVE_TAB_INDEX, NO_ACTIVE_TAB);
        mSheetVisibilityDelegate.onCloseAccessorySheet();
    }

    @Override
    public void onPropertyChanged(PropertyObservable<PropertyKey> source, PropertyKey propertyKey) {
        if (propertyKey == VISIBLE) {
            for (AccessorySheetVisualStateProvider.Observer observer : mVisualObservers) {
                observer.onAccessorySheetStateChanged(
                        mModel.get(VISIBLE), SemanticColorUtils.getDefaultBgColor(mContext));
            }
            if (mModel.get(VISIBLE) && getTab() != null && getTab().getListener() != null) {
                getTab().getListener().onTabShown();
            }
            return;
        }
        if (propertyKey == ACTIVE_TAB_INDEX
                || propertyKey == HEIGHT
                || propertyKey == MAX_WIDTH
                || propertyKey == HORIZONTAL_PADDING
                || propertyKey == GRAVITY
                || propertyKey == ELEVATION
                || propertyKey == TOP_OFFSET
                || propertyKey == BACKGROUND
                || propertyKey == BAR_SHADOW_VISIBLE
                || propertyKey == TOP_SHADOW_VISIBLE
                || propertyKey == PAGE_CHANGE_LISTENER
                || propertyKey == SHOW_KEYBOARD_CALLBACK) {
            return;
        }
        assert false : "Every property update needs to be handled explicitly!";
    }

    void setOnPageChangeListener(ViewPager.OnPageChangeListener onPageChangeListener) {
        mModel.set(PAGE_CHANGE_LISTENER, onPageChangeListener);
    }

    @VisibleForTesting
    public void setContentOffsetSupplier(Supplier<Integer> contentOffsetSupplier) {
        mContentOffsetSupplier = contentOffsetSupplier;
    }

    void addObserver(AccessorySheetVisualStateProvider.Observer observer) {
        mVisualObservers.addObserver(observer);
        observer.onAccessorySheetStateChanged(
                mModel.get(VISIBLE), SemanticColorUtils.getDefaultBgColor(mContext));
    }

    void removeObserver(AccessorySheetVisualStateProvider.Observer observer) {
        mVisualObservers.removeObserver(observer);
    }
}
