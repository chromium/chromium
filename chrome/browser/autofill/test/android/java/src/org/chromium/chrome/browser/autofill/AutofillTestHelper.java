// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.espresso.PerformException;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.util.HumanReadables;

import org.hamcrest.Matcher;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.autofill.AddressNormalizer;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.IbanRecordType;
import org.chromium.components.autofill.SubKeyRequester;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.url.GURL;

import java.util.Calendar;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Helper class for testing AutofillProfiles. */
@JNINamespace("autofill")
public class AutofillTestHelper {
    private final CallbackHelper mOnPersonalDataChangedHelper = new CallbackHelper();

    public AutofillTestHelper() {
        registerDataObserver();
        setRequestTimeoutForTesting();
        setSyncServiceForTesting();
    }

    void setRequestTimeoutForTesting() {
        runOnUiThreadBlocking(
                () -> {
                    AddressNormalizer.setRequestTimeoutForTesting(0);
                    SubKeyRequester.setRequestTimeoutForTesting(0);
                });
    }

    /**
     * Return the {@link PersonalDataManager} associated with {@link
     * ProfileManager#getLastUsedRegularProfile()}.
     */
    public static PersonalDataManager getPersonalDataManagerForLastUsedProfile() {
        return runOnUiThreadBlocking(
                () ->
                        PersonalDataManagerFactory.getForProfile(
                                ProfileManager.getLastUsedRegularProfile()));
    }

    void setSyncServiceForTesting() {
        runOnUiThreadBlocking(() -> AutofillTestHelperJni.get().setSyncService());
    }

    AutofillProfile getProfile(final String guid) {
        return runOnUiThreadBlocking(
                () -> getPersonalDataManagerForLastUsedProfile().getProfile(guid));
    }

    List<AutofillProfile> getProfilesToSuggest(final boolean includeNameInLabel) {
        return runOnUiThreadBlocking(
                () ->
                        getPersonalDataManagerForLastUsedProfile()
                                .getProfilesToSuggest(includeNameInLabel));
    }

    List<AutofillProfile> getProfilesForSettings() {
        return runOnUiThreadBlocking(
                () -> getPersonalDataManagerForLastUsedProfile().getProfilesForSettings());
    }

    int getNumberOfProfilesToSuggest() {
        return getProfilesToSuggest(false).size();
    }

    int getNumberOfProfilesForSettings() {
        return getProfilesForSettings().size();
    }

