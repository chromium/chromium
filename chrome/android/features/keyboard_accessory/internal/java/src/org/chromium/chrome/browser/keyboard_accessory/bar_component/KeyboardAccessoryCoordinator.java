// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.chromium.chrome.browser.autofill.AutofillUiUtils.getCardIcon;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SKIP_CLOSING_ANIMATION;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.viewpager.widget.ViewPager;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryViewBinder.BarItemViewHolder;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.Provider;
import org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.ImageSize;
import org.chromium.ui.AsyncViewProvider;
import org.chromium.ui.AsyncViewStub;
import org.chromium.ui.ViewProvider;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;

import java.util.List;

/**
 * Creates and owns all elements which are part of the keyboard accessory component. It's part of
 * the controller but will mainly forward events (like adding a tab, or showing the accessory) to
 * the {@link KeyboardAccessoryMediator}.
 */
public class KeyboardAccessoryCoordinator {
    private final KeyboardAccessoryMediator mMediator;
    private final KeyboardAccessoryButtonGroupCoordinator mButtonGroup;
    private final PropertyModel mModel;
    private KeyboardAccessoryView mView;

    /**
     * The interface to notify consumers about keyboard accessories visibility. E.g: the animation
     * end. The actual implementation isn't relevant for this component. Therefore, a class
     * implementing this interface takes that responsibility, i.e. ManualFillingCoordinator.
     */
    public interface BarVisibilityDelegate {
        /**
         * Signals that the accessory bar has completed the fade-in. This may be relevant to the
         * keyboard extensions state to adjust the scroll position.
         */
        void onBarFadeInAnimationEnd();
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

        /** Closes any active tab so that {@link #getActiveTab} returns null again. */
        void closeActiveTab();

        /**
         * Set the currently active tab to the given tabType.
         * @param tabType The type of the tab that should be selected.
         */
        void setActiveTab(@AccessoryTabType int tabType);

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
     *
     * @param profile The {@link Profile} associated with the data.
     * @param barVisibilityDelegate A {@link BarVisibilityDelegate} for delegating the bar
     *     visibility changes.
     * @param sheetVisibilityDelegate A {@link AccessorySheetCoordinator.SheetVisibilityDelegate}
     *     for delegating the sheet visibility changes.
     * @param barStub A {@link AsyncViewStub} for the accessory bar layout.
     */
    public KeyboardAccessoryCoordinator(
            Profile profile,
            BarVisibilityDelegate barVisibilityDelegate,
            AccessorySheetCoordinator.SheetVisibilityDelegate sheetVisibilityDelegate,
            AsyncViewStub barStub) {
        this(
                barStub.getContext(),
                profile,
                new KeyboardAccessoryButtonGroupCoordinator(),
                barVisibilityDelegate,
                sheetVisibilityDelegate,
                AsyncViewProvider.of(barStub, R.id.keyboard_accessory));
    }

    /**
     * Constructor that allows to mock the {@link AsyncViewProvider}.
     *
     * @param context The {@link Context} associated with the current UI context.
     * @param profile The {@link Profile} associated with the data.
     * @param viewProvider A provider for the accessory.
     */
    @VisibleForTesting
    public KeyboardAccessoryCoordinator(
            Context context,
            Profile profile,
            KeyboardAccessoryButtonGroupCoordinator buttonGroup,
            BarVisibilityDelegate barVisibilityDelegate,
            AccessorySheetCoordinator.SheetVisibilityDelegate sheetVisibilityDelegate,
            ViewProvider<KeyboardAccessoryView> viewProvider) {
        mButtonGroup = buttonGroup;
        mModel = KeyboardAccessoryProperties.defaultModelBuilder().build();

        mMediator =
                new KeyboardAccessoryMediator(
                        mModel,
                        barVisibilityDelegate,
                        sheetVisibilityDelegate,
                        mButtonGroup.getTabSwitchingDelegate(),
                        mButtonGroup.getSheetOpenerCallbacks());
        viewProvider.whenLoaded(
                view -> {
                    mView = view;
                    mView.setBarItemsAdapter(
                            createBarItemsAdapter(
                                    mModel.get(KeyboardAccessoryProperties.BAR_ITEMS),
                                    mView,
                                    createUiConfiguration(
                                            context,
                                            PersonalDataManagerFactory.getForProfile(profile))));
                    mView.setFeatureEngagementTracker(TrackerFactory.getTrackerForProfile(profile));
                });

        mButtonGroup.setTabObserver(mMediator);
        LazyConstructionPropertyMcp.create(
                mModel, VISIBLE, viewProvider, KeyboardAccessoryViewBinder::bind);
        KeyboardAccessoryMetricsRecorder.registerKeyboardAccessoryModelMetricsObserver(mModel);
    }

