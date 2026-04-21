// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN_VIEW_MODEL;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SURVIVES_NAVIGATION;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.ERROR_SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.FOP_SELECTOR;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.PIX_ACCOUNT_LINKING_PROMPT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.PROGRESS_SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.UNINITIALIZED;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.UI_EVENT_LISTENER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VISIBLE_STATE;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.HIDDEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.SHOWN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.SWAPPING_SCREEN;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link FacilitatedPaymentsPaymentMethodsProperties} changes in a
 * {@link PropertyModel} to the suitable method in {@link FacilitatedPaymentsPaymentMethodsView}.
 */
@NullMarked
class FacilitatedPaymentsPaymentMethodsViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link FacilitatedPaymentsPaymentMethodsView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindFacilitatedPaymentsPaymentMethodsView(
            PropertyModel model,
            FacilitatedPaymentsPaymentMethodsView view,
            PropertyKey propertyKey) {
        if (propertyKey == VISIBLE_STATE) {
            switch (model.get(VISIBLE_STATE)) {
                case HIDDEN:
                    view.setVisible(false);
                    break;
                case SHOWN:
                    view.setVisible(true);
                    break;
                default:
                    assert model.get(VISIBLE_STATE) == SWAPPING_SCREEN : "Undefined visible state";
            }
        } else if (propertyKey == SCREEN) {
            switch (model.get(SCREEN)) {
                case FOP_SELECTOR:
                    {
                        FacilitatedPaymentsSequenceView fopSelectorScreen =
                                new FacilitatedPaymentsFopSelectorScreen();
                        fopSelectorScreen.setupView(view.getScreenHolder());
                        view.setNextScreen(fopSelectorScreen);
                        model.set(SCREEN_VIEW_MODEL, fopSelectorScreen.getModel());
                        break;
                    }
                case PROGRESS_SCREEN:
                    {
                        FacilitatedPaymentsSequenceView progressScreen =
                                new FacilitatedPaymentsProgressScreen();
                        progressScreen.setupView(view.getScreenHolder());
                        view.setNextScreen(progressScreen);
                        model.set(SCREEN_VIEW_MODEL, progressScreen.getModel());
                        break;
                    }
                case ERROR_SCREEN:
                    {
                        FacilitatedPaymentsSequenceView errorScreen =
                                new FacilitatedPaymentsErrorScreen();
                        errorScreen.setupView(view.getScreenHolder());
                        view.setNextScreen(errorScreen);
                        model.set(SCREEN_VIEW_MODEL, errorScreen.getModel());
                        break;
                    }
                case PIX_ACCOUNT_LINKING_PROMPT:
                    {
                        FacilitatedPaymentsSequenceView pixAccountLinkingPrompt =
                                new PixAccountLinkingPrompt();
                        pixAccountLinkingPrompt.setupView(view.getScreenHolder());
                        view.setNextScreen(pixAccountLinkingPrompt);
                        model.set(SCREEN_VIEW_MODEL, pixAccountLinkingPrompt.getModel());
                        break;
                    }
                default:
                    assert model.get(SCREEN) == UNINITIALIZED : "Undefined screen type.";
            }
        } else if (propertyKey == SCREEN_VIEW_MODEL) {
            // This property contains the model to manipulate the {@link #SCREEN} view. No need to
            // update the {@code view} for this property. Intentional fall-through.
        } else if (propertyKey == UI_EVENT_LISTENER) {
            view.setUiEventListener(model.get(UI_EVENT_LISTENER));
        } else if (propertyKey == SURVIVES_NAVIGATION) {
            view.setSurvivesNavigation(model.get(SURVIVES_NAVIGATION));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private FacilitatedPaymentsPaymentMethodsViewBinder() {}
}
