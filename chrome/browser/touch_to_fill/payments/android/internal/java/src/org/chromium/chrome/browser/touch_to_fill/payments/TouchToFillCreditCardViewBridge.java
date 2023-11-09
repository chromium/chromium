// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import android.content.Context;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * JNI wrapper for C++ TouchToFillCreditCardViewImpl. Delegates calls from native to Java.
 */
@JNINamespace("autofill")
class TouchToFillCreditCardViewBridge {
    private final TouchToFillCreditCardComponent mComponent;

    private TouchToFillCreditCardViewBridge(TouchToFillCreditCardComponent.Delegate delegate,
            Context context, BottomSheetController bottomSheetController,
            WindowAndroid windowAndroid) {
        mComponent = new TouchToFillCreditCardCoordinator();
        mComponent.initialize(context, bottomSheetController, delegate,
                new BottomSheetFocusHelper(bottomSheetController, windowAndroid));
    }

    @CalledByNative
    private static @Nullable TouchToFillCreditCardViewBridge create(
            TouchToFillCreditCardComponent.Delegate delegate, WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;
        Context context = windowAndroid.getContext().get();
        if (context == null) return null;
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) return null;
        return new TouchToFillCreditCardViewBridge(
                delegate, context, bottomSheetController, windowAndroid);
    }

    @CalledByNative
    private void showSheet(
            PersonalDataManager.CreditCard[] cards, boolean shouldShowScanCreditCard) {
        mComponent.showSheet(cards, shouldShowScanCreditCard);
    }

    @CalledByNative
    private void hideSheet() {
        mComponent.hideSheet();
    }

    @CalledByNative
    private static PersonalDataManager.CreditCard[] createCreditCardsArray(int size) {
        return new PersonalDataManager.CreditCard[size];
    }

    @CalledByNative
    private static void setCreditCard(PersonalDataManager.CreditCard[] creditCards, int index,
            PersonalDataManager.CreditCard creditCard) {
        creditCards[index] = creditCard;
    }
}
