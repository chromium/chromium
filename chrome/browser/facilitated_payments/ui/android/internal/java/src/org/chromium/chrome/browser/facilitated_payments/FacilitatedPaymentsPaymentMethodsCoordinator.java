// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.UNINITIALIZED;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.UI_EVENT_LISTENER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VISIBLE_STATE;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.HIDDEN;

import android.content.Context;
import android.content.pm.ResolveInfo;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.autofill.payments.Ewallet;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.facilitated_payments.core.metrics.FacilitatedPaymentsType;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/**
 * Implements the FacilitatedPaymentsPaymentMethodsComponent. It uses a bottom sheet to let the user
 * select a form of payment.
 */
@NullMarked
public class FacilitatedPaymentsPaymentMethodsCoordinator
        implements FacilitatedPaymentsPaymentMethodsComponent {
    private final FacilitatedPaymentsPaymentMethodsMediator mMediator =
            new FacilitatedPaymentsPaymentMethodsMediator();
    private @Nullable PropertyModel mFacilitatedPaymentsPaymentMethodsModel;

    @Override
    public void initialize(
            Context context,
            BottomSheetController bottomSheetController,
            @Nullable WindowAndroid windowAndroid,
            Delegate delegate,
            Profile profile) {
        mFacilitatedPaymentsPaymentMethodsModel = createModel(mMediator);
        mMediator.initialize(
                context, mFacilitatedPaymentsPaymentMethodsModel, delegate, profile, windowAndroid);
        setUpModelChangeProcessors(
                mFacilitatedPaymentsPaymentMethodsModel,
                new FacilitatedPaymentsPaymentMethodsView(context, bottomSheetController));
    }

    @Override
    public boolean isInLandscapeMode() {
        return mMediator.isInLandscapeMode();
    }

    @Override
    public void showSheetForPix(List<BankAccount> bankAccounts) {
        mMediator.showSheetForPix(bankAccounts);
    }

    @Override
    public void showSheetForPaymentLink(List<Ewallet> eWallets, List<ResolveInfo> apps) {
        mMediator.showSheetForPaymentLink(eWallets, apps);
    }

    @Override
    public void showProgressScreen() {
        mMediator.showProgressScreen();
    }

    @Override
    public void showErrorScreen() {
        mMediator.showErrorScreen();
    }

    @Override
    public void dismiss() {
        mMediator.dismiss();
    }

    @Override
    public void showPixAccountLinkingPrompt(int strikeCount, String accountEmail) {
        mMediator.showPixAccountLinkingPrompt(strikeCount, accountEmail);
    }

    @Override
    public void showPixAccountLinkingSuccessScreen() {
        mMediator.showPixAccountLinkingSuccessScreen();
    }

    @Override
    public void showAccountLinkingPrompt(
            @FacilitatedPaymentsType int fopType, String fopDisplayName, int strikeCount) {
        mMediator.showAccountLinkingPrompt(fopType, fopDisplayName, strikeCount);
    }

    @Override
    public void showAccountLinkingFailureNotification(@FacilitatedPaymentsType int fopType) {
        mMediator.showAccountLinkingFailureNotification(fopType);
    }

    /**
     * Connects the given model with the given view using Model Change Processors.
     *
     * @param model A {@link PropertyModel} built with {@link
     *     FacilitatedPaymentsPaymentMethodsProperties}.
     * @param view A {@link FacilitatedPaymentsPaymentMethodsView}.
     */
    @VisibleForTesting
    static void setUpModelChangeProcessors(
            PropertyModel model, FacilitatedPaymentsPaymentMethodsView view) {
        PropertyModelChangeProcessor.create(
                model,
                view,
                FacilitatedPaymentsPaymentMethodsViewBinder
                        ::bindFacilitatedPaymentsPaymentMethodsView);
    }

    PropertyModel createModel(FacilitatedPaymentsPaymentMethodsMediator mediator) {
        return new PropertyModel.Builder(FacilitatedPaymentsPaymentMethodsProperties.ALL_KEYS)
                .with(VISIBLE_STATE, HIDDEN)
                .with(SCREEN, UNINITIALIZED)
                .with(UI_EVENT_LISTENER, mediator::onUiEvent)
                .build();
    }

    @Nullable PropertyModel getModelForTesting() {
        return mFacilitatedPaymentsPaymentMethodsModel;
    }

    FacilitatedPaymentsPaymentMethodsMediator getMediatorForTesting() {
        return mMediator;
    }
}
