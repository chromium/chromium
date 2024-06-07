// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import android.content.Context;
import android.util.Pair;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;

/** JNI wrapper for C++ TouchToFillPaymentMethodViewImpl. Delegates calls from native to Java. */
@JNINamespace("autofill")
class TouchToFillPaymentMethodViewBridge {
    private final TouchToFillPaymentMethodComponent mComponent;

    private TouchToFillPaymentMethodViewBridge(
            TouchToFillPaymentMethodComponent.Delegate delegate,
            Context context,
            PersonalDataManager personalDataManager,
            BottomSheetController bottomSheetController,
            WindowAndroid windowAndroid) {
        mComponent = new TouchToFillPaymentMethodCoordinator();
        mComponent.initialize(
                context,
                personalDataManager,
                bottomSheetController,
                delegate,
                new BottomSheetFocusHelper(bottomSheetController, windowAndroid));
    }

    @CalledByNative
    private static @Nullable TouchToFillPaymentMethodViewBridge create(
        TouchToFillPaymentMethodComponent.Delegate delegate,
            Profile profile,
            WindowAndroid windowAndroid) {
    if (windowAndroid == null) return null;
        Context context = windowAndroid.getContext().get();
        if (context == null) return null;
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) return null;
        return new TouchToFillPaymentMethodViewBridge(
                delegate,
                context,
                PersonalDataManagerFactory.getForProfile(profile),
                bottomSheetController,
                windowAndroid);
   }

    @CalledByNative
    private void showSheet(
            @JniType("std::vector") Object[] cards,
            boolean[] cardAcceptabilities,
            boolean shouldShowScanCreditCard) {
        assert cards.length == cardAcceptabilities.length;
        List<Pair<PersonalDataManager.CreditCard, Boolean>> cardsWithAcceptabilities =
                new ArrayList<>();
        for (int i = 0; i < cards.length; i++) {
            cardsWithAcceptabilities.add(
                    Pair.create((PersonalDataManager.CreditCard) cards[i], cardAcceptabilities[i]));
        }
        mComponent.showSheet(cardsWithAcceptabilities, shouldShowScanCreditCard);
    }

    @CalledByNative
    private void showSheet(@JniType("std::vector") List<PersonalDataManager.Iban> ibans) {
        mComponent.showSheet(ibans);
    }

    @CalledByNative
    private void hideSheet() {
        mComponent.hideSheet();
    }
}