    public String setProfile(final AutofillProfile profile) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        String guid =
                runOnUiThreadBlocking(
                        () -> getPersonalDataManagerForLastUsedProfile().setProfile(profile));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
        return guid;
    }

    public void deleteProfile(final String guid) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        runOnUiThreadBlocking(() -> getPersonalDataManagerForLastUsedProfile().deleteProfile(guid));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }

    public CreditCard getCreditCard(final String guid) {
        return runOnUiThreadBlocking(
                () -> getPersonalDataManagerForLastUsedProfile().getCreditCard(guid));
    }

    List<CreditCard> getCreditCardsToSuggest() {
        return runOnUiThreadBlocking(
                () -> getPersonalDataManagerForLastUsedProfile().getCreditCardsToSuggest());
    }

    List<CreditCard> getCreditCardsForSettings() {
        return runOnUiThreadBlocking(
                () -> getPersonalDataManagerForLastUsedProfile().getCreditCardsForSettings());
    }

    int getNumberOfCreditCardsToSuggest() {
        return getCreditCardsToSuggest().size();
    }

    public int getNumberOfCreditCardsForSettings() {
        return getCreditCardsForSettings().size();
    }

    public String setCreditCard(final CreditCard card) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        String guid =
                runOnUiThreadBlocking(
                        () -> getPersonalDataManagerForLastUsedProfile().setCreditCard(card));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
        return guid;
    }

    public void addServerCreditCard(final CreditCard card) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        runOnUiThreadBlocking(() -> AutofillTestHelperJni.get().addServerCreditCard(card));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }

    public void addServerCreditCard(final CreditCard card, String nickname, int cardIssuer)
            throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        runOnUiThreadBlocking(
                () ->
                        AutofillTestHelperJni.get()
                                .addServerCreditCardWithAdditionalFields(
                                        card, nickname, cardIssuer));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }

    void deleteCreditCard(final String guid) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        runOnUiThreadBlocking(
                () -> getPersonalDataManagerForLastUsedProfile().deleteCreditCard(guid));
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
        runOnUiThreadBlocking(
                () -> getPersonalDataManagerForLastUsedProfile().recordAndLogProfileUse(guid));
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
    public void setProfileUseStatsForTesting(
            final String guid, final int count, final int daysSinceLastUsed)
            throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        runOnUiThreadBlocking(
                () ->
                        AutofillTestHelperJni.get()
                                .setProfileUseStats(guid, count, daysSinceLastUsed));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }

    /**
     * Get the use count of the test profile associated with the {@code guid}.
     *
     * @param guid The GUID of the profile to query.
     * @return The non-negative use count of the profile.
     */
    public int getProfileUseCountForTesting(final String guid) {
        return runOnUiThreadBlocking(() -> AutofillTestHelperJni.get().getProfileUseCount(guid));
    }

    /**
     * Get the use date of the test profile associated with the {@code guid}.
     *
     * @param guid The GUID of the profile to query.
     * @return A non-negative long representing the last use date of the profile. It represents an
     *     absolute point in coordinated universal time (UTC) represented as microseconds since the
     *     Windows epoch. For more details see the comment header in time.h.
     */
    public long getProfileUseDateForTesting(final String guid) {
        return runOnUiThreadBlocking(() -> AutofillTestHelperJni.get().getProfileUseDate(guid));
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
                () -> getPersonalDataManagerForLastUsedProfile().recordAndLogCreditCardUse(guid));
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
    public void setCreditCardUseStatsForTesting(
            final String guid, final int count, final int daysSinceLastUsed)
            throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        runOnUiThreadBlocking(
                () ->
                        AutofillTestHelperJni.get()
                                .setCreditCardUseStats(guid, count, daysSinceLastUsed));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }

    /**
     * Get the use count of the test credit card associated with the {@code guid}.
     *
     * @param guid The GUID of the credit card to query.
     * @return The non-negative use count of the credit card.
     */
    public int getCreditCardUseCountForTesting(final String guid) {
        return runOnUiThreadBlocking(() -> AutofillTestHelperJni.get().getCreditCardUseCount(guid));
    }

    /**
     * Get the use date of the test credit card associated with the {@code guid}.
     *
     * @param guid The GUID of the credit card to query.
     * @return A non-negative long representing the last use date of the credit card. It represents
     *     an absolute point in coordinated universal time (UTC) represented as microseconds since
     *     the Windows epoch. For more details see the comment header in time.h.
     */
    public long getCreditCardUseDateForTesting(final String guid) {
        return runOnUiThreadBlocking(() -> AutofillTestHelperJni.get().getCreditCardUseDate(guid));
    }

    /**
     * Get the current use date to be used in test to compare with credit card or profile use dates.
     *
     * @return A non-negative long representing the current date. It represents an absolute point in
     *     coordinated universal time (UTC) represented as microseconds since the Windows epoch. For
     *     more details see the comment header in time.h.
     */
    public long getCurrentDateForTesting() {
        return runOnUiThreadBlocking(() -> AutofillTestHelperJni.get().getCurrentDate());
    }

    /**
     * Get a certain last use date to be used in tests with credit cards and profiles.
     *
     * @param days The number of days from today.
     * @return A non-negative long representing the time N days ago. It represents an absolute point
     *     in coordinated universal time (UTC) represented as microseconds since the Windows epoch.
     *     For more details see the comment header in time.h.
     */
    public long getDateNDaysAgoForTesting(final int days) {
        return runOnUiThreadBlocking(() -> AutofillTestHelperJni.get().getDateNDaysAgo(days));
    }

    public void addServerIban(final Iban iban) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        runOnUiThreadBlocking(
                () -> getPersonalDataManagerForLastUsedProfile().addServerIbanForTest(iban));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }

    public Iban getIban(final String guid) {
        return runOnUiThreadBlocking(
                () -> getPersonalDataManagerForLastUsedProfile().getIban(guid));
    }

    Iban[] getIbansForSettings() throws TimeoutException {
        return runOnUiThreadBlocking(
                () -> getPersonalDataManagerForLastUsedProfile().getIbansForSettings());
    }

    public String addOrUpdateLocalIban(final Iban iban) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        String guid =
                runOnUiThreadBlocking(
                        () ->
                                getPersonalDataManagerForLastUsedProfile()
                                        .addOrUpdateLocalIban(iban));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
        return guid;
    }

    /**
     * Clears all local and server data, including server cards added via {@link
     * #addServerCreditCard(CreditCard)}}.
     */
    public void clearAllDataForTesting() throws TimeoutException {
        runOnUiThreadBlocking(() -> AutofillTestHelperJni.get().clearServerData());
        runOnUiThreadBlocking(
                () -> getPersonalDataManagerForLastUsedProfile().clearImageDataForTesting());
        // Clear remaining local profiles and cards.
        for (AutofillProfile profile : getProfilesForSettings()) {
            runOnUiThreadBlocking(
                    () ->
                            getPersonalDataManagerForLastUsedProfile()
                                    .deleteProfile(profile.getGUID()));
        }
        for (CreditCard card : getCreditCardsForSettings()) {
            if (card.getIsLocal()) {
                runOnUiThreadBlocking(
                        () ->
                                getPersonalDataManagerForLastUsedProfile()
                                        .deleteCreditCard(card.getGUID()));
            }
        }
        for (Iban iban : getIbansForSettings()) {
            if (iban.getRecordType() != IbanRecordType.SERVER_IBAN) {
                runOnUiThreadBlocking(
                        () ->
                                getPersonalDataManagerForLastUsedProfile()
                                        .deleteIban(iban.getGuid()));
            }
        }
        // Ensure all data is cleared. Waiting for a single callback for each operation is not
        // enough since tests or production code can also trigger callbacks and not consume them.
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        while (getProfilesForSettings().size() > 0
                || getCreditCardsForSettings().size() > 0
                || getIbansForSettings().length > 0) {
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
        return new CreditCard("", "", true, name, number, "", month, year, "", 0, "", "");
    }

    /** Creates a virtual credit card. */
    public static CreditCard createVirtualCreditCard(
            String name,
            String number,
            String month,
            String year,
            String network,
            int iconId,
            String cardNameForAutofillDisplay,
            String obfuscatedLastFourDigits) {
        return new CreditCard(
                /* guid= */ "",
                /* origin= */ "",
                /* isLocal= */ false,
                /* isVirtual= */ true,
                /* name= */ name,
                /* number= */ number,
                /* networkAndLastFourDigits= */ "",
                /* month= */ month,
                /* year= */ year,
                /* basicCardIssuerNetwork= */ network,
                /* issuerIconDrawableId= */ iconId,
                /* billingAddressId= */ "",
                /* serverId= */ "",
                /* instrumentId= */ 0,
                /* cardLabel= */ "",
                /* nickname= */ "",
                /* cardArtUrl= */ new GURL(""),
                /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.ENROLLED,
                /* productDescription= */ "",
                /* cardNameForAutofillDisplay= */ cardNameForAutofillDisplay,
                /* obfuscatedLastFourDigits= */ obfuscatedLastFourDigits,
                /* cvc= */ "",
                /* issuerId= */ "",
                /* productTermsUrl= */ null);
    }

    public static CreditCard createCreditCard(
            String name,
            String number,
            String month,
            String year,
            boolean isLocal,
            String nameForAutofillDisplay,
            String obfuscatedLastFourDigits,
            int iconId,
            String network) {
        return new CreditCard(
                /* guid= */ "",
                /* origin= */ "",
                /* isLocal= */ isLocal,
                /* isVirtual= */ false,
                /* name= */ name,
                /* number= */ number,
                /* obfuscatedNumber= */ "",
                /* month= */ month,
                year,
                /* basicCardIssuerNetwork= */ network,
                /* issuerIconDrawableId= */ iconId,
                /* billingAddressId= */ "",
                /* serverId= */ "",
                /* instrumentId= */ 0,
                /* cardLabel= */ "",
                /* nickname= */ "",
                /* cardArtUrl= */ null,
                /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.UNSPECIFIED,
                /* productDescription= */ "",
                /* cardNameForAutofillDisplay= */ nameForAutofillDisplay,
                /* obfuscatedLastFourDigits= */ obfuscatedLastFourDigits,
                /* cvc= */ "",
                /* issuerId= */ "",
                /* productTermsUrl= */ null);
    }

    public static AutofillSuggestion createCreditCardSuggestion(
            String label,
            String secondaryLabel,
            String subLabel,
            String secondarySubLabel,
            boolean applyDeactivatedStyle,
            boolean shouldDisplayTermsAvailable) {
        return new AutofillSuggestion.Builder()
                .setLabel(label)
                .setSecondaryLabel(secondaryLabel)
                .setSubLabel(subLabel)
                .setSecondarySubLabel(secondarySubLabel)
                .setApplyDeactivatedStyle(applyDeactivatedStyle)
                .setShouldDisplayTermsAvailable(shouldDisplayTermsAvailable)
                .build();
    }

    public static void addMaskedBankAccount(BankAccount bankAccount) {
        runOnUiThreadBlocking(() -> AutofillTestHelperJni.get().addMaskedBankAccount(bankAccount));
    }

    private void registerDataObserver() {
        try {
            int callCount = mOnPersonalDataChangedHelper.getCallCount();
            boolean isDataLoaded =
                    runOnUiThreadBlocking(
                            () -> {
                                return getPersonalDataManagerForLastUsedProfile()
                                        .registerDataObserver(
                                                mOnPersonalDataChangedHelper::notifyCalled);
                            });
            if (isDataLoaded) return;
            mOnPersonalDataChangedHelper.waitForCallback(callCount);
        } catch (TimeoutException e) {
            throw new AssertionError(e);
        }
    }

    // Creates an action which dispatches 2 motion events to the target view:
    // MotionEvent.ACTION_DOWN and MotionEvent.ACTION_UP.
    public static ViewAction createClickActionWithFlags(int flags) {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return isDisplayed();
            }

            @Override
            public String getDescription() {
                return "simulate click through another UI surface";
            }

            @Override
            public void perform(UiController uiController, View view) {
                if (!singleTouchView(view, flags)) {
                    throw new PerformException.Builder()
                            .withActionDescription(this.getDescription())
                            .withViewDescription(HumanReadables.describe(view))
                            .withCause(new RuntimeException("Couldn't click the view"))
                            .build();
                }
                uiController.loopMainThreadUntilIdle();
            }
        };
    }

    // Sends click event at the center of the `view` with the provided `flags`.
    private static boolean singleTouchView(View view, int flags) {
        int[] windowXY = new int[2];
        view.getLocationInWindow(windowXY);
        windowXY[0] += view.getWidth() / 2;
        windowXY[1] += view.getHeight() / 2;

        long downTime = SystemClock.uptimeMillis();
        View rootView = view.getRootView();
        if (!TouchCommon.dispatchTouchEvent(
                rootView,
                getMotionEventWithFlags(
                        downTime,
                        MotionEvent.ACTION_DOWN,
                        flags,
                        windowXY[0],
                        windowXY[1],
                        InputDevice.SOURCE_CLASS_POINTER))) {
            return false;
        }

        return TouchCommon.dispatchTouchEvent(
                rootView,
                getMotionEventWithFlags(
                        downTime,
                        MotionEvent.ACTION_UP,
                        flags,
                        windowXY[0],
                        windowXY[1],
                        InputDevice.SOURCE_CLASS_POINTER));
    }

    // Sends click event at the center of the `view` with the provided `flags`.
    public static boolean singleMouseClickView(View view) {
        int[] windowXY = new int[2];
        view.getLocationInWindow(windowXY);
        windowXY[0] += view.getWidth() / 2;
        windowXY[1] += view.getHeight() / 2;

        final long initiationTime = SystemClock.uptimeMillis();
        return dispatchMotionEvent(
                        view,
                        getMotionEvent(
                                initiationTime,
                                MotionEvent.ACTION_POINTER_DOWN,
                                windowXY,
                                InputDevice.SOURCE_MOUSE))
                && dispatchMotionEvent(
                        view,
                        getMotionEvent(
                                initiationTime,
                                MotionEvent.ACTION_POINTER_UP,
                                windowXY,
                                InputDevice.SOURCE_MOUSE));
    }

    private static boolean dispatchMotionEvent(View view, MotionEvent event) {
        return runOnUiThreadBlocking(() -> view.getRootView().dispatchGenericMotionEvent(event));
    }

    private static MotionEvent getMotionEvent(
            long downTime, int action, int[] windowXY, int device) {
        return getMotionEventWithFlags(downTime, action, 0x0, windowXY[0], windowXY[1], device);
    }

    private static MotionEvent getMotionEventWithFlags(
            long downTime, int action, int flags, int x, int y, int device) {
        MotionEvent.PointerProperties props = new MotionEvent.PointerProperties();
        props.id = 0;
        MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        coords.x = x;
        coords.y = y;
        coords.pressure = 1.0f;
        coords.size = 1.0f;
        return MotionEvent.obtain(
                /* downTime= */ downTime,
                /* eventTime= */ SystemClock.uptimeMillis(),
                /* action= */ action,
                /* pointerCount= */ 1,
                new MotionEvent.PointerProperties[] {props},
                new MotionEvent.PointerCoords[] {coords},
                /* metaState= */ 0,
                /* buttonState= */ 0,
                /* xPrecision= */ 1.0f,
                /* yPrecision= */ 1.0f,
                /* deviceId= */ 0,
                /* edgeFlags= */ 0,
                /* source= */ device,
                /* flags= */ flags);
    }

    @NativeMethods
    interface Natives {
        long getDateNDaysAgo(int days);

        void addServerCreditCard(CreditCard card);

        void addServerCreditCardWithAdditionalFields(
                CreditCard card, String nickname, int cardIssuer);

        void setProfileUseStats(String guid, int count, int daysSinceLastUsed);

        int getProfileUseCount(String guid);

        long getProfileUseDate(String guid);

        void setCreditCardUseStats(String guid, int count, int daysSinceLastUsed);

        int getCreditCardUseCount(String guid);

        long getCreditCardUseDate(String guid);

        long getCurrentDate();

        void clearServerData();

        void setSyncService();

        void addMaskedBankAccount(BankAccount bankAccount);
    }
}
