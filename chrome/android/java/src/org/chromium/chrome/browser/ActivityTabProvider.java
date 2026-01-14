// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.build.NullUtil.assumeNonNull;

import com.google.errorprone.annotations.DoNotMock;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;

import java.util.function.Supplier;

/** A class that provides the current {@link Tab} for various states of the browser's activity. */
@NullMarked
@DoNotMock("Using a concrete class has worked everywhere so far.")
public class ActivityTabProvider implements Destroyable, Supplier<@Nullable Tab> {
    /**
     * A utility class for observing the activity tab via {@link TabObserver}. When the activity tab
     * changes, the observer is switched to that tab.
     */
    public static class ActivityTabTabObserver extends TabSupplierObserver {
        /**
         * Create a new {@link TabObserver} that only observes the activity tab. It doesn't trigger
         * for the initial tab being attached to after creation.
         * @param tabProvider An {@link ActivityTabProvider} to get the activity tab.
         */
        public ActivityTabTabObserver(ActivityTabProvider tabProvider) {
            this(tabProvider, false);
        }

        /**
         * Create a new {@link TabObserver} that only observes the activity tab. This constructor
         * allows the option of triggering for the initial tab being attached to after creation.
         *
         * @param tabProvider An {@link ActivityTabProvider} to get the activity tab.
         * @param shouldTrigger Whether the observer should be triggered for the initial tab after
         *     creation.
         */
        public ActivityTabTabObserver(ActivityTabProvider tabProvider, boolean shouldTrigger) {
            super(tabProvider.mObservableSupplier, shouldTrigger);
        }

        @Override
        protected void onObservingDifferentTab(@Nullable Tab tab) {}
    }

    /** A handle to the {@link LayoutStateProvider} to get the active layout. */
    private @Nullable LayoutStateProvider mLayoutStateProvider;

    /** The observer watching scene changes in the active layout. */
    private final LayoutStateObserver mLayoutStateObserver;

    /** A handle to the {@link TabModelSelector}. */
    private @Nullable TabModelSelector mTabModelSelector;

    /** An observer for watching tab creation and switching events. */
    private @Nullable TabModelSelectorTabModelObserver mTabModelObserver;

    /** An observer for watching tab model switching event. */
    private final Callback<TabModel> mCurrentTabModelObserver;

    private final SettableNullableObservableSupplier<Tab> mObservableSupplier =
            ObservableSuppliers.createNullable();

    /** Default constructor. */
    public ActivityTabProvider() {
        mLayoutStateObserver =
                new LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(@LayoutType int layout) {
                        // The {@link SimpleAnimationLayout} is a special case, the intent is not to
                        // switch tabs, but to merely run an animation. In this case, do nothing.
                        // If the animation layout does result in a new tab {@link
                        // TabModelObserver#didSelectTab} will trigger the event instead. If the
                        // tab does not change, the event will noop.
                        if (LayoutType.SIMPLE_ANIMATION == layout) return;

                        assumeNonNull(mTabModelSelector);
                        Tab tab = mTabModelSelector.getCurrentTab();
                        if (layout != LayoutType.BROWSING) tab = null;
                        triggerActivityTabChangeEvent(tab);
                    }

                    @Override
                    @SuppressWarnings("NullAway") // https://github.com/uber/NullAway/issues/1209
                    public void onStartedHiding(@LayoutType int layout) {
                        if (mTabModelSelector == null) return;

                        if (LayoutType.TAB_SWITCHER == layout) {
                            // TODO(https://github.com/uber/NullAway/issues/1209): Remove
                            // assumeNonNull().
                            Tab tab = assumeNonNull(mTabModelSelector.getCurrentTab());
                            mObservableSupplier.set(tab);
                        }
                    }
                };
        mCurrentTabModelObserver =
                (tabModel) -> {
                    // Send a signal with null tab if a new model has no tab. Other cases
                    // are taken care of by TabModelSelectorTabModelObserver#didSelectTab.
                    if (tabModel.getCount() == 0) triggerActivityTabChangeEvent(null);
                };
    }

    @Override
    public Tab get() {
        return mObservableSupplier.get();
    }

    public void setForTesting(@Nullable Tab tab) {
        mObservableSupplier.set(tab);
    }

    public NullableObservableSupplier<Tab> asObservable() {
        return mObservableSupplier;
    }

    /**
     * @param selector A {@link TabModelSelector} for watching for changes in tabs.
     */
    public void setTabModelSelector(TabModelSelector selector) {
        assert mTabModelSelector == null;
        mTabModelSelector = selector;
        mTabModelObserver =
                new TabModelSelectorTabModelObserver(selector) {
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        triggerActivityTabChangeEvent(tab);
                    }

                    @Override
                    public void willCloseTab(Tab tab, boolean didCloseAlone) {
                        // If this is the last tab to close, make sure a signal is sent to the
                        // observers.
                        if (selector.getCurrentModel().getCount() <= 1) {
                            triggerActivityTabChangeEvent(null);
                        }
                    }
                };

        mTabModelSelector.getCurrentTabModelSupplier().addObserver(mCurrentTabModelObserver);
    }

    /**
     * @param layoutStateProvider A {@link LayoutStateProvider} for watching for scene changes.
     */
    public void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        assert mLayoutStateProvider == null;
        mLayoutStateProvider = layoutStateProvider;
        mLayoutStateProvider.addObserver(mLayoutStateObserver);
    }

    /**
     * Check if the interactive tab change event needs to be triggered based on the provided tab.
     * @param tab The activity's tab.
     */
    private void triggerActivityTabChangeEvent(@Nullable Tab tab) {
        // Allow the event to trigger before native is ready (before the layout manager is set).
        if (mLayoutStateProvider != null
                && !(mLayoutStateProvider.isLayoutVisible(LayoutType.BROWSING)
                        || mLayoutStateProvider.isLayoutVisible(LayoutType.SIMPLE_ANIMATION))
                && tab != null) {
            return;
        }

        mObservableSupplier.set(tab);
    }

    /** Clean up and detach any observers this object created. */
    @Override
    public void destroy() {
        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
            mLayoutStateProvider = null;
        }
        if (mTabModelSelector != null) {
            assumeNonNull(mTabModelObserver);
            mTabModelObserver.destroy();
            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
            mTabModelSelector = null;
        }
    }
}
