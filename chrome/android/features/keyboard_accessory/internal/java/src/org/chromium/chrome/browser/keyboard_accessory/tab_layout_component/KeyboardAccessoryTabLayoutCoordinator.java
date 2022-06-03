// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.tab_layout_component;

import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutProperties.ACTIVE_TAB;
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutProperties.TABS;
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutProperties.TAB_SELECTION_CALLBACKS;

import androidx.annotation.VisibleForTesting;
import androidx.viewpager.widget.ViewPager;

import com.google.android.material.tabs.TabLayout;

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
            new PropertyModel.Builder(TABS, ACTIVE_TAB, TAB_SELECTION_CALLBACKS)
                    .with(TABS, new ListModel<>())
                    .with(ACTIVE_TAB, null)
                    .build();
    private final KeyboardAccessoryTabLayoutMediator mMediator;

    private final HashMap<TabLayout, TemporaryTabLayoutBindings> mBindings = new HashMap<>();

    private final TabLayoutCallbacks mTabLayoutCallbacks = new TabLayoutCallbacks() {
        @Override
        public void onTabLayoutBound(TabLayout tabLayout) {
            if (!mBindings.containsKey(tabLayout)) {
                mBindings.put(tabLayout, new TemporaryTabLayoutBindings(tabLayout));
            }
        }

        @Override
        public void onTabLayoutUnbound(TabLayout tabLayout) {
            TemporaryTabLayoutBindings binding = mBindings.remove(tabLayout);
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
     * These callbacks are triggered when the view corresponding to the tab layout is bound or
     * unbound which provides opportunity to initialize and clean up.
     */
    public interface TabLayoutCallbacks {
        /**
         * Called when the this item is bound to a view. It's useful for setting up MCPs that
         * need the view for initialization.
         * @param tabs The {@link TabLayout} representing this item.
         */
        void onTabLayoutBound(TabLayout tabs);

        /**
         * Called right before the view currently representing this item gets recycled.
         * It's useful for cleaning up Adapters and MCPs.
         * @param tabs The {@link TabLayout} representing this item.
         */
        void onTabLayoutUnbound(TabLayout tabs);
    }

    private class TemporaryTabLayoutBindings {
        private PropertyModelChangeProcessor mMcp;
        private TabLayout.TabLayoutOnPageChangeListener mOnPageChangeListener;

        TemporaryTabLayoutBindings(TabLayout tabLayout) {
            mMcp = PropertyModelChangeProcessor.create(mModel,
                    (KeyboardAccessoryTabLayoutView) tabLayout,
                    KeyboardAccessoryTabLayoutViewBinder::bind);
            mOnPageChangeListener = new TabLayout.TabLayoutOnPageChangeListener(tabLayout);
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

    public TabLayoutCallbacks getTabLayoutCallbacks() {
        return mTabLayoutCallbacks;
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
