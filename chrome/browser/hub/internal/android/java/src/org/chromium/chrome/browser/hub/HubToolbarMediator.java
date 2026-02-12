// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.HAIRLINE_VISIBILITY;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.HUB_SEARCH_ENABLED_STATE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.MANUAL_SEARCH_BOX_ANIMATION;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.MENU_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_BUTTON_LOOKUP_CALLBACK;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_INDEX;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_BOX_VISIBILITY_FRACTION;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_LISTENER;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_LOUPE_VISIBLE;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNonNativeNtpUrl;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.core.util.Pair;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.hub.HubToolbarProperties.PaneButtonLookup;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.ResolutionType;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.omnibox.OmniboxFeatures;
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
        HubSearchEntrypoint.TAB_GROUPS_SEARCHBOX,
        HubSearchEntrypoint.TAB_GROUPS_LOUPE,
        HubSearchEntrypoint.NUM_ENTRIES
    })
    public @interface HubSearchEntrypoint {
        int REGULAR_SEARCHBOX = 0;
        int INCOGNITO_SEARCHBOX = 1;
        int REGULAR_LOUPE = 2;
        int INCOGNITO_LOUPE = 3;
        int TAB_GROUPS_SEARCHBOX = 4;
        int TAB_GROUPS_LOUPE = 5;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 6;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/android/enums.xml:HubSearchEntrypoint)

    static final int INVALID_PANE_SWITCHER_INDEX = -1;

    private final ComponentCallbacks mComponentCallbacks =
            new ComponentCallbacks() {
                @Override
                public void onConfigurationChanged(Configuration configuration) {
                    int screenWidthDp = configuration.screenWidthDp;
                    boolean isTablet = HubUtils.isScreenWidthTablet(screenWidthDp);

                    Pane pane = mPaneManager.getFocusedPaneSupplier().get();
                    if (pane == null) return;

                    // Only show the search box visuals in the tab switcher, incognito and
                    // potentially tab groups panes.
                    @PaneId int focusedPaneId = pane.getPaneId();
                    if (shouldOmitFocusedPaneForHubSearch(focusedPaneId)) {
                        mPropertyModel.set(APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION, true);
                        mPropertyModel.set(SEARCH_BOX_VISIBLE, false);
                        mPropertyModel.set(SEARCH_LOUPE_VISIBLE, false);
                    } else {
                        mPropertyModel.set(APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION, false);
                        mPropertyModel.set(SEARCH_BOX_VISIBLE, !isTablet);
                        mPropertyModel.set(SEARCH_LOUPE_VISIBLE, isTablet);
                    }
                }

                @Override
                public void onLowMemory() {}
            };

    private final PropertyModel mPropertyModel;

    private final Context mContext;
    private final PaneManager mPaneManager;
    private final Tracker mTracker;
    private final SearchActivityClient mSearchActivityClient;
    // The order of entries in this map are the order the buttons should appear to the user. A null
    // value should not be shown to the user.
    private final ArrayList<Pair<Integer, @Nullable DisplayButtonData>>
            mCachedPaneSwitcherButtonData = new ArrayList<>();
    // Actual observers are curried with PaneId, making it difficult to unsubscribe. These runnables
    // are closures that contain the original lambda object reference. It also protects us from
    // changes in the returned panes or suppliers.
    private final List<Runnable> mRemoveReferenceButtonObservers = new ArrayList<>();
    private final Callback<Pane> mOnFocusedPaneChange = this::onFocusedPaneChange;
    private final Callback<Boolean> mOnHubSearchEnabledStateChange =
            this::onHubSearchEnabledStateChange;
    private final Callback<Boolean> mOnSearchBoxVisibilityChange =
            this::onSearchBoxVisibilityChange;
    private final NonNullObservableSupplier<Boolean> mHairlineVisibilitySupplier;
    private final NonNullObservableSupplier<Boolean> mManualSearchBoxAnimationSupplier;
    private final NonNullObservableSupplier<Float> mSearchBoxVisibilityFractionSupplier;

    private final Callback<Boolean> mOnHairlineVisibilityChange = this::onHairlineVisibilityChange;
    private final Callback<Boolean> mOnManualSearchBoxAnimationChange =
            this::onManualSearchBoxAnimationChange;
    private final Callback<Float> mOnSearchBoxVisibilityFractionChange =
            this::onSearchBoxVisibilityFractionChange;

    private @Nullable PaneButtonLookup mPaneButtonLookup;
    private boolean mIgnoreTabLayoutSelection;

    /** Creates the mediator. */
    public HubToolbarMediator(
            Context context,
            PropertyModel propertyModel,
            PaneManager paneManager,
            Tracker tracker,
            SearchActivityClient searchActivityClient,
            Runnable exitHubRunnable) {
        mContext = context;
        mPropertyModel = propertyModel;
        mPaneManager = paneManager;
        mTracker = tracker;
        mSearchActivityClient = searchActivityClient;

        for (@PaneId int paneId : paneManager.getPaneOrderController().getPaneOrder()) {
            Pane pane = paneManager.getPaneForId(paneId);
            if (pane == null) continue;

            NullableObservableSupplier<DisplayButtonData> supplier =
                    pane.getReferenceButtonDataSupplier();
            Callback<@Nullable DisplayButtonData> observer =
                    (data) -> onReferenceButtonChange(paneId, data);

            // If the supplier already has data, this will post a callback to run our observer. But
            // we do not want this. We don't want to rebuild the button data list n times. Instead
            // all of these posted events should have data identical to what we initialize our cache
            // to, and they should all no-op.
            @Nullable DisplayButtonData currentButtonData =
                    supplier.addSyncObserverAndPostIfNonNull(observer);
            mCachedPaneSwitcherButtonData.add(new Pair<>(paneId, currentButtonData));

            mRemoveReferenceButtonObservers.add(() -> supplier.removeObserver(observer));

            pane.getHubSearchEnabledStateSupplier().addSyncObserver(mOnHubSearchEnabledStateChange);
            pane.getHubSearchBoxVisibilitySupplier().addSyncObserver(mOnSearchBoxVisibilityChange);
        }
        mHairlineVisibilitySupplier =
                paneManager
                        .getFocusedPaneSupplier()
                        .createTransitiveNonNull(false, Pane::getHairlineVisibilitySupplier);
        mHairlineVisibilitySupplier.addSyncObserverAndPostIfNonNull(mOnHairlineVisibilityChange);
        MonotonicObservableSupplier<Pane> focusedPaneSupplier =
                paneManager.getFocusedPaneSupplier();
        focusedPaneSupplier.addSyncObserverAndPostIfNonNull(mOnFocusedPaneChange);

        mManualSearchBoxAnimationSupplier =
                paneManager
                        .getFocusedPaneSupplier()
                        .createTransitiveNonNull(false, Pane::getManualSearchBoxAnimationSupplier);
        mManualSearchBoxAnimationSupplier.addSyncObserverAndPostIfNonNull(
                mOnManualSearchBoxAnimationChange);

        mSearchBoxVisibilityFractionSupplier =
                paneManager
                        .getFocusedPaneSupplier()
                        .createTransitiveNonNull(
                                0.0f, Pane::getSearchBoxVisibilityFractionSupplier);
        mSearchBoxVisibilityFractionSupplier.addSyncObserverAndPostIfNonNull(
                mOnSearchBoxVisibilityFractionChange);

        rebuildPaneSwitcherButtonData();

        mPropertyModel.set(PANE_BUTTON_LOOKUP_CALLBACK, this::consumeButtonLookup);

        mPropertyModel.set(SEARCH_LISTENER, this::onSearchClicked);

        // Fire an event for the original setup.
        mComponentCallbacks.onConfigurationChanged(mContext.getResources().getConfiguration());
        mContext.registerComponentCallbacks(mComponentCallbacks);
    }

    /** Cleans up observers. */
    public void destroy() {
        mRemoveReferenceButtonObservers.forEach(Runnable::run);
        mRemoveReferenceButtonObservers.clear();
        mPaneManager.getFocusedPaneSupplier().removeObserver(mOnFocusedPaneChange);
        mContext.unregisterComponentCallbacks(mComponentCallbacks);

        for (@PaneId int paneId : mPaneManager.getPaneOrderController().getPaneOrder()) {
            @Nullable Pane pane = mPaneManager.getPaneForId(paneId);
            if (pane == null) continue;
            pane.getHubSearchEnabledStateSupplier().removeObserver(mOnHubSearchEnabledStateChange);
            pane.getHubSearchBoxVisibilitySupplier().removeObserver(mOnSearchBoxVisibilityChange);
        }
        mHairlineVisibilitySupplier.removeObserver(mOnHairlineVisibilityChange);
        mManualSearchBoxAnimationSupplier.removeObserver(mOnManualSearchBoxAnimationChange);
        mSearchBoxVisibilityFractionSupplier.removeObserver(mOnSearchBoxVisibilityFractionChange);
    }

    /** Returns the button view for a given pane if present. */
    public @Nullable View getButton(@PaneId int paneId) {
        if (mPaneButtonLookup == null) return null;

        int size = mCachedPaneSwitcherButtonData.size();
        int index = 0;
        for (int i = 0; i < size; ++i) {
            Pair<Integer, @Nullable DisplayButtonData> pair = mCachedPaneSwitcherButtonData.get(i);
            if (Objects.equals(paneId, pair.first)) {
                return mPaneButtonLookup.get(index);
            } else if (pair.second != null) {
                // The button lookup only knows about visible (non null) buttons.
                index++;
            }
        }
        return null;
    }

    private int findCachedPaneSwitcherIndex(@PaneId int paneId) {
        int size = mCachedPaneSwitcherButtonData.size();
        for (int i = 0; i < size; ++i) {
            Pair<Integer, @Nullable DisplayButtonData> pair = mCachedPaneSwitcherButtonData.get(i);
            if (Objects.equals(paneId, pair.first)) {
                return i;
            }
        }
        return INVALID_PANE_SWITCHER_INDEX;
    }

    private void onSearchBoxVisibilityChange(Boolean shouldShow) {
        int screenWidthDp = mContext.getResources().getConfiguration().screenWidthDp;
        boolean isTablet = HubUtils.isScreenWidthTablet(screenWidthDp);
        shouldShow = !isTablet && shouldShow;

        mPropertyModel.set(SEARCH_BOX_VISIBLE, shouldShow);
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

        mIgnoreTabLayoutSelection = true;
        mPropertyModel.set(PANE_SWITCHER_BUTTON_DATA, buttonDataList);
        mIgnoreTabLayoutSelection = false;
    }

    private FullButtonData wrapButtonData(
            @PaneId int paneId, DisplayButtonData referenceButtonData) {
        Runnable onPress =
                () -> {
                    if (mIgnoreTabLayoutSelection) {
                        // When we rebuild the tab data, the selected tab layout will change, and
                        // our Runnables will be invoked for the current tab. This isn't a real
                        // input from the user, and can safely be ignored.
                        return;
                    }

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

    private void onFocusedPaneChange(Pane focusedPane) {
        int focusedPaneId = focusedPane.getPaneId();

        // This must be called before IS_INCOGNITO is set for all valid focused panes. This is
        // because hub search box elements (hint text) that will be updated via incognito state
        // changing will depend on a delay property key set in the configuration changed callback.
        // Fire an event to determine what is shown.
        mComponentCallbacks.onConfigurationChanged(mContext.getResources().getConfiguration());

        // Reset the enabled state of hub search to the supplier value or true if uninitialized
        // when toggling panes to account for a potential disabled state from incognito reauth.
        boolean enabled = focusedPane.getHubSearchEnabledStateSupplier().get();
        mPropertyModel.set(HUB_SEARCH_ENABLED_STATE, enabled);

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

    private void onHairlineVisibilityChange(@Nullable Boolean visible) {
        mPropertyModel.set(HAIRLINE_VISIBILITY, Boolean.TRUE.equals(visible));
    }

    private void onManualSearchBoxAnimationChange(@Nullable Boolean manual) {
        mPropertyModel.set(MANUAL_SEARCH_BOX_ANIMATION, Boolean.TRUE.equals(manual));
    }

    private void onSearchBoxVisibilityFractionChange(@Nullable Float fraction) {
        mPropertyModel.set(SEARCH_BOX_VISIBILITY_FRACTION, fraction == null ? 0.0f : fraction);
    }

    private void consumeButtonLookup(PaneButtonLookup paneButtonLookup) {
        mPaneButtonLookup = paneButtonLookup;
    }

    private void onSearchClicked() {
        @PaneId
        int focusedPaneId = assumeNonNull(mPaneManager.getFocusedPaneSupplier().get()).getPaneId();
        // Due to animations when switching between focused panes, there is exists a possibility for
        // race conditions which can cause clicks to register or allows them to be registered when
        // toggling panes. This logic filters out clicks unless the pane is hub search eligible.
        if (shouldOmitFocusedPaneForHubSearch(focusedPaneId)) return;

        mSearchActivityClient.requestOmniboxForResult(
                mSearchActivityClient
                        .newIntentBuilder()
                        .setPageUrl(new GURL(getOriginalNonNativeNtpUrl()))
                        .setIncognito(mPropertyModel.get(IS_INCOGNITO))
                        .setResolutionType(ResolutionType.OPEN_IN_CHROME)
                        .build());
        recordHubSearchEntrypointHistogram(mPropertyModel.get(SEARCH_BOX_VISIBLE));
    }

    private void recordHubSearchEntrypointHistogram(boolean isSearchBox) {
        // Based on the ComponentCallback#onConfigurationChanged logic for hub search, it is implied
        // that the search box and search loupe visibilities have opposite behaviors at any time.
        @HubSearchEntrypoint int action;
        @PaneId
        int focusedPaneId = assumeNonNull(mPaneManager.getFocusedPaneSupplier().get()).getPaneId();

        switch (focusedPaneId) {
            case PaneId.INCOGNITO_TAB_SWITCHER:
                action =
                        isSearchBox
                                ? HubSearchEntrypoint.INCOGNITO_SEARCHBOX
                                : HubSearchEntrypoint.INCOGNITO_LOUPE;
                break;
            case PaneId.TAB_SWITCHER:
                action =
                        isSearchBox
                                ? HubSearchEntrypoint.REGULAR_SEARCHBOX
                                : HubSearchEntrypoint.REGULAR_LOUPE;
                break;
            case PaneId.TAB_GROUPS:
                action =
                        isSearchBox
                                ? HubSearchEntrypoint.TAB_GROUPS_SEARCHBOX
                                : HubSearchEntrypoint.TAB_GROUPS_LOUPE;
                break;
            default:
                assert false : "Invalid focused pane id " + focusedPaneId;
                action = HubSearchEntrypoint.REGULAR_SEARCHBOX;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "Android.HubSearch.SearchBoxEntrypointV2", action, HubSearchEntrypoint.NUM_ENTRIES);
    }

    private boolean shouldOmitFocusedPaneForHubSearch(@PaneId int focusedPaneId) {
        return focusedPaneId != PaneId.TAB_SWITCHER
                && focusedPaneId != PaneId.INCOGNITO_TAB_SWITCHER
                && maybeExcludeHubSearchForTabGroupsPane(focusedPaneId);
    }

    private boolean maybeExcludeHubSearchForTabGroupsPane(@PaneId int focusedPaneId) {
        if (!OmniboxFeatures.sAndroidHubSearchTabGroups.isEnabled()
                || !OmniboxFeatures.sAndroidHubSearchEnableOnTabGroupsPane.getValue()) {
            return true;
        }

        return focusedPaneId != PaneId.TAB_GROUPS;
    }

    /** Test-only method to trigger configuration change for testing purposes. */
    void triggerConfigurationChangeForTesting(Configuration configuration) {
        mComponentCallbacks.onConfigurationChanged(configuration);
    }
}