    @VisibleForTesting
    static KeyboardAccessoryViewBinder.UiConfiguration createUiConfiguration(
            Context context, PersonalDataManager personalDataManager) {
        KeyboardAccessoryViewBinder.UiConfiguration uiConfiguration =
                new KeyboardAccessoryViewBinder.UiConfiguration();
        uiConfiguration.suggestionDrawableFunction =
                (suggestion) ->
                        getCardIcon(
                                context,
                                personalDataManager,
                                suggestion.getCustomIconUrl(),
                                suggestion.getIconId(),
                                ImageSize.SMALL,
                                /* showCustomIcon= */ true);
        return uiConfiguration;
    }

    /**
     * Creates an adapter to an {@link BarItemViewHolder} that is wired up to the model change
     * processor which listens to the given item list.
     *
     * @param barItems The list of shown items represented by the adapter.
     * @param view The keyboard accessory view that will display the bar items.
     * @return Returns a fully initialized and wired adapter to an BarItemViewHolder.
     */
    @VisibleForTesting
    static RecyclerViewAdapter<BarItemViewHolder, Void> createBarItemsAdapter(
            ListModel<BarItem> barItems,
            KeyboardAccessoryView view,
            KeyboardAccessoryViewBinder.UiConfiguration uiConfiguration) {
        return new RecyclerViewAdapter<>(
                new KeyboardAccessoryRecyclerViewMcp<>(
                        barItems,
                        BarItem::getViewType,
                        BarItemViewHolder::bind,
                        BarItemViewHolder::recycle),
                (parent, viewType) ->
                        KeyboardAccessoryViewBinder.create(
                                view, uiConfiguration, parent, viewType));
    }

    public void closeActiveTab() {
        mButtonGroup.getTabSwitchingDelegate().closeActiveTab();
    }

    public void setTabs(KeyboardAccessoryData.Tab[] tabs) {
        mButtonGroup.getTabSwitchingDelegate().setTabs(tabs);
    }

    public void setActiveTab(@AccessoryTabType int tabType) {
        mButtonGroup.getTabSwitchingDelegate().setActiveTab(tabType);
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
     * Registers a Provider.Observer to the given Provider. The new observer will render chips into
     * the accessory bar for every new suggestion and call the given {@link AutofillDelegate} when
     * the user interacts with a chip.
     *
     * @param provider A {@link Provider<List<AutofillSuggestion>>}.
     * @param delegate A {@link AutofillDelegate}.
     */
    public void registerAutofillProvider(
            Provider<List<AutofillSuggestion>> provider, AutofillDelegate delegate) {
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

    /** Triggers the accessory to be shown. */
    public void show() {
        TraceEvent.begin("KeyboardAccessoryCoordinator#show");
        mMediator.show();
        TraceEvent.end("KeyboardAccessoryCoordinator#show");
    }

    /** Next time the accessory is closed, don't delay the closing animation. */
    public void skipClosingAnimationOnce() {
        mMediator.skipClosingAnimationOnce();
        // TODO(fhorschig): Consider allow LazyConstructionPropertyMcp to propagate updates once the
        // view exists. Currently it doesn't, so we need this ugly explicit binding.
        if (mView != null) {
            KeyboardAccessoryViewBinder.bind(mModel, mView, SKIP_CLOSING_ANIMATION);
        }
    }

    /**
     * Returns the visibility of the the accessory. The returned property reflects the latest change
     * while the view might still be in progress of being updated accordingly.
     *
     * @return True if the accessory should be visible, false otherwise.
     */
    // TODO(crbug.com/40879203): Hide because it's only used in tests.
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

    public ViewPager.OnPageChangeListener getOnPageChangeListener() {
        return mButtonGroup.getStablePageChangeListener();
    }

    public KeyboardAccessoryMediator getMediatorForTesting() {
        return mMediator;
    }
}
