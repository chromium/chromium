// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlockingNoException;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.Calendar;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Helper class for testing AutofillProfiles.
 */
@JNINamespace("autofill")
public class AutofillTestHelper {
    private final CallbackHelper mOnPersonalDataChangedHelper = new CallbackHelper();

    public AutofillTestHelper() {
        registerDataObserver();
        setRequestTimeoutForTesting();
        setSyncServiceForTesting();
    }

    void setRequestTimeoutForTesting() {
        runOnUiThreadBlocking(() -> PersonalDataManager.setRequestTimeoutForTesting(0));
    }

    void setSyncServiceForTesting() {
        runOnUiThreadBlocking(() -> PersonalDataManager.getInstance().setSyncServiceForTesting());
    }

    AutofillProfile getProfile(final String guid) {
        return runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().getProfile(guid));
    }

    List<AutofillProfile> getProfilesToSuggest(final boolean includeNameInLabel) {
        return runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().getProfilesToSuggest(includeNameInLabel));
    }

    List<AutofillProfile> getProfilesForSettings() {
        return runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().getProfilesForSettings());
    }

    int getNumberOfProfilesToSuggest() {
        return getProfilesToSuggest(false).size();
    }

    int getNumberOfProfilesForSettings() {
        return getProfilesForSettings().size();
    }

    public String setProfile(final AutofillProfile profile) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        String guid = runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().setProfile(profile));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
        return guid;
    }

    public void deleteProfile(final String guid) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        runOnUiThreadBlocking(() -> PersonalDataManager.getInstance().deleteProfile(guid));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }

    public CreditCard getCreditCard(final String guid) {
        return runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().getCreditCard(guid));
    }

    List<CreditCard> getCreditCardsToSuggest() {
        return runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().getCreditCardsToSuggest());
    }

    List<CreditCard> getCreditCardsForSettings() {
        return runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().getCreditCardsForSettings());
    }

    int getNumberOfCreditCardsToSuggest() {
        return getCreditCardsToSuggest().size();
    }

    public int getNumberOfCreditCardsForSettings() {
        return getCreditCardsForSettings().size();
    }

    public String setCreditCard(final CreditCard card) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        String guid = runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().setCreditCard(card));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
        return guid;
    }

    public void addServerCreditCard(final CreditCard card) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        runOnUiThreadBlocking(
                () -> PersonalDataManager.getInstance().addServerCreditCardForTest(card));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }

    public void addServerCreditCard(final CreditCard card, String nickname, int cardIssuer)
            throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        runOnUiThreadBlocking(()
                                      -> PersonalDataManager.getInstance()
                                                 .addServerCreditCardForTestWithAdditionalFields(
                                                         card, nickname, cardIssuer));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }

    void deleteCreditCard(final String guid) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        runOnUiThreadBlocking(() -> PersonalDataManager.getInstance().deleteCreditCard(guid));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }

    /**
     * Records the use of the profile associated with the specified {@code guid}.. Effectively
     * increments the use count of the profile and set its use date to the current time. Also logs
     * usage metrics.
     *
     * @param guid The GUID of the profile.
     */
    void recordAndLogProfileUse(final String guid) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        runOnUiThreadBlocking(() -> PersonalDataManager.getInstance().recordAndLogProfileUse(guid));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }

    /**
     * Sets the use {@code count} and use {@code date} of the test profile associated with the
     * {@code guid}. This update is not saved to disk.
     *
     * @param guid The GUID of the profile to modify.
     * @param count The use count to assign to the profile. It should be non-negative.
     * @param daysSinceLastUsed The number of days since the profile was last used.
     */
    public void setProfileUseStatsForTesting(final String guid, final int count,
            final int daysSinceLastUsed) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        runOnUiThreadBlocking(
                ()
                        -> PersonalDataManager.getInstance().setProfileUseStatsForTesting(
                                guid, count, daysSinceLastUsed));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }

    /**
     * Get the use count of the test profile associated with the {@code guid}.
     *
     * @param guid The GUID of the profile to query.
     * @return The non-negative use count of the profile.
     */
    public int getProfileUseCountForTesting(final String guid) {
        return runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().getProfileUseCountForTesting(guid));
    }

    /**
     * Get the use date of the test profile associated with the {@code guid}.
     *
     * @param guid The GUID of the profile to query.
     * @return A non-negative long representing the last use date of the profile. It represents an
     *         absolute point in coordinated universal time (UTC) represented as microseconds since
     *         the Windows epoch. For more details see the comment header in time.h.
     */
    public long getProfileUseDateForTesting(final String guid) {
        return runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().getProfileUseDateForTesting(guid));
    }

    /**
     * Records the use of the credit card associated with the specified {@code guid}. Effectively
     * increments the use count of the credit card and sets its use date to the current time. Also
     * logs usage metrics.
     *
     * @param guid The GUID of the credit card.
     */
    public void recordAndLogCreditCardUse(final String guid) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        runOnUiThreadBlocking(
                () -> PersonalDataManager.getInstance().recordAndLogCreditCardUse(guid));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }

    /**
     * Sets the use {@code count} and use {@code date} of the test credit card associated with the
     * {@code guid}. This update is not saved to disk.
     *
     * @param guid The GUID of the credit card to modify.
     * @param count The use count to assign to the credit card. It should be non-negative.
     * @param daysSinceLastUsed The number of days since the credit card was last used.
     */
    public void setCreditCardUseStatsForTesting(final String guid, final int count,
            final int daysSinceLastUsed) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        runOnUiThreadBlocking(
                ()
                        -> PersonalDataManager.getInstance().setCreditCardUseStatsForTesting(
                                guid, count, daysSinceLastUsed));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }

    /**
     * Get the use count of the test credit card associated with the {@code guid}.
     *
     * @param guid The GUID of the credit card to query.
     * @return The non-negative use count of the credit card.
     */
    public int getCreditCardUseCountForTesting(final String guid) {
        return runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().getCreditCardUseCountForTesting(guid));
    }

    /**
     * Get the use date of the test credit card associated with the {@code guid}.
     *
     * @param guid The GUID of the credit card to query.
     * @return A non-negative long representing the last use date of the credit card. It represents
     *         an absolute point in coordinated universal time (UTC) represented as microseconds
     *         since the Windows epoch. For more details see the comment header in time.h.
     */
    public long getCreditCardUseDateForTesting(final String guid) {
        return runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().getCreditCardUseDateForTesting(guid));
    }

    /**
     * Get the current use date to be used in test to compare with credit card or profile use dates.
     *
     * @return A non-negative long representing the current date. It represents an absolute point in
     *         coordinated universal time (UTC) represented as microseconds since the Windows epoch.
     *         For more details see the comment header in time.h.
     */
    public long getCurrentDateForTesting() {
        return runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().getCurrentDateForTesting());
    }

    /**
     * Get a certain last use date to be used in tests with credit cards and profiles.
     *
     * @param days The number of days from today.
     * @return A non-negative long representing the time N days ago. It represents an absolute point
     *         in coordinated universal time (UTC) represented as microseconds since the Windows
     *         epoch. For more details see the comment header in time.h.
     */
    public long getDateNDaysAgoForTesting(final int days) {
        return runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().getDateNDaysAgoForTesting(days));
    }

    /**
     * Clears all local and server data, including server cards added via {@link
     * #addServerCreditCard(CreditCard)}}.
     */
    public void clearAllDataForTesting() throws TimeoutException {
        runOnUiThreadBlocking(() -> PersonalDataManager.getInstance().clearServerDataForTesting());
        runOnUiThreadBlocking(() -> PersonalDataManager.getInstance().clearImageDataForTesting());
        // Clear remaining local profiles and cards.
        for (AutofillProfile profile : getProfilesForSettings()) {
            if (profile.getIsLocal()) {
                runOnUiThreadBlocking(
                        () -> PersonalDataManager.getInstance().deleteProfile(profile.getGUID()));
            }
        }
        for (CreditCard card : getCreditCardsForSettings()) {
            if (card.getIsLocal()) {
                runOnUiThreadBlocking(
                        () -> PersonalDataManager.getInstance().deleteCreditCard(card.getGUID()));
            }
        }
        // Ensure all data is cleared. Waiting for a single callback for each operation is not
        // enough since tests or production code can also trigger callbacks and not consume them.
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        while (getProfilesForSettings().size() > 0 || getCreditCardsForSettings().size() > 0) {
            mOnPersonalDataChangedHelper.waitForCallback(callCount);
            callCount = mOnPersonalDataChangedHelper.getCallCount();
        }
    }

    /** Returns the YYYY value of the year after the current year. */
    public static String nextYear() {
        return String.valueOf(Calendar.getInstance().get(Calendar.YEAR) + 1);
    }

    /** Returns the YY value of the year after the current year. */
    public static String twoDigitNextYear() {
        return nextYear().substring(2);
    }

    /** Creates a simple {@link CreditCard}. */
    public static CreditCard createLocalCreditCard(
            String name, String number, String month, String year) {
        return new CreditCard("", "", true, false, name, number, "", month, year, "", 0, "", "");
    }

    /** Creates a virtual credit card. */
    public static CreditCard createVirtualCreditCard(String name, String number, String month,
            String year, String network, int iconId, String cardNameForAutofillDisplay,
            String obfuscatedLastFourDigits) {
        return new CreditCard(/* guid= */ "", /* origin= */ "", /* isLocal= */ false,
                /* isCached= */ false, /* isVirtual= */ true,
                /* name= */ name, /* number= */ number, /* networkAndLastFourDigits= */ "",
                /* month= */ month, /* year= */ year,
                /* basicCardIssuerNetwork =*/network, /* issuerIconDrawableId= */ iconId,
                /* billingAddressId= */ "",
                /* serverId= */ "", /* instrumentId= */ 0, /* cardLabel= */ "", /* nickname= */ "",
                /* cardArtUrl= */ new GURL(""),
                /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.ENROLLED,
                /* productDescription= */ "",
                /* cardNameForAutofillDisplay= */ cardNameForAutofillDisplay,
                /* obfuscatedLastFourDigits= */ obfuscatedLastFourDigits);
    }

    public static CreditCard createCreditCard(String name, String number, String month, String year,
            boolean isLocal, String nameForAutofillDisplay, String obfuscatedLastFourDigits,
            int iconId, String network) {
        return new CreditCard(/* guid= */ "",
                /* origin= */ "",
                /* isLocal= */ isLocal, /* isCached= */ false, /* isVirtual= */ false,
                /* name= */ name,
                /* number= */ number,
                /* obfuscatedNumber= */ "", /* month= */ month, year,
                /* basicCardIssuerNetwork =*/network,
                /* issuerIconDrawableId= */ iconId, /* billingAddressId= */ "",
                /* serverId= */ "", /* instrumentId= */ 0, /* cardLabel= */ "", /* nickname= */ "",
                /* cardArtUrl= */ null,
                /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.UNSPECIFIED,
                /* productDescription= */ "",
                /* cardNameForAutofillDisplay= */ nameForAutofillDisplay,
                /* obfuscatedLastFourDigits= */ obfuscatedLastFourDigits);
    }

    private void registerDataObserver() {
        try {
            int callCount = mOnPersonalDataChangedHelper.getCallCount();
            boolean isDataLoaded = runOnUiThreadBlockingNoException(
                    ()
                            -> PersonalDataManager.getInstance().registerDataObserver(
                                    () -> mOnPersonalDataChangedHelper.notifyCalled()));
            if (isDataLoaded) return;
            mOnPersonalDataChangedHelper.waitForCallback(callCount);
        } catch (TimeoutException e) {
            throw new AssertionError(e);
        }
    }

    // Disables minimum time that popup needs to be shown prior to click being processed.
    // Only has an effect if autofill popup is being shown.
    public static void disableThresholdForCurrentlyShownAutofillPopup(WebContents webContents) {
        AutofillTestHelperJni.get().disableThresholdForCurrentlyShownAutofillPopup(webContents);
    }

    @NativeMethods
    interface Natives {
        void disableThresholdForCurrentlyShownAutofillPopup(WebContents webContents);
    }
}
