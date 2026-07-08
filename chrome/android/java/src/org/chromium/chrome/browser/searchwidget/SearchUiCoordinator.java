// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.View;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.BackKeyBehaviorDelegate;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.LocationBarEmbedder;
import org.chromium.chrome.browser.omnibox.LocationBarEmbedderUiOverrides;
import org.chromium.chrome.browser.omnibox.OverrideUrlLoadingDelegate;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionDelegateImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.VoiceToolbarButtonController;
import org.chromium.chrome.browser.ui.edge_to_edge.NoOpTopInsetProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.components.omnibox.TextSelection;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

/**
 * Reusable coordinator hosting {@link LocationBarCoordinator}, {@link SearchBoxDataProvider}, and
 * associated UI overrides for embedding Omnibox search inside activities or other instances.
 */
@NullMarked
public class SearchUiCoordinator {
    private final Activity mActivity;
    private @Nullable View mControlContainer;
    private @Nullable SearchActivityLocationBarLayout mSearchBox;
    private @Nullable View mAnchorView;
    private @Nullable EdgeToEdgeSystemBarColorHelper mSystemBarColorHelper;
    private @Nullable BackPressManager mBackPressManager;
    private @Nullable LocationBarCoordinator mLocationBarCoordinator;

    // LocationBarEmbedderUiOverrides are passed to several child components upon construction.
    // Ensure we don't accidentally introduce disconnection by keeping only a single live instance
    // here.
    private final LocationBarEmbedderUiOverrides mLocationBarUiOverrides =
            new LocationBarEmbedderUiOverrides();
    private final LocationBarDataProvider mLocationBarDataProvider;

    /**
     * Constructs a new {@link SearchUiCoordinator}.
     *
     * @param activity Hosting activity.
     * @param locationBarDataProvider The location bar data provider.
     */
    public SearchUiCoordinator(Activity activity, LocationBarDataProvider locationBarDataProvider) {
        mActivity = activity;
        mLocationBarDataProvider = locationBarDataProvider;
        mLocationBarUiOverrides.setForcedPhoneStyleOmnibox();
    }

