// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.button_group_component;

import static org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupProperties.ACTIVE_TAB;
import static org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupProperties.BUTTON_SELECTION_CALLBACKS;
import static org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupProperties.TABS;

import android.view.View;

import androidx.viewpager.widget.ViewPager;

import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryCoordinator;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.HashMap;

/**
 * This component reflects the state of selected tabs in the keyboard accessory. It can be assigned
 * to multiple {@link KeyboardAccessoryButtonGroupView}s and will keep them in sync.
 */
public class KeyboardAccessoryButtonGroupCoordinator {
    private final PropertyModel mModel =
            new PropertyModel.Builder(TABS, ACTIVE_TAB, BUTTON_SELECTION_CALLBACKS)
                    .with(TABS, new ListModel<>())
                    .with(ACTIVE_TAB, null)
                    .build();
    private final KeyboardAccessoryButtonGroupMediator mMediator;

    private final HashMap<View, TemporarySheetOpenerBindings> mBindings = new HashMap<>();

    private final SheetOpenerCallbacks mSheetOpenerCallbacks =
            new SheetOpenerCallbacks() {
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

    /** This observer gets notified when a tab get selected, reselected or when any tab changes. */
    public interface AccessoryTabObserver {
        /**
         * Called when the active tab changes.
         * @param activeTab The index of the active tab.
         */
        void onActiveTabChanged(Integer activeTab);
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
         * Called right before the view currently representing this item gets recycled. It's useful
         * for cleaning up Adapters and MCPs.
         */
        void onViewUnbound(View sheetOpenerView);
    }

    private class TemporarySheetOpenerBindings {
        private PropertyModelChangeProcessor mMcp;
        private ViewPager.OnPageChangeListener mOnPageChangeListener;

        TemporarySheetOpenerBindings(View view) {
            mMcp =
                    PropertyModelChangeProcessor.create(
                            mModel,
                            (KeyboardAccessoryButtonGroupView) view,
                            KeyboardAccessoryButtonGroupViewBinder::bind);
            mOnPageChangeListener = new ViewPager.SimpleOnPageChangeListener();
            mMediator.addPageChangeListener(mOnPageChangeListener);
        }

        void destroy() {
            mMediator.removePageChangeListener(mOnPageChangeListener);
            mMcp.destroy();
            mOnPageChangeListener = null;
        }
    }

    /**
     * Creates the {@link KeyboardAccessoryButtonGroupViewBinder} that is linked to the
     * {@link ListModelChangeProcessor} that connects the given
     * {@link KeyboardAccessoryButtonGroupView} to the given tab list.
     * @param model the {@link PropertyModel} with {@link KeyboardAccessoryButtonGroupProperties}.
     * @param inflatedView the {@link KeyboardAccessoryButtonGroupView}.
     * @return Returns a fully initialized and wired {@link KeyboardAccessoryButtonGroupViewBinder}.
     */
    static KeyboardAccessoryButtonGroupViewBinder createButtonGroupViewBinder(
            PropertyModel model, KeyboardAccessoryButtonGroupView inflatedView) {
        KeyboardAccessoryButtonGroupViewBinder buttonGroupViewBinder =
                new KeyboardAccessoryButtonGroupViewBinder();
        model.get(TABS)
                .addObserver(
                        new ListModelChangeProcessor<>(
                                model.get(TABS), inflatedView, buttonGroupViewBinder));
        return buttonGroupViewBinder;
    }

    /** Creates a new Tab Layout component that isn't assigned to any view yet. */
    public KeyboardAccessoryButtonGroupCoordinator() {
        mMediator = new KeyboardAccessoryButtonGroupMediator(mModel);
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

    PropertyModel getModelForTesting() {
        return mModel;
    }

    KeyboardAccessoryButtonGroupMediator getMediatorForTesting() {
        return mMediator;
    }
}
