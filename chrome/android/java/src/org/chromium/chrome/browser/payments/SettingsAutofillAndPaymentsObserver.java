// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.ArrayList;
import java.util.List;

/** The class to observe address and card changes in 'settings/Autofill and payment'. */
public class SettingsAutofillAndPaymentsObserver {
    private static final List<Observer> sObservers = new ArrayList<>();
    private static SettingsAutofillAndPaymentsObserver sSettingsAutofillAndPaymentsObserver;

    /** The interface to observe address and card changes in 'settings/Autofill and payment'. */
    public interface Observer {
        /**
         * Called when user updates or adds an address.
         *
         * @param address The updated or added address.
         */
        void onAddressUpdated(AutofillAddress address);

        /**
         * Called when user deletes an address.
         *
         * @param guid The guid of the address.
         */
        void onAddressDeleted(String guid);

        /**
         * Called when user updates or adds a credit card.
         *
         * @param card The updated or added card.
         */
        void onCreditCardUpdated(CreditCard card);

        /**
         * Called when user deletes a credit card.
         *
         * @param guid The guid of the card.
         */
        void onCreditCardDeleted(String guid);
    }

    /** Gets an instance of this class. */
    public static SettingsAutofillAndPaymentsObserver getInstance() {
        ThreadUtils.assertOnUiThread();

        if (sSettingsAutofillAndPaymentsObserver == null) {
            sSettingsAutofillAndPaymentsObserver = new SettingsAutofillAndPaymentsObserver();
        }
        return sSettingsAutofillAndPaymentsObserver;
    }

    // Avoid accident instantiation.
    private SettingsAutofillAndPaymentsObserver() {}

    /**
     * Registers the given observer.
     *
     * @param observer The observer to register.
     */
    public void registerObserver(Observer observer) {
        sObservers.add(observer);
    }

    /**
     * Unregisters the given observer.
     *
     * @param observer The observer to remove.
     */
    public void unregisterObserver(Observer observer) {
        sObservers.remove(observer);
    }

    /**
     * Notify the given address has been updated.
     *
     * @param address The given address.
     */
    public void notifyOnAddressUpdated(AutofillAddress address) {
        for (Observer observer : sObservers) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                @Override
                public void run() {
                    observer.onAddressUpdated(address);
                }
            });
        }
    }

    /**
     * Notify the given address has been deleted.
     *
     * @param guid The given address guid.
     */
    public void notifyOnAddressDeleted(String guid) {
        for (Observer observer : sObservers) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                @Override
                public void run() {
                    observer.onAddressDeleted(guid);
                }
            });
        }
    }

    /**
     * Notify the given card has been updated.
     *
     * @param card The given card.
     */
    public void notifyOnCreditCardUpdated(CreditCard card) {
        for (Observer observer : sObservers) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                @Override
                public void run() {
                    observer.onCreditCardUpdated(card);
                }
            });
        }
    }

    /**
     * Notify the given card has been deleted.
     *
     * @param guid The given card guid.
     */
    public void notifyOnCreditCardDeleted(String guid) {
        for (Observer observer : sObservers) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                @Override
                public void run() {
                    observer.onCreditCardDeleted(guid);
                }
            });
        }
    }
}