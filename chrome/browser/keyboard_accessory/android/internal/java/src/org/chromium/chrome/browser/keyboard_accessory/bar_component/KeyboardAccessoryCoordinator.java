// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.autofill.AutofillUiUtils.getCardIcon;
import static org.chromium.chrome.browser.autofill.AutofillUiUtils.getValuableIcon;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SKIP_CLOSING_ANIMATION;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.viewpager.widget.ViewPager;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.AutofillImageFetcher;
import org.chromium.chrome.browser.autofill.AutofillImageFetcherFactory;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.KeyboardAccessoryVisualStateProvider;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryViewBinder.BarItemViewHolder;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.Provider;
import org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.AsyncViewProvider;
import org.chromium.ui.AsyncViewStub;
import org.chromium.ui.ViewProvider;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSupplier;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;

import java.util.List;
import java.util.function.Supplier;

/**
 * Creates and owns all elements which are part of the keyboard accessory component. It's part of
 * the controller but will mainly forward events (like adding a tab, or showing the accessory) to
 * the {@link KeyboardAccessoryMediator}.
 */
@NullMarked
public class KeyboardAccessoryCoordinator implements KeyboardAccessoryVisualStateProvider {
    private final KeyboardAccessoryMediator mMediator;
    private final KeyboardAccessoryButtonGroupCoordinator mButtonGroup;
    private final PropertyModel mModel;
    private @MonotonicNonNull KeyboardAccessoryView mView;
    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    private @MonotonicNonNull EdgeToEdgePadObserver mEdgeToEdgePadObserver;

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
         * The {@link KeyboardAccessoryData.Tab} passed into this function will be completely
         * removed from the tab layout.
         *
         * @param tab The tab to be removed.
         */
        void removeTab(KeyboardAccessoryData.Tab tab);

        /**
         * Clears all currently known tabs and adds the given tabs as replacement.
         *
         * @param tabs An array of {@link KeyboardAccessoryData.Tab}s.
         */
        void setTabs(KeyboardAccessoryData.Tab[] tabs);

        /** Closes any active tab so that {@link #getActiveTab} returns null again. */
        void closeActiveTab();

        /**
         * Set the currently active tab to the given tabType.
         *
         * @param tabType The type of the tab that should be selected.
         */
        void setActiveTab(@AccessoryTabType int tabType);

        /**
         * Returns whether active tab or null if no tab is currently active. The returned property
         * reflects the latest change while the view might still be in progress of being updated.
         *
         * @return The active {@link KeyboardAccessoryData.Tab}, null otherwise.
         */
        KeyboardAccessoryData.@Nullable Tab getActiveTab();

        /**
         * Returns whether the model holds any tabs.
         *
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
     * @param edgeToEdgeControllerSupplier A {@link Supplier<EdgeToEdgeController>}.
     * @param insetObserver An {@link InsetObserver}.
     * @param barStub A {@link AsyncViewStub} for the accessory bar layout.
     * @param isLargeFormFactorSupplier A {@link Supplier} that checks whether the device is in
     *     Large Form Factor mode.
     * @param dismissRunnable A {@link Runnable} used to dismiss the Keyboard Accessory bar.
     */
    public KeyboardAccessoryCoordinator(
            Profile profile,
            BarVisibilityDelegate barVisibilityDelegate,
            AccessorySheetCoordinator.SheetVisibilityDelegate sheetVisibilityDelegate,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            InsetObserver insetObserver,
            AsyncViewStub barStub,
            Supplier<Boolean> isLargeFormFactorSupplier,
            Runnable dismissRunnable) {
        this(
                barStub.getContext(),
                profile,
                new KeyboardAccessoryButtonGroupCoordinator(),
                barVisibilityDelegate,
                sheetVisibilityDelegate,
                edgeToEdgeControllerSupplier,
                insetObserver,
                AsyncViewProvider.of(barStub, R.id.keyboard_accessory),
                isLargeFormFactorSupplier,
                dismissRunnable);
    }

