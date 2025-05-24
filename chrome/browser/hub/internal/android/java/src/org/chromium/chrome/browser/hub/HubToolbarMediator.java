// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.HUB_SEARCH_ENABLED_STATE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.MENU_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_BUTTON_LOOKUP_CALLBACK;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_INDEX;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_LISTENER;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_LOUPE_VISIBLE;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.Pair;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.TransitiveObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.hub.HubToolbarProperties.PaneButtonLookup;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.ResolutionType;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/** Logic for the toolbar of the Hub. */
@NullMarked
public class HubToolbarMediator {
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange(HubSearchEntrypoint)
    @IntDef({
        HubSearchEntrypoint.REGULAR_SEARCHBOX,
        HubSearchEntrypoint.INCOGNITO_SEARCHBOX,
        HubSearchEntrypoint.REGULAR_LOUPE,
        HubSearchEntrypoint.INCOGNITO_LOUPE,
        HubSearchEntrypoint.NUM_ENTRIES
    })
    public @interface HubSearchEntrypoint {
        int REGULAR_SEARCHBOX = 0;
        int INCOGNITO_SEARCHBOX = 1;
        int REGULAR_LOUPE = 2;
        int INCOGNITO_LOUPE = 3;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 4;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/android/enums.xml:HubSearchEntrypoint)

    private static final int INVALID_PANE_SWITCHER_INDEX = -1;

    private final ComponentCallbacks mComponentCallbacks =
            new ComponentCallbacks() {
                @Override
                public void onConfigurationChanged(Configuration configuration) {
                    Pane pane = mPaneManager.getFocusedPaneSupplier().get();
                    if (pane == null) return;

                    // Only show the search box visuals in the tab switcher and incognito panes.
                    @PaneId int focusedPaneId = pane.getPaneId();
                    if (focusedPaneId != PaneId.TAB_SWITCHER
                            && focusedPaneId != PaneId.INCOGNITO_TAB_SWITCHER) {
                        mPropertyModel.set(APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION, true);
                        mPropertyModel.set(SEARCH_BOX_VISIBLE, false);
                        mPropertyModel.set(SEARCH_LOUPE_VISIBLE, false);
                        return;
                    }

                    int screenWidthDp = mContext.getResources().getConfiguration().screenWidthDp;
                    boolean showLoupe = isScreenWidthTablet(screenWidthDp);
                    mPropertyModel.set(APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION, false);
                    mPropertyModel.set(SEARCH_BOX_VISIBLE, !showLoupe);
                    mPropertyModel.set(SEARCH_LOUPE_VISIBLE, showLoupe);
                }

                @Override
                public void onLowMemory() {}
            };

    private final PropertyModel mPropertyModel;

    private final Callback<FullButtonData> mOnActionButtonChangeCallback =
            this::onActionButtonChange;
    private @Nullable TransitiveObservableSupplier<Pane, FullButtonData> mActionButtonDataSupplier;

    private final Context mContext;
    private final PaneManager mPaneManager;
    private final Tracker mTracker;
    private final SearchActivityClient mSearchActivityClient;
    // The order of entries in this map are the order the buttons should appear to the user. A null
    // value should not be shown to the user.
    private final ArrayList<Pair<Integer, DisplayButtonData>> mCachedPaneSwitcherButtonData =
            new ArrayList<>();
    // Actual observers are curried with PaneId, making it difficult to unsubscribe. These runnables
    // are closures that contain the original lambda object reference. It also protects us from
    // changes in the returned panes or suppliers.
    private final List<Runnable> mRemoveReferenceButtonObservers = new ArrayList<>();
    private final Callback<Pane> mOnFocusedPaneChange = this::onFocusedPaneChange;
    private final Callback<Boolean> mOnHubSearchEnabledStateChange =
            this::onHubSearchEnabledStateChange;

    private @Nullable PaneButtonLookup mPaneButtonLookup;

