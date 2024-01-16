// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubToolbarProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.COLOR_SCHEME;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_INDEX;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SHOW_ACTION_BUTTON_TEXT;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.util.Pair;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.TransitiveObservableSupplier;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/** Logic for the toolbar of the Hub. */
public class HubToolbarMediator {
    private static final int INVALID_PANE_SWITCHER_INDEX = -1;

    private final @NonNull PropertyModel mPropertyModel;

    private final @NonNull Callback<FullButtonData> mOnActionButtonChangeCallback =
            this::onActionButtonChange;
    private @Nullable TransitiveObservableSupplier<Pane, FullButtonData> mActionButtonDataSupplier;

    private final @NonNull ObservableSupplier<Pane> mFocusedPaneSupplier;
    private final @NonNull Callback<Integer> mSharedFocusPaneCallback;
    // The order of entries in this map are the order the buttons should appear to the user. A null
    // value should not be shown to the user.
    private final ArrayList<Pair<Integer, DisplayButtonData>> mCachedPaneSwitcherButtonData =
            new ArrayList<>();
    // Actual observers are curried with PaneId, making it difficult to unsubscribe. These runnables
    // are closures that contain the original lambda object reference. It also protects us from
    // changes in the returned panes or suppliers.
    private final @NonNull List<Runnable> mRemoveReferenceButtonObservers = new ArrayList<>();
    private final @NonNull Callback<Pane> mOnFocusedPaneChange = this::onFocusedPaneChange;

    /** Creates the mediator. */
    public HubToolbarMediator(
            @NonNull PropertyModel propertyModel, @NonNull PaneManager paneManager) {
        mPropertyModel = propertyModel;
        mFocusedPaneSupplier = paneManager.getFocusedPaneSupplier();
        mSharedFocusPaneCallback = paneManager::focusPane;

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
        mFocusedPaneSupplier.addObserver(mOnFocusedPaneChange);
        rebuildPaneSwitcherButtonData();

        if (!HubFieldTrial.usesFloatActionButton()) {
            mActionButtonDataSupplier =
                    new TransitiveObservableSupplier<>(
                            mFocusedPaneSupplier, p -> p.getActionButtonDataSupplier());
            mActionButtonDataSupplier.addObserver(mOnActionButtonChangeCallback);
        }
    }

    /** Cleans up observers. */
    public void destroy() {
        if (mActionButtonDataSupplier != null) {
            mActionButtonDataSupplier.removeObserver(mOnActionButtonChangeCallback);
            mActionButtonDataSupplier = null;
        }
        mRemoveReferenceButtonObservers.stream().forEach(r -> r.run());
        mRemoveReferenceButtonObservers.clear();
        mFocusedPaneSupplier.removeObserver(mOnFocusedPaneChange);
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
        @Nullable Pane focusedPane = mFocusedPaneSupplier.get();
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
            return;
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
        Runnable onPress = mSharedFocusPaneCallback.bind(paneId);
        return new DelegateButtonData(referenceButtonData, onPress);
    }
}
