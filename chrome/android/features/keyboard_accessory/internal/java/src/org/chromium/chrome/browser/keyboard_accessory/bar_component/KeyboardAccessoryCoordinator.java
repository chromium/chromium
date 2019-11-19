// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SKIP_CLOSING_ANIMATION;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;

import android.support.v4.view.ViewPager;
import android.view.ViewStub;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryViewBinder.BarItemViewHolder;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.Provider;
import org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutCoordinator;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.ui.DeferredViewStubInflationProvider;
import org.chromium.ui.ViewProvider;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.RecyclerViewAdapter;

/**
 * Creates and owns all elements which are part of the keyboard accessory component.
 * It's part of the controller but will mainly forward events (like adding a tab,
 * or showing the accessory) to the {@link KeyboardAccessoryMediator}.
 */
public class KeyboardAccessoryCoordinator {
    private final KeyboardAccessoryMediator mMediator;
    private final KeyboardAccessoryTabLayoutCoordinator mTabLayout;
    private final PropertyModelChangeProcessor
            .ViewBinder<PropertyModel, KeyboardAccessoryView, PropertyKey> mViewBinder;
    private final PropertyModel mModel;
    private KeyboardAccessoryView mView;

    /**
     * The keyboard accessory provides signals when to show or change the accessory sheet below it.
     * The actual implementation isn't relevant for this component. Therefore, a class implementing
     * this interface takes that responsibility, i.e. ManualFillingCoordinator.
     */
    public interface VisibilityDelegate {
        /**
         * Is triggered when a tab in the accessory was selected and the sheet needs to change.
         * @param tabIndex The index of the selected tab in the tab bar.
         */
        void onChangeAccessorySheet(int tabIndex);

        /**
         * Called when the sheet needs to be hidden.
         */
        void onCloseAccessorySheet();
    }

    /**
     * Describes a delegate manages all known tabs and is responsible to determine the active tab.
     */
    public interface TabSwitchingDelegate {
        /**
         * A {@link KeyboardAccessoryData.Tab} passed into this function will be represented as item
         * at the start of the tab layout. It is meant to trigger various bottom sheets.
         * @param tab The tab which contains representation data of a bottom sheet.
         */
        void addTab(KeyboardAccessoryData.Tab tab);

        /**
         * The {@link KeyboardAccessoryData.Tab} passed into this function will be completely
         * removed from the tab layout.
         * @param tab The tab to be removed.
         */
        void removeTab(KeyboardAccessoryData.Tab tab);

        /**
         * Clears all currently known tabs and adds the given tabs as replacement.
         * @param tabs An array of {@link KeyboardAccessoryData.Tab}s.
         */
        void setTabs(KeyboardAccessoryData.Tab[] tabs);

        /**
         * Closes any active tab so that {@link #getActiveTab} returns null again.
         */
        void closeActiveTab();

        /**
         * Returns whether active tab or null if no tab is currently active. The returned property
         * reflects the latest change while the view might still be in progress of being updated.
         * @return The active {@link KeyboardAccessoryData.Tab}, null otherwise.
         */
        @Nullable
        KeyboardAccessoryData.Tab getActiveTab();

        /**
         * Returns whether the model holds any tabs.
         * @return True if there is at least one tab, false otherwise.
         */
        boolean hasTabs();
    }

    /**
     * Initializes the component as soon as the native library is loaded by e.g. starting to listen
     * to keyboard visibility events.
     * @param barStub A {@link ViewStub} for the accessory bar layout.
     */
    public KeyboardAccessoryCoordinator(VisibilityDelegate visibilityDelegate, ViewStub barStub) {
        this(new KeyboardAccessoryTabLayoutCoordinator(), visibilityDelegate,
                new DeferredViewStubInflationProvider<>(barStub));
    }