    /** Creates the mediator. */
    public HubToolbarMediator(
            Context context,
            PropertyModel propertyModel,
            PaneManager paneManager,
            Tracker tracker,
            SearchActivityClient searchActivityClient) {
        mContext = context;
        mPropertyModel = propertyModel;
        mPaneManager = paneManager;
        mTracker = tracker;
        mSearchActivityClient = searchActivityClient;

        for (@PaneId int paneId : paneManager.getPaneOrderController().getPaneOrder()) {
            Pane pane = paneManager.getPaneForId(paneId);
            if (pane == null) continue;

            ObservableSupplier<DisplayButtonData> supplier = pane.getReferenceButtonDataSupplier();
            Callback<DisplayButtonData> observer = (data) -> onReferenceButtonChange(paneId, data);

            // If the supplier already has data, this will post a callback to run our observer. But
            // we do not want this. We don't want to rebuild the button data list n times. Instead
            // all of these posted events should have data identical to what we initialize our cache
            // to, and they should all no-op.
            DisplayButtonData currentButtonData = supplier.addObserver(observer);
            mCachedPaneSwitcherButtonData.add(new Pair<>(paneId, currentButtonData));

            mRemoveReferenceButtonObservers.add(() -> supplier.removeObserver(observer));

            if (OmniboxFeatures.sAndroidHubSearch.isEnabled()) {
                pane.getHubSearchEnabledStateSupplier().addObserver(mOnHubSearchEnabledStateChange);
            }
        }
        ObservableSupplier<Pane> focusedPaneSupplier = paneManager.getFocusedPaneSupplier();
        focusedPaneSupplier.addObserver(mOnFocusedPaneChange);
        rebuildPaneSwitcherButtonData();

        mActionButtonDataSupplier =
                new TransitiveObservableSupplier<>(
                        focusedPaneSupplier, p -> p.getActionButtonDataSupplier());
        mActionButtonDataSupplier.addObserver(mOnActionButtonChangeCallback);

        mPropertyModel.set(PANE_BUTTON_LOOKUP_CALLBACK, this::consumeButtonLookup);

        if (OmniboxFeatures.sAndroidHubSearch.isEnabled()) {
            mPropertyModel.set(SEARCH_LISTENER, this::onSearchClicked);
            // Fire an event for the original setup.
            mComponentCallbacks.onConfigurationChanged(mContext.getResources().getConfiguration());
            mContext.registerComponentCallbacks(mComponentCallbacks);
        }
    }

    /** Cleans up observers. */
    public void destroy() {
        if (mActionButtonDataSupplier != null) {
            mActionButtonDataSupplier.removeObserver(mOnActionButtonChangeCallback);
            mActionButtonDataSupplier = null;
        }
        mRemoveReferenceButtonObservers.forEach(Runnable::run);
        mRemoveReferenceButtonObservers.clear();
        mPaneManager.getFocusedPaneSupplier().removeObserver(mOnFocusedPaneChange);
        if (OmniboxFeatures.sAndroidHubSearch.isEnabled()) {
            mContext.unregisterComponentCallbacks(mComponentCallbacks);

            for (@PaneId int paneId : mPaneManager.getPaneOrderController().getPaneOrder()) {
                @Nullable Pane pane = mPaneManager.getPaneForId(paneId);
                if (pane == null) continue;
                pane.getHubSearchEnabledStateSupplier()
                        .removeObserver(mOnHubSearchEnabledStateChange);
            }
        }
    }

    /** Returns the button view for a given pane if present. */
    public @Nullable View getButton(@PaneId int paneId) {
        if (mPaneButtonLookup == null) return null;

        int size = mCachedPaneSwitcherButtonData.size();
        int index = 0;
        for (int i = 0; i < size; ++i) {
            Pair<Integer, DisplayButtonData> pair = mCachedPaneSwitcherButtonData.get(i);
            if (Objects.equals(paneId, pair.first)) {
                return mPaneButtonLookup.get(index);
            } else if (pair.second != null) {
                // The button lookup only knows about visible (non null) buttons.
                index++;
            }
        }
        return null;
    }

    private void onActionButtonChange(@Nullable FullButtonData actionButtonData) {
        mPropertyModel.set(ACTION_BUTTON_DATA, actionButtonData);
    }

    private int findCachedPaneSwitcherIndex(@PaneId int paneId) {
        int size = mCachedPaneSwitcherButtonData.size();
        for (int i = 0; i < size; ++i) {
            Pair<Integer, DisplayButtonData> pair = mCachedPaneSwitcherButtonData.get(i);
            if (Objects.equals(paneId, pair.first)) {
                return i;
            }
        }
        return INVALID_PANE_SWITCHER_INDEX;
    }

    private void onReferenceButtonChange(@PaneId int paneId, @Nullable DisplayButtonData current) {
        int index = findCachedPaneSwitcherIndex(paneId);
        if (index == INVALID_PANE_SWITCHER_INDEX) return;
        @Nullable DisplayButtonData previous = mCachedPaneSwitcherButtonData.get(index).second;
        if (!Objects.equals(current, previous)) {
            mCachedPaneSwitcherButtonData.set(index, new Pair<>(paneId, current));
            rebuildPaneSwitcherButtonData();
        }
    }

    private void rebuildPaneSwitcherButtonData() {
        @Nullable Pane focusedPane = mPaneManager.getFocusedPaneSupplier().get();
        @Nullable Integer focusedPaneId = focusedPane == null ? null : focusedPane.getPaneId();
        int currentIndex = 0;
        int selectedIndex = -1;

        List<FullButtonData> buttonDataList = new ArrayList<>();
        for (Pair<Integer, DisplayButtonData> pair : mCachedPaneSwitcherButtonData) {
            @Nullable DisplayButtonData buttonData = pair.second;
            if (buttonData == null) {
                continue;
            }
            if (Objects.equals(pair.first, focusedPaneId)) {
                selectedIndex = currentIndex;
            }
            buttonDataList.add(wrapButtonData(pair.first, buttonData));
            currentIndex++;
        }
        mPropertyModel.set(PANE_SWITCHER_INDEX, selectedIndex);
        mPropertyModel.set(PANE_SWITCHER_BUTTON_DATA, buttonDataList);
    }

