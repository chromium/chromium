// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.tab_layout_component;

import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutProperties.ACTIVE_TAB;
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutProperties.BUTTON_SELECTION_CALLBACKS;
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutProperties.TABS;
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutProperties.TAB_SELECTION_CALLBACKS;

import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.viewpager.widget.ViewPager;

import com.google.android.material.tabs.TabLayout;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryCoordinator;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.HashMap;

/**
 * This component reflects the state of selected tabs in the keyboard accessory. It can be assigned
 * to multiple {@link TabLayout}s and will keep them in sync.
 */
public class KeyboardAccessoryTabLayoutCoordinator {
    private final PropertyModel mModel =
            new PropertyModel
                    .Builder(TABS, ACTIVE_TAB, TAB_SELECTION_CALLBACKS, BUTTON_SELECTION_CALLBACKS)
                    .with(TABS, new ListModel<>())
                    .with(ACTIVE_TAB, null)
                    .build();
    private final KeyboardAccessoryTabLayoutMediator mMediator;

    private final HashMap<View, TemporarySheetOpenerBindings> mBindings = new HashMap<>();

    private final SheetOpenerCallbacks mSheetOpenerCallbacks = new SheetOpenerCallbacks() {
        @Override
        public void onViewBound(View view) {
            if (!mBindings.containsKey(view)) {
                mBindings.put(view, new TemporarySheetOpenerBindings(view));
            }
        }

        @Override
        public void onViewUnbound(View view) {
            TemporarySheetOpenerBindings binding = mBindings.remove(view);
            if (binding != null) binding.destroy();
        }
    };

    /**
     * This observer gets notified when a tab get selected, reselected or when any tab changes.
     */
    public interface AccessoryTabObserver {
        /**
         * Called when the active tab changes.
         * @param activeTab The index of the active tab.
         */
        void onActiveTabChanged(Integer activeTab);

        /**
         * Called when an active tab is selected again. This only triggers if the selected tab is
         * the {@link KeyboardAccessoryTabLayoutProperties#ACTIVE_TAB} in the tab layout model.
         * Therefore, whenever {@link TabLayout.OnTabSelectedListener#onTabReselected} is called,
         * either this function or {@link #onActiveTabChanged(Integer)} is called. Never both.
         */
        void onActiveTabReselected();
    }

    /**
     * These callbacks are triggered when the view corresponding to the sheet opener buttons or tabs
     * is bound or unbound which provides opportunity to initialize and clean up.
     */
    public interface SheetOpenerCallbacks {
        /**
         * Called when the this item is bound to a view. It's useful for setting up MCPs that
         * need the view for initialization.
         * @param sheetOpenerView The {@link View} representing this item.
         */
        void onViewBound(View sheetOpenerView);

        /**
         * Called right before the view currently representing this item gets recycled.
         * It's useful for cleaning up Adapters and MCPs.
         * @param tabs The {@link View} representing this item.
         */
        void onViewUnbound(View sheetOpenerView);
    }

    private class TemporarySheetOpenerBindings {
        private PropertyModelChangeProcessor mMcp;
        private ViewPager.OnPageChangeListener mOnPageChangeListener;