    /**
     * Constructor that allows to mock the {@link AsyncViewProvider}.
     *
     * @param context The {@link Context} associated with the current UI context.
     * @param profile The {@link Profile} associated with the data.
     * @param viewProvider A provider for the accessory.
     * @param edgeToEdgeControllerSupplier A {@link Supplier<EdgeToEdgeController>}.
     * @param insetObserver An {@link InsetObserver}.
     * @param isLargeFormFactorSupplier A {@link Supplier} that checks whether the device is in
     *     Large Form Factor mode.
     * @param dismissRunnable A {@link Runnable} used to dismiss the Keyboard Accessory bar.
     */
    @VisibleForTesting
    public KeyboardAccessoryCoordinator(
            Context context,
            Profile profile,
            KeyboardAccessoryButtonGroupCoordinator buttonGroup,
            BarVisibilityDelegate barVisibilityDelegate,
            AccessorySheetCoordinator.SheetVisibilityDelegate sheetVisibilityDelegate,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            InsetObserver insetObserver,
            ViewProvider<KeyboardAccessoryView> viewProvider,
            Supplier<Boolean> isLargeFormFactorSupplier,
            Runnable dismissRunnable) {
        mButtonGroup = buttonGroup;
        mModel = KeyboardAccessoryProperties.defaultModelBuilder().build();
        mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;

        mMediator =
                new KeyboardAccessoryMediator(
                        mModel,
                        profile,
                        barVisibilityDelegate,
                        sheetVisibilityDelegate,
                        mButtonGroup.getTabSwitchingDelegate(),
                        mButtonGroup.getSheetOpenerCallbacks(),
                        () -> SemanticColorUtils.getDefaultBgColor(context),
                        isLargeFormFactorSupplier,
                        dismissRunnable);
        viewProvider.whenLoaded(
                view -> {
                    mView = view;
                    mView.setBarItemsAdapter(
                            createBarItemsAdapter(
                                    mModel.get(KeyboardAccessoryProperties.BAR_ITEMS),
                                    mView,
                                    createUiConfiguration(
                                            context,
                                            AutofillImageFetcherFactory.getForProfile(profile))));
                    mView.setFixedBarItemsAdapter(
                            createBarItemsAdapter(
                                    mModel.get(KeyboardAccessoryProperties.BAR_ITEMS_FIXED),
                                    mView,
                                    createUiConfiguration(
                                            context,
                                            AutofillImageFetcherFactory.getForProfile(profile))));
                    mView.setFeatureEngagementTracker(TrackerFactory.getTrackerForProfile(profile));
                    mEdgeToEdgePadObserver =
                            new EdgeToEdgePadObserver(
                                    mView,
                                    mEdgeToEdgeControllerSupplier,
                                    insetObserver.getSupplierForKeyboardInset());
                });

        mButtonGroup.setTabObserver(mMediator);
        LazyConstructionPropertyMcp.create(
                mModel, VISIBLE, viewProvider, KeyboardAccessoryViewBinder::bind);
        KeyboardAccessoryMetricsRecorder.registerKeyboardAccessoryModelMetricsObserver(mModel);
    }

    @SuppressWarnings("NullAway")
    public void destroy() {
        if (mEdgeToEdgePadObserver != null) {
            mEdgeToEdgePadObserver.destroy();
            mEdgeToEdgePadObserver = null;
        }
    }

    @VisibleForTesting
    static KeyboardAccessoryViewBinder.UiConfiguration createUiConfiguration(
            Context context, AutofillImageFetcher imageFetcher) {
        KeyboardAccessoryViewBinder.UiConfiguration uiConfiguration =
                new KeyboardAccessoryViewBinder.UiConfiguration(
                        /* suggestionDrawableFunction= */ (suggestion) ->
                                getSuggestionIcon(context, imageFetcher, suggestion));

        return uiConfiguration;
    }

