// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubToolbarProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SHOW_ACTION_BUTTON_TEXT;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.TransitiveObservableSupplier;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Objects;
import java.util.stream.Collectors;

/** Logic for the toolbar of the Hub. */
public class HubToolbarMediator {
    private final @NonNull PropertyModel mPropertyModel;
    private final @NonNull PaneManager mPaneManager;
    private final @NonNull ObservableSupplier<Pane> mPaneSupplier;
    private final @NonNull Callback<Integer> mSharedFocusPaneCallback;
    // The order of entries in this map are the order the buttons should appear to the user. A null
    // value should not be shown to the user.
    private final LinkedHashMap<Integer, DisplayButtonData> mCachedPaneSwitcherButtonData =
            new LinkedHashMap<>();
    private final @NonNull Callback<FullButtonData> mOnActionButtonChangeCallback =
            this::onActionButtonChange;
    // Actual observers are curried with PaneId, making it difficult to unsubscribe. These runnables
    // are closures that contain the original lambda object reference. It also protects us from
    // changes in the returned panes or suppliers.
    private final @NonNull List<Runnable> mRemoveReferenceButtonObservers = new ArrayList<>();

    private @Nullable TransitiveObservableSupplier<Pane, FullButtonData> mActionButtonDataSupplier;

    /** Creates the mediator. */
    public HubToolbarMediator(
            @NonNull PropertyModel propertyModel, @NonNull PaneManager paneManager) {
        mPropertyModel = propertyModel;
        mPaneManager = paneManager;
        mPaneSupplier = paneManager.getFocusedPaneSupplier();
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
            DisplayButtonData currentButtonData = supplier.addObserver(observer);
            mCachedPaneSwitcherButtonData.put(paneId, currentButtonData);
            mRemoveReferenceButtonObservers.add(() -> supplier.removeObserver(observer));
        }
        rebuildPaneSwitcherButtonData();

        if (!HubFieldTrial.usesFloatActionButton()) {
            mActionButtonDataSupplier =
                    new TransitiveObservableSupplier<>(
                            mPaneSupplier, p -> p.getActionButtonDataSupplier());
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
    }

    private void onActionButtonChange(@Nullable FullButtonData actionButtonData) {
        mPropertyModel.set(ACTION_BUTTON_DATA, actionButtonData);
    }

    private void onReferenceButtonChange(@PaneId int paneId, @Nullable DisplayButtonData current) {
        @Nullable
        DisplayButtonData previous = mCachedPaneSwitcherButtonData.getOrDefault(paneId, null);
        if (!Objects.equals(current, previous)) {
            mCachedPaneSwitcherButtonData.put(paneId, current);
            rebuildPaneSwitcherButtonData();
        }
    }

    private void rebuildPaneSwitcherButtonData() {
        List<FullButtonData> buttonDataList =
                mCachedPaneSwitcherButtonData.entrySet().stream()
                        .filter(e -> e.getValue() != null)
                        .map(e -> wrapButtonData(e.getKey(), e.getValue()))
                        .collect(Collectors.toList());
        mPropertyModel.set(PANE_SWITCHER_BUTTON_DATA, buttonDataList);
        mPropertyModel.set(SHOW_ACTION_BUTTON_TEXT, buttonDataList.size() <= 1);
    }

    private FullButtonData wrapButtonData(
            @PaneId int paneId, @NonNull DisplayButtonData referenceButtonData) {
        Runnable onPress = mSharedFocusPaneCallback.bind(paneId);
        return new DelegateButtonData(referenceButtonData, onPress);
    }
}