        TemporarySheetOpenerBindings(View view) {
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)) {
                mMcp = PropertyModelChangeProcessor.create(mModel,
                        (KeyboardAccessoryButtonGroupView) view,
                        KeyboardAccessoryButtonGroupViewBinder::bind);
                mOnPageChangeListener = new ViewPager.SimpleOnPageChangeListener();
                mMediator.addPageChangeListener(mOnPageChangeListener);
                return;
            }
            mMcp = PropertyModelChangeProcessor.create(mModel,
                    (KeyboardAccessoryTabLayoutView) view,
                    KeyboardAccessoryTabLayoutViewBinder::bind);
            mOnPageChangeListener = new TabLayout.TabLayoutOnPageChangeListener((TabLayout) view);
            mMediator.addPageChangeListener(mOnPageChangeListener);
        }

        void destroy() {
            mMediator.removePageChangeListener(mOnPageChangeListener);
            mMcp.destroy();
            mOnPageChangeListener = null;
        }
    }

    /**
     * Creates the {@link KeyboardAccessoryTabLayoutViewBinder} that is linked to the
     * {@link ListModelChangeProcessor} that connects the given
     * {@link KeyboardAccessoryTabLayoutView} to the given tab list.
     * @param model the {@link PropertyModel} with {@link KeyboardAccessoryTabLayoutProperties}.
     * @param inflatedView the {@link KeyboardAccessoryTabLayoutView}.
     * @return Returns a fully initialized and wired {@link KeyboardAccessoryTabLayoutView}.
     */
    static KeyboardAccessoryTabLayoutViewBinder createTabViewBinder(
            PropertyModel model, KeyboardAccessoryTabLayoutView inflatedView) {
        KeyboardAccessoryTabLayoutViewBinder tabViewBinder =
                new KeyboardAccessoryTabLayoutViewBinder();
        model.get(TABS).addObserver(
                new ListModelChangeProcessor<>(model.get(TABS), inflatedView, tabViewBinder));
        return tabViewBinder;
    }

    /**
     * Creates the {@link KeyboardAccessoryButtonGroupViewBinder} that is linked to the
     * {@link ListModelChangeProcessor} that connects the given
     * {@link KeyboardAccessoryButtonGroupView} to the given tab list.
     * @param model the {@link PropertyModel} with {@link KeyboardAccessoryTabLayoutProperties}.
     * @param inflatedView the {@link KeyboardAccessoryButtonGroupView}.
     * @return Returns a fully initialized and wired {@link KeyboardAccessoryButtonGroupViewBinder}.
     */
    static KeyboardAccessoryButtonGroupViewBinder createButtonGroupViewBinder(
            PropertyModel model, KeyboardAccessoryButtonGroupView inflatedView) {
        KeyboardAccessoryButtonGroupViewBinder buttonGroupViewBinder =
                new KeyboardAccessoryButtonGroupViewBinder();
        model.get(TABS).addObserver(new ListModelChangeProcessor<>(
                model.get(TABS), inflatedView, buttonGroupViewBinder));
        return buttonGroupViewBinder;
    }

    /**
     * Creates a new Tab Layout component that isn't assigned to any view yet.
     */
    public KeyboardAccessoryTabLayoutCoordinator() {
        mMediator = new KeyboardAccessoryTabLayoutMediator(mModel);
    }

    /**
     * Binds the given view to its model using the {@link KeyboardAccessoryTabLayoutViewBinder}.
     * @param tabLayout A {@link TabLayout}.
     */
    public void assignNewView(TabLayout tabLayout) {
        mMediator.addPageChangeListener(new TabLayout.TabLayoutOnPageChangeListener(tabLayout));
        PropertyModelChangeProcessor.create(mModel, (KeyboardAccessoryTabLayoutView) tabLayout,
                KeyboardAccessoryTabLayoutViewBinder::bind);
    }

    public SheetOpenerCallbacks getSheetOpenerCallbacks() {
        return mSheetOpenerCallbacks;
    }

    /**
     * Returns a delegate that executes on several tab-related actions.
     * @return A {@link KeyboardAccessoryCoordinator.TabSwitchingDelegate}.
     */
    public KeyboardAccessoryCoordinator.TabSwitchingDelegate getTabSwitchingDelegate() {
        return mMediator;
    }

    /**
     * Adds a {@link AccessoryTabObserver} that is notified about events emitted when a tab changes.
     * @param accessoryTabObserver The component to be notified of tab changes.
     */
    public void setTabObserver(AccessoryTabObserver accessoryTabObserver) {
        mMediator.setTabObserver(accessoryTabObserver);
    }

    /**
     * Returns an OnPageChangeListener that remains the same even if the assigned views changes.
     * This is useful if multiple views are bound to this component or if the view may temporarily
     * be destroyed (like in a RecyclerView).
     * @return A stable {@link ViewPager.OnPageChangeListener}.
     */
    public ViewPager.OnPageChangeListener getStablePageChangeListener() {
        return mMediator.getStableOnPageChangeListener();
    }

    @VisibleForTesting
    PropertyModel getModelForTesting() {
        return mModel;
    }

    @VisibleForTesting
    KeyboardAccessoryTabLayoutMediator getMediatorForTesting() {
        return mMediator;
    }
}
