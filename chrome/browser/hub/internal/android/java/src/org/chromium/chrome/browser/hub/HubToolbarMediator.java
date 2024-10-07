// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubToolbarProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.COLOR_SCHEME;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.MENU_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_BUTTON_LOOKUP_CALLBACK;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_INDEX;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_BOX_LISTENER;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SHOW_ACTION_BUTTON_TEXT;

import android.app.Activity;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.util.Pair;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.TransitiveObservableSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.HubToolbarProperties.PaneButtonLookup;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/** Logic for the toolbar of the Hub. */
public class HubToolbarMediator {
    private static final int INVALID_PANE_SWITCHER_INDEX = -1;

    private final @NonNull Activity mActivity;
    private final @NonNull PropertyModel mPropertyModel;

    private final @NonNull Callback<FullButtonData> mOnActionButtonChangeCallback =
            this::onActionButtonChange;
    private @Nullable TransitiveObservableSupplier<Pane, FullButtonData> mActionButtonDataSupplier;

    private final @NonNull PaneManager mPaneManager;
    private final @NonNull Tracker mTracker;
    private final @NonNull SearchActivityClient mSearchActivityClient;
    // The order of entries in this map are the order the buttons should appear to the user. A null
    // value should not be shown to the user.
    private final ArrayList<Pair<Integer, DisplayButtonData>> mCachedPaneSwitcherButtonData =
            new ArrayList<>();
    // Actual observers are curried with PaneId, making it difficult to unsubscribe. These runnables
    // are closures that contain the original lambda object reference. It also protects us from
    // changes in the returned panes or suppliers.
    private final @NonNull List<Runnable> mRemoveReferenceButtonObservers = new ArrayList<>();
    private final @NonNull Callback<Pane> mOnFocusedPaneChange = this::onFocusedPaneChange;

    private @Nullable PaneButtonLookup mPaneButtonLookup;

    /** Creates the mediator. */
    public HubToolbarMediator(
            @NonNull Activity activity,
            @NonNull PropertyModel propertyModel,
            @NonNull PaneManager paneManager,
            @NonNull Tracker tracker,
            @NonNull SearchActivityClient searchActivityClient) {
        mActivity = activity;
        mPropertyModel = propertyModel;
        mPaneManager = paneManager;
        mTracker = tracker;
        mSearchActivityClient = searchActivityClient;

        for (@PaneId int paneId : paneManager.getPaneOrderController().getPaneOrder()) {
            @Nullable Pane pane = paneManager.getPaneForId(paneId);
            if (pane == null) continue;

            @NonNull
            ObservableSupplier<DisplayButtonData> supplier = pane.getReferenceButtonDataSupplier();
            Callback<DisplayButtonData> observer = (data) -> onReferenceButtonChange(paneId, data);

            // If the supplier already has data, this will post a callback to run our observer. But
            // we do not want this. We don't want to rebuild the button data list n times. Instead
            // all of these posted events should have data identical to what we initialize our cache
            // to, and they should all no-op.
            @Nullable DisplayButtonData currentButtonData = supplier.addObserver(observer);
            mCachedPaneSwitcherButtonData.add(new Pair<>(paneId, currentButtonData));

            mRemoveReferenceButtonObservers.add(() -> supplier.removeObserver(observer));
        }
        ObservableSupplier<Pane> focusedPaneSupplier = paneManager.getFocusedPaneSupplier();
        focusedPaneSupplier.addObserver(mOnFocusedPaneChange);
        rebuildPaneSwitcherButtonData();

        if (!HubFieldTrial.usesFloatActionButton()) {
            mActionButtonDataSupplier =
                    new TransitiveObservableSupplier<>(
                            focusedPaneSupplier, p -> p.getActionButtonDataSupplier());
            mActionButtonDataSupplier.addObserver(mOnActionButtonChangeCallback);
        }

        mPropertyModel.set(PANE_BUTTON_LOOKUP_CALLBACK, this::consumeButtonLookup);

        mPropertyModel.set(SEARCH_BOX_VISIBLE, ChromeFeatureList.sAndroidHubSearch.isEnabled());
        mPropertyModel.set(SEARCH_BOX_LISTENER, this::onSearchClicked);
    }

    /** Cleans up observers. */
    public void destroy() {
        if (mActionButtonDataSupplier != null) {
            mActionButtonDataSupplier.removeObserver(mOnActionButtonChangeCallback);
            mActionButtonDataSupplier = null;
        }
        mRemoveReferenceButtonObservers.stream().forEach(r -> r.run());
        mRemoveReferenceButtonObservers.clear();
        mPaneManager.getFocusedPaneSupplier().removeObserver(mOnFocusedPaneChange);
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
        mPropertyModel.set(SHOW_ACTION_BUTTON_TEXT, buttonDataList.size() <= 1);
    }

    private void onFocusedPaneChange(@Nullable Pane focusedPane) {
        mPropertyModel.set(COLOR_SCHEME, HubColors.getColorSchemeSafe(focusedPane));

        @Nullable Integer focusedPaneId = focusedPane == null ? null : focusedPane.getPaneId();
        if (focusedPaneId == null) {
            mPropertyModel.set(PANE_SWITCHER_INDEX, INVALID_PANE_SWITCHER_INDEX);
            mPropertyModel.set(MENU_BUTTON_VISIBLE, false);
            mPropertyModel.set(IS_INCOGNITO, false);
            return;
        } else {
            mPropertyModel.set(MENU_BUTTON_VISIBLE, focusedPane.getMenuButtonVisible());

            boolean isIncognito = focusedPaneId == PaneId.INCOGNITO_TAB_SWITCHER;
            mPropertyModel.set(IS_INCOGNITO, isIncognito);
        }

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

    private FullButtonData wrapButtonData(
            @PaneId int paneId, @NonNull DisplayButtonData referenceButtonData) {
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

    private void consumeButtonLookup(PaneButtonLookup paneButtonLookup) {
        mPaneButtonLookup = paneButtonLookup;
    }

    private void onSearchClicked() {
        mSearchActivityClient.requestOmniboxForResult(
                mActivity,
                new GURL(UrlConstants.NTP_NON_NATIVE_URL),
                IntentOrigin.HUB,
                null,
                mPropertyModel.get(IS_INCOGNITO));
    }
}
