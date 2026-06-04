// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import android.graphics.RectF;
import android.view.View;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.Provider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.AsyncViewStub;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;

import java.util.List;
import java.util.function.BooleanSupplier;

/**
 * Empty implementation of {@link ManualFillingComponent} interface. Can be used as a base class for
 * mock/fake implementations in testing.
 */
@NullMarked
public class EmptyManualFillingComponent implements ManualFillingComponent {
    @Override
    public void initialize(
            WindowAndroid windowAndroid,
            Profile profile,
            BottomSheetController sheetController,
            BooleanSupplier isContextualSearchOpened,
            SoftKeyboardDelegate keyboardDelegate,
            BackPressManager backPressManager,
            MonotonicObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            InsetObserver insetObserver,
            AsyncViewStub sheetStub,
            AsyncViewStub barStub) {}

    @Override
    public void destroy() {}

    @Override
    public boolean onBackPressed() {
        return false;
    }

    @Override
    public void dismiss() {}

    @Override
    public void registerSheetDataProvider(
            WebContents webContents,
            @AccessoryTabType int sheetType,
            Provider<KeyboardAccessoryData.AccessorySheetData> sheetDataProvider) {}

    @Override
    public void registerSheetUpdateDelegate(
            WebContents webContents, UpdateAccessorySheetDelegate delegate) {}

    @Override
    public void registerActionProvider(
            WebContents webContents, Provider<KeyboardAccessoryData.Action[]> actionProvider) {}

    @Override
    public void setFieldBounds(RectF bounds) {}

    @Override
    public void setSuggestions(List<AutofillSuggestion> suggestions, AutofillDelegate delegate) {}

    @Override
    public void show(boolean waitForKeyboard, boolean isCredentialFieldOrHasAutofillSuggestions) {}

    @Override
    public void closeAccessorySheet() {}

    @Override
    public void swapSheetWithKeyboard() {}

    @Override
    public void hide() {}

    @Override
    public void showAccessorySheetTab(@AccessoryTabType int tabType) {}

    @Override
    public void setAtMemoryCallback(Runnable callback) {}

    @Override
    public void onResume() {}

    @Override
    public void onPause() {}

    @Override
    public boolean isFillingViewShown(View view) {
        return false;
    }

    @Override
    public NonNullObservableSupplier<Integer> getBottomInsetSupplier() {
        return ObservableSuppliers.alwaysZero();
    }

    @Override
    public boolean addObserver(Observer observer) {
        return false;
    }

    @Override
    public boolean removeObserver(Observer observer) {
        return false;
    }

    @Override
    public void confirmDeletionOperation(
            String title,
            CharSequence message,
            String confirmButtonText,
            Runnable confirmedCallback,
            Runnable declinedCallback) {}

    @Override
    public int getKeyboardExtensionHeight() {
        return 0;
    }

    @Override
    public void forceShowForTesting() {}

    @Override
    public MonotonicObservableSupplier<KeyboardAccessoryVisualStateProvider>
            getKeyboardAccessoryVisualStateProvider() {
        return ObservableSuppliers.alwaysNull();
    }

    @Override
    public MonotonicObservableSupplier<AccessorySheetVisualStateProvider>
            getAccessorySheetVisualStateProvider() {
        return ObservableSuppliers.alwaysNull();
    }

    @Override
    public void setWaitingForFetch(boolean waiting) {}

    @Override
    public void dismissIfWaitingForFetch() {}

    @Override
    public int handleBackPress() {
        return BackPressResult.FAILURE;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return ObservableSuppliers.alwaysFalse();
    }

    @Override
    public NonNullObservableSupplier<Boolean> getIsAccessoryRequestedSupplier() {
        return ObservableSuppliers.alwaysFalse();
    }
}