    private static @Nullable Drawable getSuggestionIcon(
            Context context,
            AutofillImageFetcher imageFetcher,
            @Nullable AutofillSuggestion suggestion) {
        if (suggestion == null) return null;
        if (suggestion.getSuggestionType() == SuggestionType.LOYALTY_CARD_ENTRY) {
            return getValuableIcon(
                    context,
                    imageFetcher,
                    assumeNonNull(suggestion.getCustomIconUrl()),
                    ImageSize.SMALL,
                    suggestion.getSublabel());
        }
        // TODO: crbug.com/404437211 - Figure out all suggestion types that have icons on Android
        // and return icons in a switch.
        return getCardIcon(
                context,
                imageFetcher,
                suggestion.getCustomIconUrl(),
                suggestion.getIconId(),
                ImageSize.SMALL,
                /* showCustomIcon= */ true);
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
     * Allows any {@link Provider} to communicate with the {@link KeyboardAccessoryMediator} of this
     * component.
     *
     * <p>Note that the provided actions are removed when the accessory is hidden.
     *
     * @param provider The object providing action lists to observers in this component.
     */
    public void registerActionProvider(Provider<KeyboardAccessoryData.Action[]> provider) {
        provider.addObserver(mMediator);
    }

    /**
     * Sets the suggestions to be displayed in the accessory bar.
     *
     * @param suggestions A list of {@link AutofillSuggestion}s.
     * @param delegate A {@link AutofillDelegate}.
     */
    public void setSuggestions(List<AutofillSuggestion> suggestions, AutofillDelegate delegate) {
        mMediator.setSuggestions(suggestions, delegate);
    }

    /**
     * Dismisses the accessory by hiding it's view, clearing potentially left over suggestions and
     * hiding the keyboard.
     */
    public void dismiss() {
        mMediator.dismiss();
    }

    /**
     * Applies a new style to the keyboard accessory component.
     *
     * <p>The style object encapsulates visual properties for the component, such as its vertical
     * offset or maximum width.
     *
     * @param style The {@link KeyboardAccessoryStyle} containing the visual attributes to apply.
     */
    public void setStyle(KeyboardAccessoryStyle style) {
        mMediator.setStyle(style);
    }

    /**
     * Defines whether the last item in the Keyboard Accessory bar should be aligned to the end of
     * the bar.
     */
    public void setHasStickyLastItem(boolean hasStickyLastItem) {
        mMediator.setHasStickyLastItem(hasStickyLastItem);
    }

    /**
     * Defines whether the suggestion animation should start from the top of the accessory bar.
     *
     * @param animateSuggestionsFromTop A boolean indicating whether to animate from the top.
     */
    public void setAnimateSuggestionsFromTop(boolean animateSuggestionsFromTop) {
        mMediator.setAnimateSuggestionsFromTop(animateSuggestionsFromTop);
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
     *
     * @return False if there is any content to be shown. True otherwise.
     */
    public boolean empty() {
        return mMediator.empty();
    }

    /**
     * Returns whether the active tab is non-null. The returned property reflects the latest change
     * while the view might still be in progress of being updated accordingly.
     *
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

    // KeyboardAccessoryVisualStateProvider

    @Override
    public void addObserver(KeyboardAccessoryVisualStateProvider.Observer observer) {
        mMediator.addObserver(observer);
    }

    @Override
    public void removeObserver(KeyboardAccessoryVisualStateProvider.Observer observer) {
        mMediator.removeObserver(observer);
    }

    /**
     * This provides an alternative to the SimpleEdgeToEdgePadAdjuster that doesn't clear the bottom
     * inset / bottom padding when the browser controls are showing, which is needed for bottom
     * components that aren't part of the bottom browser controls.
     */
    // TODO(crbug.com/435453719): Clean up after use cases are no longer needed.
    private static class EdgeToEdgePadObserver
            implements EdgeToEdgeSupplier.ChangeObserver, Destroyable {
        private final View mViewToPad;
        private final int mDefaultBottomPadding;
        private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
        private final ObservableSupplier<Integer> mKeyboardInsetSupplier;
        private @Nullable EdgeToEdgeController mEdgeToEdgeController;
        private final Callback<EdgeToEdgeController> mControllerChangedCallback =
                this::updateEdgeToEdgeController;
        private final Callback<Integer> mKeyboardInsetChangedCallback =
                this::onKeyboardInsetChanged;

        private boolean mIsDrawingToEdge;

        EdgeToEdgePadObserver(
                View view,
                ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
                ObservableSupplier<Integer> keyboardInsetSupplier) {
            mViewToPad = view;
            mDefaultBottomPadding = mViewToPad.getPaddingBottom();
            mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;
            mEdgeToEdgeControllerSupplier.addObserver(mControllerChangedCallback);
            mKeyboardInsetSupplier = keyboardInsetSupplier;
            mKeyboardInsetSupplier.addObserver(mKeyboardInsetChangedCallback);
        }

        @Override
        public void onToEdgeChange(
                int bottomInset, boolean isDrawingToEdge, boolean isPageOptInToEdge) {
            mIsDrawingToEdge = isDrawingToEdge;
            overrideBottomInset(bottomInset);
        }

        private void onKeyboardInsetChanged(int keyboardInset) {
            overrideBottomInset(
                    mEdgeToEdgeController != null ? mEdgeToEdgeController.getBottomInsetPx() : 0);
        }

        private void overrideBottomInset(int bottomInset) {
            // Set bottom padding to 0 if not drawing to edge, or if the keyboard is showing.
            int additionalBottomPadding =
                    (!mIsDrawingToEdge || mKeyboardInsetSupplier.get() > 0) ? 0 : bottomInset;
            mViewToPad.setPadding(
                    mViewToPad.getPaddingLeft(),
                    mViewToPad.getPaddingTop(),
                    mViewToPad.getPaddingRight(),
                    mDefaultBottomPadding + additionalBottomPadding);
        }

        @Override
        public void destroy() {
            // Reset the bottom insets for the view.
            overrideBottomInset(0);

            updateEdgeToEdgeController(null);
            if (mEdgeToEdgeControllerSupplier != null) {
                mEdgeToEdgeControllerSupplier.removeObserver(mControllerChangedCallback);
            }
            mKeyboardInsetSupplier.removeObserver(mKeyboardInsetChangedCallback);
        }

        private void updateEdgeToEdgeController(@Nullable EdgeToEdgeController newController) {
            if (mEdgeToEdgeController != null) {
                mEdgeToEdgeController.unregisterObserver(this);
            }
            mEdgeToEdgeController = newController;
            if (mEdgeToEdgeController != null) {
                mEdgeToEdgeController.registerObserver(this);
                onToEdgeChange(
                        mEdgeToEdgeController.getBottomInsetPx(),
                        mEdgeToEdgeController.isDrawingToEdge(),
                        mEdgeToEdgeController.isPageOptedIntoEdgeToEdge());
            }
        }
    }
}