    private FullButtonData wrapButtonData(
            @PaneId int paneId, DisplayButtonData referenceButtonData) {
        Runnable onPress =
                () -> {
                    // TODO(crbug.com/345492118): Move the event name into the tab group pane impl.
                    if (paneId == PaneId.TAB_GROUPS) {
                        mTracker.notifyEvent("tab_groups_surface_clicked");
                    }
                    mPaneManager.focusPane(paneId);
                    RecordHistogram.recordEnumeratedHistogram(
                            "Android.Hub.PaneFocused.PaneSwitcher", paneId, PaneId.COUNT);
                };
        return new DelegateButtonData(referenceButtonData, onPress);
    }

    private void onFocusedPaneChange(@Nullable Pane focusedPane) {
        @Nullable Integer focusedPaneId = focusedPane == null ? null : focusedPane.getPaneId();
        if (focusedPaneId == null) {
            mPropertyModel.set(PANE_SWITCHER_INDEX, INVALID_PANE_SWITCHER_INDEX);
            mPropertyModel.set(MENU_BUTTON_VISIBLE, false);
            mPropertyModel.set(IS_INCOGNITO, false);
            return;
        }
        assumeNonNull(focusedPane);

        // This must be called before IS_INCOGNITO is set for all valid focused panes. This is
        // because hub search box elements (hint text) that will be updated via incognito state
        // changing will depend on a delay property key set in the configuration changed callback.
        if (OmniboxFeatures.sAndroidHubSearch.isEnabled()) {
            // Fire an event to determine what is shown.
            mComponentCallbacks.onConfigurationChanged(mContext.getResources().getConfiguration());

            // Reset the enabled state of hub search to the supplier value or true if uninitialized
            // when toggling panes to account for a potential disabled state from incognito reauth.
            Boolean hubSearchEnabledState = focusedPane.getHubSearchEnabledStateSupplier().get();
            boolean enabled = hubSearchEnabledState == null ? true : hubSearchEnabledState;
            mPropertyModel.set(HUB_SEARCH_ENABLED_STATE, enabled);
        }

        mPropertyModel.set(MENU_BUTTON_VISIBLE, focusedPane.getMenuButtonVisible());

        boolean isIncognito = focusedPaneId == PaneId.INCOGNITO_TAB_SWITCHER;
        mPropertyModel.set(IS_INCOGNITO, isIncognito);

        int index = 0;
        for (Pair<Integer, DisplayButtonData> pair : mCachedPaneSwitcherButtonData) {
            if (pair.second == null) {
                continue;
            } else if (Objects.equals(pair.first, focusedPaneId)) {
                mPropertyModel.set(PANE_SWITCHER_INDEX, index);
                return;
            }
            index++;
        }
    }

    private void onHubSearchEnabledStateChange(boolean enabled) {
        mPropertyModel.set(HUB_SEARCH_ENABLED_STATE, enabled);
    }

    private void consumeButtonLookup(PaneButtonLookup paneButtonLookup) {
        mPaneButtonLookup = paneButtonLookup;
    }

    private void onSearchClicked() {
        mSearchActivityClient.requestOmniboxForResult(
                mSearchActivityClient
                        .newIntentBuilder()
                        .setPageUrl(new GURL(UrlConstants.NTP_NON_NATIVE_URL))
                        .setIncognito(mPropertyModel.get(IS_INCOGNITO))
                        .setResolutionType(ResolutionType.OPEN_IN_CHROME)
                        .build());
        recordHubSearchEntrypointHistogram(
                mPropertyModel.get(SEARCH_BOX_VISIBLE), mPropertyModel.get(IS_INCOGNITO));
    }

    /** Utility to determine which UI variants to show based on device width. */
    @VisibleForTesting
    public static boolean isScreenWidthTablet(int screenWidthDp) {
        return screenWidthDp >= DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP;
    }

    private void recordHubSearchEntrypointHistogram(boolean isSearchBox, boolean isIncognito) {
        // Based on the ComponentCallback#onConfigurationChanged logic for hub search, it is implied
        // that the search box and search loupe visibilities have opposite behaviors at any time.
        @HubSearchEntrypoint int action;

        if (isIncognito) {
            action =
                    isSearchBox
                            ? HubSearchEntrypoint.INCOGNITO_SEARCHBOX
                            : HubSearchEntrypoint.INCOGNITO_LOUPE;
        } else {
            action =
                    isSearchBox
                            ? HubSearchEntrypoint.REGULAR_SEARCHBOX
                            : HubSearchEntrypoint.REGULAR_LOUPE;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "Android.HubSearch.SearchBoxEntrypointV2", action, HubSearchEntrypoint.NUM_ENTRIES);
    }
}