    /**
     * Initializes the coordinator with the required UI components and dependencies.
     *
     * @param contentView The root layout containing the search UI elements.
     * @param controlContainer The container holding the surrounding or enclosing parent views.
     * @param windowAndroid Window container instance.
     * @param profileSupplier Profile supplier.
     * @param snackbarManager Snackbar manager.
     * @param modalDialogManagerSupplier Modal dialog manager supplier.
     * @param lifecycleDispatcher Activity lifecycle dispatcher.
     * @param tabModelSelectorSupplier Tab model selector supplier.
     * @param overrideUrlLoadingDelegate Delegate to override navigation URL loading.
     * @param backKeyBehaviorDelegate Delegate for back key navigation in location bar.
     * @param bringTabGroupToFrontCallback Callback to bring a tab group to front.
     * @param omniboxActionDelegate Omnibox action delegate.
     * @param backPressManager Optional back press manager. If null, an internal instance is
     *     created.
     * @param locationBarEmbedder Location bar embedder.
     * @param systemBarColorHelper Edge-to-edge system bar color helper.
     */
    public void initialize(
            View contentView,
            View controlContainer,
            WindowAndroid windowAndroid,
            MonotonicObservableSupplier<Profile> profileSupplier,
            SnackbarManager snackbarManager,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            OverrideUrlLoadingDelegate overrideUrlLoadingDelegate,
            BackKeyBehaviorDelegate backKeyBehaviorDelegate,
            Callback<String> bringTabGroupToFrontCallback,
            OmniboxActionDelegateImpl omniboxActionDelegate,
            @Nullable BackPressManager backPressManager,
            LocationBarEmbedder locationBarEmbedder,
            @Nullable EdgeToEdgeSystemBarColorHelper systemBarColorHelper) {
        assert mLocationBarCoordinator == null : "initialize() should only be called once";

        mControlContainer = controlContainer;
        mSearchBox = contentView.findViewById(R.id.search_location_bar);
        mAnchorView = contentView.findViewById(R.id.toolbar);
        mSystemBarColorHelper = systemBarColorHelper;
        mBackPressManager = backPressManager != null ? backPressManager : new BackPressManager();

        mLocationBarCoordinator =
                new LocationBarCoordinator(
                        mSearchBox,
                        mAnchorView,
                        profileSupplier,
                        mLocationBarDataProvider,
                        null,
                        assertNonNull(windowAndroid),
                        /* activityTabSupplier= */ ObservableSuppliers.alwaysNull(),
                        modalDialogManagerSupplier,
                        /* shareDelegateSupplier= */ null,
                        /* incognitoStateProvider= */ null,
                        lifecycleDispatcher,
                        overrideUrlLoadingDelegate,
                        backKeyBehaviorDelegate,
                        /* pageInfoAction= */ (tab, pageInfoHighlight) -> {},
                        bringTabGroupToFrontCallback,
                        /* omniboxUma= */ (url, transition, isNtp) -> {},
                        /* bookmarkState= */ (url) -> false,
                        VoiceToolbarButtonController::isToolbarMicEnabled,
                        omniboxActionDelegate,
                        null,
                        mBackPressManager,
                        /* omniboxSuggestionsDropdownScrollListener= */ null,
                        tabModelSelectorSupplier,
                        /* topInsetProvider= */ new NoOpTopInsetProvider(),
                        locationBarEmbedder,
                        mLocationBarUiOverrides,
                        contentView,
                        /* bottomWindowPaddingSupplier= */ () -> 0,
                        /* onLongClickListener= */ null,
                        /* browserControlsStateProvider= */ null,
                        /* isToolbarPositionCustomizationEnabled= */ false,
                        /* pageZoomManager= */ null,
                        TabFavicon::getBitmap,
                        assertNonNull(snackbarManager),
                        assertNonNull(contentView.findViewById(R.id.bottom_container)),
                        /* omniboxChipManager= */ null,
                        /* scrimHandler= */ null,
                        /* userEducationHelper= */ null);

        mLocationBarCoordinator.setUrlBarFocusable(true);
        mLocationBarCoordinator.setShouldShowMicButtonWhenUnfocused(true);
        setColorScheme(mLocationBarDataProvider.isIncognitoBranded());

        // The native initialization may have already finished before this coordinator was
        // initialized. Finish native initialization for the LocationBarCoordinator in that case.
        if (lifecycleDispatcher.isNativeInitializationFinished()) {
            mLocationBarCoordinator.onFinishNativeInitialization();
        }
    }

    /** Destroys the coordinator and its associated underlying components. */
    public void destroy() {
        if (mLocationBarCoordinator != null) {
            mLocationBarCoordinator.destroy();
            mLocationBarCoordinator = null;
        }
    }

    /**
     * @return The primary search box layout.
     */
    public SearchActivityLocationBarLayout getSearchBox() {
        return assertNonNull(mSearchBox);
    }

    /**
     * @return The anchor view for suggestions dropdown.
     */
    public View getAnchorView() {
        return assertNonNull(mAnchorView);
    }

    /**
     * @return The authoritative {@link BackPressManager} arbitrating UI navigation.
     */
    public BackPressManager getBackPressManager() {
        return assertNonNull(mBackPressManager);
    }

    /**
     * @return The {@link LocationBarCoordinator} hosting the underlying Omnibox.
     */
    public LocationBarCoordinator getLocationBarCoordinator() {
        return assertNonNull(mLocationBarCoordinator);
    }

    /**
     * @return The {@link LocationBarEmbedderUiOverrides} controlling Omnibox styling.
     */
    public LocationBarEmbedderUiOverrides getLocationBarUiOverrides() {
        return mLocationBarUiOverrides;
    }

    /**
     * Sets an icon override resource ID to replace the default status icon. See {@link
     * StatusCoordinator#setDefaultStatusIconOverrideResId(int)} for more details.
     *
     * @param iconOverrideResId The resource ID of the override icon, or {@link Resources#ID_NULL}
     *     to clear.
     */
    public void setDefaultStatusIconOverrideResId(@DrawableRes int iconOverrideResId) {
        assertNonNull(mLocationBarCoordinator).setDefaultStatusIconOverrideResId(iconOverrideResId);
    }

    @VisibleForTesting
    /* package */ void setLocationBarCoordinator(LocationBarCoordinator coordinator) {
        mLocationBarCoordinator = coordinator;
    }

    @VisibleForTesting
    /* package */ void setSearchBox(SearchActivityLocationBarLayout layout) {
        mSearchBox = layout;
    }

