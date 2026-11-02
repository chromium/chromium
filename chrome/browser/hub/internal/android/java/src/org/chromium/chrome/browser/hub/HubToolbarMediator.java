// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.APPLY_DELAY_FOR_SEARCH_BOX_ANIMATION;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.CLOSE_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.HAIRLINE_VISIBILITY;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.HUB_SEARCH_ENABLED_STATE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.MANUAL_SEARCH_BOX_ANIMATION;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.MENU_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_BUTTON_LOOKUP_CALLBACK;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_INDEX;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_BOX_TRANSITION_IS_INCOGNITO;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_BOX_TRANSITION_OFFSET;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_BOX_VISIBILITY_FRACTION;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_LISTENER;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_LOUPE_VISIBLE;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNtpUrl;

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
import org.chromium.chrome.browser.ui.actions.button.DelegateButtonData;
import org.chromium.chrome.browser.ui.actions.button.DisplayButtonData;
import org.chromium.chrome.browser.ui.actions.button.FullButtonData;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.ResolutionType;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/** Logic for the toolbar of the Hub. */
@NullMarked
public class HubToolbarMediator {
    // Progress at which a search box shown by both panes switches to the incoming pane's hint.
    private static final float SEARCH_BOX_HINT_SWITCH_PROGRESS = 0.5f;
    // A manual search box animation starting below this fraction reveals the search box.
    private static final float SEARCH_BOX_REVEAL_MAX_START_FRACTION = 0.5f;

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
                    mPropertyModel.set(
                            CLOSE_BUTTON_VISIBLE,
                            DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext));

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

    private final NullableObservableSupplier<PaneTransitionProgress>
            mPaneTransitionProgressSupplier;
    private final Callback<@Nullable PaneTransitionProgress> mOnPaneTransitionProgress =
            this::onPaneTransitionProgress;

    private @Nullable PaneButtonLookup mPaneButtonLookup;
    private boolean mIgnoreTabLayoutSelection;
    // Ignores search box clicks during a pane transition, since the search box can show the hint of
    // a pane that is not focused.
    private boolean mIgnoreSearchBoxClicks;
    // Keeps the search box open after a pane transition lands on a pane with a search box, until
    // that pane's next manual search box animation ends.
    private boolean mKeepSearchBoxOpen;
    // Whether the current pane transition already decided to keep the search box open.
    private boolean mHasCheckedKeepSearchBoxOpen;

    /** Creates the mediator. */
    public HubToolbarMediator(
            Context context,
            PropertyModel propertyModel,
            PaneManager paneManager,
            Tracker tracker,
            SearchActivityClient searchActivityClient,
            Runnable exitHubRunnable,
            NullableObservableSupplier<PaneTransitionProgress> paneTransitionProgressSupplier) {
        mContext = context;
        mPropertyModel = propertyModel;
        mPaneManager = paneManager;
        mTracker = tracker;
        mSearchActivityClient = searchActivityClient;
        mPaneTransitionProgressSupplier = paneTransitionProgressSupplier;

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

        mPaneTransitionProgressSupplier.addSyncObserver(mOnPaneTransitionProgress);

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
        mPaneTransitionProgressSupplier.removeObserver(mOnPaneTransitionProgress);
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
        shouldShow = !isScreenWidthTablet() && shouldShow;

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
        Callback<View> onPress =
                view -> {
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
        return new DelegateButtonData.Builder(referenceButtonData).setOnPress(onPress).build();
    }

    private void onFocusedPaneChange(Pane focusedPane) {
        int focusedPaneId = focusedPane.getPaneId();
        // Any focus change decides again whether to keep the search box open, even if it interrupts
        // a transition. Swipes focus their destination while their progress is still set, so they
        // decide here. Taps focus before HubPaneHostMediator starts their slide (HubCoordinator
        // creates the toolbar first), so their first frame decides instead.
        mKeepSearchBoxOpen = false;
        mHasCheckedKeepSearchBoxOpen = false;
        maybeKeepSearchBoxOpen(mPaneTransitionProgressSupplier.get());

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

        boolean isIncognito = isIncognitoPane(focusedPaneId);
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
        boolean isManual = Boolean.TRUE.equals(manual);
        // HubToolbarView only applies SEARCH_BOX_VISIBILITY_FRACTION during a manual animation.
        mPropertyModel.set(MANUAL_SEARCH_BOX_ANIMATION, isManual);
        if (!isManual) {
            // The manual search box animation ended, so stop keeping the search box open.
            mKeepSearchBoxOpen = false;
        } else if (mKeepSearchBoxOpen) {
            // Only a reveal, which starts from a collapsed search box, keeps it open.
            float fraction = mSearchBoxVisibilityFractionSupplier.get();
            mKeepSearchBoxOpen = fraction < SEARCH_BOX_REVEAL_MAX_START_FRACTION;
            mPropertyModel.set(
                    SEARCH_BOX_VISIBILITY_FRACTION, mKeepSearchBoxOpen ? 1.0f : fraction);
        }
    }

    private void onSearchBoxVisibilityFractionChange(@Nullable Float fraction) {
        float value = fraction == null ? 0.0f : fraction;
        mPropertyModel.set(SEARCH_BOX_VISIBILITY_FRACTION, mKeepSearchBoxOpen ? 1.0f : value);
    }

    private void consumeButtonLookup(PaneButtonLookup paneButtonLookup) {
        mPaneButtonLookup = paneButtonLookup;
    }

    private void onSearchClicked() {
        if (mIgnoreSearchBoxClicks) return;

        @PaneId
        int focusedPaneId = assumeNonNull(mPaneManager.getFocusedPaneSupplier().get()).getPaneId();
        // Due to animations when switching between focused panes, there is exists a possibility for
        // race conditions which can cause clicks to register or allows them to be registered when
        // toggling panes. This logic filters out clicks unless the pane is hub search eligible.
        if (shouldOmitFocusedPaneForHubSearch(focusedPaneId)) return;

        mSearchActivityClient.requestOmniboxForResult(
                mSearchActivityClient
                        .newIntentBuilder()
                        .setPageUrl(new GURL(getOriginalNtpUrl()))
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
        return focusedPaneId != PaneId.TAB_GROUPS;
    }

    private void onPaneTransitionProgress(@Nullable PaneTransitionProgress progress) {
        if (progress == null) {
            mIgnoreSearchBoxClicks = false;
            mHasCheckedKeepSearchBoxOpen = false;
            mPropertyModel.set(SEARCH_BOX_TRANSITION_OFFSET, null);
            mPropertyModel.set(SEARCH_BOX_TRANSITION_IS_INCOGNITO, null);
            return;
        }

        maybeKeepSearchBoxOpen(progress);

        // The outgoing pane is judged by what is on screen, since pinned tabs can hide its search
        // box. The incoming pane shows its search box once it is focused.
        boolean fromHasSearchBox = isSearchBoxShowing(progress.fromPaneId);
        boolean toHasSearchBox = supportsSearchBox(progress.toPaneId);
        @PaneId
        Integer searchBoxPaneId =
                getTransitionSearchBoxPaneId(progress, fromHasSearchBox, toHasSearchBox);
        mIgnoreSearchBoxClicks = searchBoxPaneId != null;
        mPropertyModel.set(
                SEARCH_BOX_TRANSITION_IS_INCOGNITO,
                searchBoxPaneId == null ? null : isIncognitoPane(searchBoxPaneId));

        // The search box stays in place when both panes show it, and there is nothing to move
        // when neither does.
        if (fromHasSearchBox == toHasSearchBox) {
            mPropertyModel.set(SEARCH_BOX_TRANSITION_OFFSET, null);
            return;
        }

        // The search box moves with the pane that shows it. The outgoing pane moves away from its
        // resting position, while the incoming pane moves toward it.
        float offset = toHasSearchBox ? progress.progress - 1.0f : progress.progress;
        mPropertyModel.set(SEARCH_BOX_TRANSITION_OFFSET, progress.isSwipeLeft ? -offset : offset);
    }

    /**
     * Keeps the search box open once a pane transition lands on a pane that shows it. The search
     * box is already open by then, so a reveal the pane plays on landing should not collapse and
     * re-expand it.
     */
    private void maybeKeepSearchBoxOpen(@Nullable PaneTransitionProgress progress) {
        if (progress == null || mHasCheckedKeepSearchBoxOpen) return;

        Pane focusedPane = mPaneManager.getFocusedPaneSupplier().get();
        if (focusedPane == null || focusedPane.getPaneId() != progress.toPaneId) return;

        mHasCheckedKeepSearchBoxOpen = true;
        mKeepSearchBoxOpen = supportsSearchBox(progress.toPaneId);
    }

    /**
     * Returns the pane whose search box shows during a pane transition, or null if neither pane has
     * one. When both panes have one, the incoming pane takes over halfway through.
     */
    private @Nullable @PaneId Integer getTransitionSearchBoxPaneId(
            PaneTransitionProgress progress, boolean fromHasSearchBox, boolean toHasSearchBox) {
        if (fromHasSearchBox && toHasSearchBox) {
            return progress.progress < SEARCH_BOX_HINT_SWITCH_PROGRESS
                    ? progress.fromPaneId
                    : progress.toPaneId;
        }
        if (fromHasSearchBox) return progress.fromPaneId;
        if (toHasSearchBox) return progress.toPaneId;
        return null;
    }

    /** Returns whether the pane shows the incognito search hint. */
    private static boolean isIncognitoPane(@PaneId int paneId) {
        return paneId == PaneId.INCOGNITO_TAB_SWITCHER;
    }

    /** Returns whether the pane shows the search box once it is focused. */
    private boolean supportsSearchBox(@PaneId int paneId) {
        return !isScreenWidthTablet() && !shouldOmitFocusedPaneForHubSearch(paneId);
    }

    /** Returns whether the screen is as wide as a tablet's, where search uses the loupe. */
    private boolean isScreenWidthTablet() {
        return HubUtils.isScreenWidthTablet(
                mContext.getResources().getConfiguration().screenWidthDp);
    }

    /** Returns whether the pane's search box is showing, which pinned tabs can turn off. */
    private boolean isSearchBoxShowing(@PaneId int paneId) {
        if (!supportsSearchBox(paneId)) return false;

        Pane pane = mPaneManager.getPaneForId(paneId);
        return pane != null && pane.getHubSearchBoxVisibilitySupplier().get();
    }

    /** Test-only method to trigger configuration change for testing purposes. */
    void triggerConfigurationChangeForTesting(Configuration configuration) {
        mComponentCallbacks.onConfigurationChanged(configuration);
    }
}
