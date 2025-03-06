// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubPaneHostProperties.COLOR_SCHEME;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.HAIRLINE_VISIBILITY;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.PANE_ROOT_VIEW;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.SNACKBAR_CONTAINER_CALLBACK;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.TransitiveObservableSupplier;
import org.chromium.ui.modelutil.PropertyModel;

/** Logic for hosting a single pane at a time in the Hub. */
public class HubPaneHostMediator {
    private final @NonNull Callback<Pane> mOnPaneChangeCallback = this::onPaneChange;
    private final @NonNull Callback<Boolean> mOnHairlineVisibilityChange =
            this::onHairlineVisibilityChange;
    private final @NonNull PropertyModel mPropertyModel;
    private final @NonNull ObservableSupplier<Pane> mPaneSupplier;
    private final @NonNull TransitiveObservableSupplier<Pane, Boolean> mHairlineVisibilitySupplier;

    /** Should be non-null after constructor finishes. */
    private ViewGroup mSnackbarContainer;

    /** Creates the mediator. */
    public HubPaneHostMediator(
            @NonNull PropertyModel propertyModel, @NonNull ObservableSupplier<Pane> paneSupplier) {
        mPropertyModel = propertyModel;
        mPaneSupplier = paneSupplier;
        mPaneSupplier.addObserver(mOnPaneChangeCallback);

        mHairlineVisibilitySupplier =
                new TransitiveObservableSupplier<>(
                        paneSupplier, p -> p.getHairlineVisibilitySupplier());
        mHairlineVisibilitySupplier.addObserver(mOnHairlineVisibilityChange);

        propertyModel.set(SNACKBAR_CONTAINER_CALLBACK, this::consumeSnackbarContainer);
    }

    /** Cleans up observers. */
    public void destroy() {
        mPropertyModel.set(PANE_ROOT_VIEW, null);
        mPaneSupplier.removeObserver(mOnPaneChangeCallback);
        mHairlineVisibilitySupplier.removeObserver(mOnHairlineVisibilityChange);
    }

    /** Returns the view group to contain the snackbar. */
    public ViewGroup getSnackbarContainer() {
        return mSnackbarContainer;
    }

    private void onPaneChange(@Nullable Pane pane) {
        @HubColorScheme int newColorScheme = HubColors.getColorSchemeSafe(pane);
        setNewColorScheme(newColorScheme, /* animate= */ true);
        View view = pane == null ? null : pane.getRootView();
        mPropertyModel.set(PANE_ROOT_VIEW, view);
    }

    private void setNewColorScheme(@HubColorScheme int newColorScheme, boolean animate) {
        @HubColorScheme
        int prevColorScheme =
                mPropertyModel.get(COLOR_SCHEME) == null
                        ? newColorScheme
                        : mPropertyModel.get(COLOR_SCHEME).newColorScheme;

        mPropertyModel.set(
                COLOR_SCHEME, new HubColorSchemeUpdate(newColorScheme, prevColorScheme, animate));
    }

    /** Sets the color scheme from the tab incognito status. */
    /* package */ void setNewColorSchemeFromTabIncognitoStatus(boolean isIncognito) {
        @HubColorScheme int colorScheme = HubColors.getColorSchemeFromIncognitoStatus(isIncognito);
        setNewColorScheme(colorScheme, /* animate= */ false);
    }

    private void onHairlineVisibilityChange(@Nullable Boolean visible) {
        mPropertyModel.set(HAIRLINE_VISIBILITY, Boolean.TRUE.equals(visible));
    }

    private void consumeSnackbarContainer(ViewGroup snackbarContainer) {
        mSnackbarContainer = snackbarContainer;
    }
}