    @VisibleForTesting
    /* package */ void setAnchorView(View anchorView) {
        mAnchorView = anchorView;
    }

    @VisibleForTesting
    /* package */ void setControlContainer(View controlContainer) {
        mControlContainer = controlContainer;
    }

    /**
     * Adds a listener for URL focus changes.
     *
     * @param listener The {@link UrlFocusChangeListener} to add.
     */
    public void addUrlFocusChangeListener(UrlFocusChangeListener listener) {
        assertNonNull(mLocationBarCoordinator);
        assertNonNull(mLocationBarCoordinator.getOmniboxStub()).addUrlFocusChangeListener(listener);
    }

    /**
     * Removes a listener for URL focus changes.
     *
     * @param listener The {@link UrlFocusChangeListener} to remove.
     */
    public void removeUrlFocusChangeListener(UrlFocusChangeListener listener) {
        if (mLocationBarCoordinator == null || mLocationBarCoordinator.getOmniboxStub() == null) {
            return;
        }
        mLocationBarCoordinator.getOmniboxStub().removeUrlFocusChangeListener(listener);
    }

    /**
     * Begins a search query execution and focuses the Omnibox.
     *
     * @param intentOrigin The origin of the search intent.
     * @param searchType The type of search requested.
     * @param query Optional initial query text.
     * @param windowAndroid Optional window container.
     */
    public void beginQuery(
            int intentOrigin,
            int searchType,
            @Nullable String query,
            @Nullable WindowAndroid windowAndroid) {
        assertNonNull(mLocationBarCoordinator);
        assertNonNull(mSearchBox);

        setColorScheme(mLocationBarDataProvider.isIncognitoBranded());

        mLocationBarCoordinator.setUrlBarFocus(
                new AutocompleteInput(OmniboxFocusReason.OMNIBOX_TAP)
                        .setUserText(query != null ? query : "")
                        .setSelection(TextSelection.SELECT_ALL));
        mSearchBox.beginQuery(intentOrigin, searchType, windowAndroid);
    }

    /**
     * Sets the color scheme of the search box background and anchor view background based on the
     * current incognito state. In the non incognito state, the color scheme is the same as what is
     * defined on initialize in {@link SearchActivityLocationBarLayout}.
     *
     * @param isIncognito Whether the current session is incognito.
     */
    public void setColorScheme(boolean isIncognito) {
        @ColorRes int anchorViewBackgroundColorRes = R.color.omnibox_suggestion_dropdown_bg;
        @ColorRes int searchBoxColorRes = R.color.search_suggestion_bg_color;

        if (isIncognito) {
            anchorViewBackgroundColorRes = R.color.omnibox_dropdown_bg_incognito;
            searchBoxColorRes = R.color.toolbar_text_box_background_incognito;
        }

        assertNonNull(mSearchBox)
                .getBackground()
                .setBackgroundColor(mActivity.getColor(searchBoxColorRes));

        int anchorViewColor = mActivity.getColor(anchorViewBackgroundColorRes);
        GradientDrawable anchorViewBackground =
                (GradientDrawable) assertNonNull(mAnchorView).getBackground();
        anchorViewBackground.setColor(anchorViewColor);

        Drawable controlBackground = assertNonNull(mControlContainer).getBackground();
        if (controlBackground instanceof GradientDrawable gradientDrawable) {
            gradientDrawable.setColor(anchorViewColor);
        } else {
            mControlContainer.setBackgroundColor(anchorViewColor);
        }

        setStatusAndNavBarColors();
    }

    /**
     * Sets the status and navigation bar colors to match the background color of the anchor view.
     *
     * <p>Make sure that mAnchorView has the desired background color before you call this method.
     */
    public void setStatusAndNavBarColors() {
        Drawable anchorViewBackground = assertNonNull(mAnchorView).getBackground();
        assert anchorViewBackground instanceof GradientDrawable
                : "Unsupported background drawable.";

        ColorStateList color = ((GradientDrawable) anchorViewBackground).getColor();
        int anchorViewColor = assumeNonNull(color).getDefaultColor();

        StatusBarColorController.setStatusBarColor(
                mSystemBarColorHelper, mActivity, anchorViewColor);
        if (mSystemBarColorHelper != null) {
            mSystemBarColorHelper.setNavigationBarColor(anchorViewColor);
        }
    }
}