    /**
     * Constructor that allows to mock the {@link DeferredViewStubInflationProvider}.
     * @param viewProvider A provider for the accessory.
     */
    @VisibleForTesting
    public KeyboardAccessoryCoordinator(KeyboardAccessoryTabLayoutCoordinator tabLayout,
            VisibilityDelegate visibilityDelegate,
            ViewProvider<KeyboardAccessoryView> viewProvider) {
        mTabLayout = tabLayout;
        mModel = KeyboardAccessoryProperties.defaultModelBuilder().build();
        mMediator = new KeyboardAccessoryMediator(mModel, visibilityDelegate,
                mTabLayout.getTabSwitchingDelegate(), mTabLayout.getTabLayoutCallbacks());
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)) {
            viewProvider.whenLoaded(barView -> mTabLayout.assignNewView(barView.getTabLayout()));
        }
        viewProvider.whenLoaded(view -> mView = view);

        mTabLayout.setTabObserver(mMediator);
        mViewBinder = ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)
                ? KeyboardAccessoryModernViewBinder::bind
                : KeyboardAccessoryViewBinder::bind;
        LazyConstructionPropertyMcp.create(mModel, VISIBLE, viewProvider, mViewBinder);
        KeyboardAccessoryMetricsRecorder.registerKeyboardAccessoryModelMetricsObserver(
                mModel, mTabLayout.getTabSwitchingDelegate());
    }

    /**
     * Creates an adapter to an {@link BarItemViewHolder} that is wired
     * up to the model change processor which listens to the given item list.
     * @param barItems The list of shown items represented by the adapter.
     * @return Returns a fully initialized and wired adapter to an BarItemViewHolder.
     */
    static RecyclerViewAdapter<BarItemViewHolder, Void> createBarItemsAdapter(
            ListModel<BarItem> barItems) {
        RecyclerViewAdapter.ViewHolderFactory<BarItemViewHolder> factory =
                KeyboardAccessoryViewBinder::create;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)) {
            factory = KeyboardAccessoryModernViewBinder::create;
        }
        return new RecyclerViewAdapter<>(
                new KeyboardAccessoryRecyclerViewMcp<>(barItems, BarItem::getViewType,
                        BarItemViewHolder::bind, BarItemViewHolder::recycle),
                factory);
    }

    public void closeActiveTab() {
        mTabLayout.getTabSwitchingDelegate().closeActiveTab();
    }

    public void setTabs(KeyboardAccessoryData.Tab[] tabs) {
        mTabLayout.getTabSwitchingDelegate().setTabs(tabs);
    }

    /**
     * Allows any {@link Provider} to communicate with the
     * {@link KeyboardAccessoryMediator} of this component.
     *
     * Note that the provided actions are removed when the accessory is hidden.
     *
     * @param provider The object providing action lists to observers in this component.
     */
    public void registerActionProvider(Provider<KeyboardAccessoryData.Action[]> provider) {
        provider.addObserver(mMediator);
    }

    /**
     * Registers a Provider.Observer to the given Provider. The
     * new observer will render chips into the accessory bar for every new suggestion and call the
     * given {@link AutofillDelegate} when the user interacts with a chip.
     * @param provider A {@link Provider<AutofillSuggestion[]>}.
     * @param delegate A {@link AutofillDelegate}.
     */
    public void registerAutofillProvider(
            Provider<AutofillSuggestion[]> provider, AutofillDelegate delegate) {
        provider.addObserver(mMediator.createAutofillSuggestionsObserver(delegate));
    }

    /**
     * Dismisses the accessory by hiding it's view, clearing potentially left over suggestions and
     * hiding the keyboard.
     */
    public void dismiss() {
        mMediator.dismiss();
    }

    /**
     * Sets the offset to the end of the activity - which is usually 0, the height of the keyboard
     * or the height of a bottom sheet.
     * @param bottomOffset The offset in pixels.
     */
    public void setBottomOffset(@Px int bottomOffset) {
        mMediator.setBottomOffset(bottomOffset);
    }

    /**
     * Triggers the accessory to be shown.
     */
    public void show() {
        mMediator.show();
    }

    /** Next time the accessory is closed, don't delay the closing animation. */
    public void skipClosingAnimationOnce() {
        mMediator.skipClosingAnimationOnce();
        // TODO(fhorschig): Consider allow LazyConstructionPropertyMcp to propagate updates once the
        // view exists. Currently it doesn't, so we need this ugly explicit binding.
        if (mView != null) mViewBinder.bind(mModel, mView, SKIP_CLOSING_ANIMATION);
    }

    /**
     * Returns the visibility of the the accessory. The returned property reflects the latest change
     * while the view might still be in progress of being updated accordingly.
     * @return True if the accessory should be visible, false otherwise.
     */
    public boolean isShown() {
        return mMediator.isShown();
    }

    /**
     * This method returns whether the accessory has any contents that justify showing it. A single
     * tab, action or suggestion chip would already mean it is not empty.
     * @return False if there is any content to be shown. True otherwise.
     */
    public boolean empty() {
        return mMediator.empty();
    }

    /**
     * Returns whether the active tab is non-null. The returned property reflects the latest change
     * while the view might still be in progress of being updated accordingly.
     * @return True if the accessory is visible and has an active tab, false otherwise.
     */
    public boolean hasActiveTab() {
        return mMediator.hasActiveTab();
    }

    public void prepareUserEducation() {
        mMediator.prepareUserEducation();
    }

    public ViewPager.OnPageChangeListener getOnPageChangeListener() {
        return mTabLayout.getStablePageChangeListener();
    }

    @VisibleForTesting
    public KeyboardAccessoryMediator getMediatorForTesting() {
        return mMediator;
    }
}
