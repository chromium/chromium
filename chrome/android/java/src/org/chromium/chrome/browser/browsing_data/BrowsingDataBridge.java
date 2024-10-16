// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.components.browsing_data.content.BrowsingDataModel;

/**
 * Communicates between ClearBrowsingData, ImportantSitesUtils (C++) and ClearBrowsingDataFragment
 * (Java UI).
 */
public final class BrowsingDataBridge {
    private static ProfileKeyedMap<BrowsingDataBridge> sProfileMap;

    private final Profile mProfile;

    /** Interface for a class that is listening to clear browser data events. */
    public interface OnClearBrowsingDataListener {
        @CalledByNative("OnClearBrowsingDataListener")
        void onBrowsingDataCleared();
    }

    /** Interface for a class that is fetching important site information. */
    public interface ImportantSitesCallback {
        /**
         * Called when the list of important registerable domains has been fetched from cpp.
         * See net/base/registry_controlled_domains/registry_controlled_domain.h for more details on
         * registrable domains and the current list of effective eTLDs.
         * @param domains Important registerable domains.
         * @param exampleOrigins Example origins for each domain. These can be used to retrieve
         *                       favicons.
         * @param importantReasons Bitfield of reasons why this domain was selected. Pass this back
         *                         to clearBrowinsgData so we can record metrics.
         * @param dialogDisabled If the important dialog has been ignored too many times and should
         *                       not be shown.
         */
        @CalledByNative("ImportantSitesCallback")
        void onImportantRegisterableDomainsReady(
                String[] domains,
                String[] exampleOrigins,
                int[] importantReasons,
                boolean dialogDisabled);
    }

    /**
     * Interface to a class that receives callbacks instructing it to inform the user about other
     * forms of browsing history.
     */
    public interface OtherFormsOfBrowsingHistoryListener {
        /**
         * Called by the web history service when it discovers that other forms of browsing history
         * exist.
         */
        @CalledByNative("OtherFormsOfBrowsingHistoryListener")
        void enableDialogAboutOtherFormsOfBrowsingHistory();
    }

    private BrowsingDataBridge(Profile profile) {
        mProfile = profile;
    }

    /** Return the {@link BrowsingDataBridge} associated with the given {@link Profile}. */
    public static BrowsingDataBridge getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        if (sProfileMap == null) {
            sProfileMap = new ProfileKeyedMap<>(ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION);
        }
        return sProfileMap.getForProfile(profile, BrowsingDataBridge::new);
    }

    /**
     * Clear the specified types of browsing data asynchronously. |listener| is an object to be
     * notified when clearing completes. It can be null, but many operations (e.g. navigation) are
     * ill-advised while browsing data is being cleared.
     *
     * @param listener A listener to call back when the clearing is finished.
     * @param dataTypes An array of browsing data types to delete, represented as values from the
     *     shared enum {@link BrowsingDataType}.
     * @param timePeriod The time period for which to delete the data.
     */
    public void clearBrowsingData(
            OnClearBrowsingDataListener listener, int[] dataTypes, @TimePeriod int timePeriod) {
        clearBrowsingDataExcludingDomains(
                listener,
                dataTypes,
                timePeriod,
                new String[0],
                new int[0],
                new String[0],
                new int[0]);
    }

    /**
     * Same as above, but now we can specify a list of domains to exclude from clearing browsing
     * data.
     * Do not use this method unless caller knows what they're doing. Not all backends are supported
     * yet, and more data than expected could be deleted. See crbug.com/113621.
     * @param listener A listener to call back when the clearing is finished.
     * @param dataTypes An array of browsing data types to delete, represented as values from
     *                  the shared enum {@link BrowsingDataType}.
     * @param timePeriod The time period for which to delete the data.
     * @param excludedDomains A list of registerable domains that we don't clear data for.
     * @param excludedDomainReasons A list of the reason metadata for the excluded domains.
     * @param ignoredDomains A list of ignored domains that the user chose to not exclude. We use
     *                       these to remove important site entries if the user ignores them enough.
     * @param ignoredDomainReasons A list of reason metadata for the ignored domains.
     */
    public void clearBrowsingDataExcludingDomains(
            OnClearBrowsingDataListener listener,
            int[] dataTypes,
            @TimePeriod int timePeriod,
            String[] excludedDomains,
            int[] excludedDomainReasons,
            String[] ignoredDomains,
            int[] ignoredDomainReasons) {
        BrowsingDataBridgeJni.get()
                .clearBrowsingData(
                        BrowsingDataBridge.this,
                        mProfile,
                        listener,
                        dataTypes,
                        timePeriod,
                        excludedDomains,
                        excludedDomainReasons,
                        ignoredDomains,
                        ignoredDomainReasons);
    }

    /**
     * This method tests clearing of specified types of browsing data for primary Incognito profile.
     * @param dataTypes An array of browsing data types to delete, represented as values from
     *                  the shared enum {@link BrowsingDataType}.
     * @param timePeriod The time period for which to delete the data.
     */
    public void clearBrowsingDataIncognitoForTesting(
            OnClearBrowsingDataListener listener, int[] dataTypes, @TimePeriod int timePeriod) {
        BrowsingDataBridgeJni.get()
                .clearBrowsingData(
                        BrowsingDataBridge.this,
                        mProfile.getPrimaryOTRProfile(/* createIfNeeded= */ true),
                        listener,
                        dataTypes,
                        timePeriod,
                        new String[0],
                        new int[0],
                        new String[0],
                        new int[0]);
    }

    /**
     * This fetches sites (registerable domains) that we consider important. This combines many
     * pieces of information, including site engagement and permissions. The callback is called with
     * the list of important registerable domains.
     *
     * <p>See net/base/registry_controlled_domains/registry_controlled_domain.h for more details on
     * registrable domains and the current list of effective eTLDs.
     *
     * @param callback The callback that will be used to set the list of important sites.
     */
    public void fetchImportantSites(ImportantSitesCallback callback) {
        BrowsingDataBridgeJni.get().fetchImportantSites(mProfile, callback);
    }

    /**
     * @return The maximum number of important sites that will be returned from the call above.
     *         This is a constant that won't change.
     */
    public static int getMaxImportantSites() {
        return BrowsingDataBridgeJni.get().getMaxImportantSites();
    }

    /** This lets us mark an origin as important for testing. */
    public void markOriginAsImportantForTesting(String origin) {
        BrowsingDataBridgeJni.get().markOriginAsImportantForTesting(mProfile, origin);
    }

    /**
     * Requests that the web history service finds out if we should inform the user about the
     * existence of other forms of browsing history. The response will be asynchronous, through
     * {@link OtherFormsOfBrowsingHistoryListener}.
     */
    public void requestInfoAboutOtherFormsOfBrowsingHistory(
            OtherFormsOfBrowsingHistoryListener listener) {
        BrowsingDataBridgeJni.get()
                .requestInfoAboutOtherFormsOfBrowsingHistory(
                        BrowsingDataBridge.this, mProfile, listener);
    }

    /**
     * Checks the state of deletion preference for a certain browsing data type.
     * @param dataType The requested browsing data type (from the shared enum
     *      {@link BrowsingDataType}).
     * @param clearBrowsingDataTab Indicates if this is a checkbox on the default, basic or advanced
     *      tab to apply the right preference.
     * @return The state of the corresponding deletion preference.
     */
    public boolean getBrowsingDataDeletionPreference(int dataType, int clearBrowsingDataTab) {
        return BrowsingDataBridgeJni.get()
                .getBrowsingDataDeletionPreference(
                        BrowsingDataBridge.this, mProfile, dataType, clearBrowsingDataTab);
    }

    /**
     * Sets the state of deletion preference for a certain browsing data type.
     * @param dataType The requested browsing data type (from the shared enum
     *      {@link BrowsingDataType}).
     * @param clearBrowsingDataTab Indicates if this is a checkbox on the default, basic or advanced
     *      tab to apply the right preference.
     * @param value The state to be set.
     */
    public void setBrowsingDataDeletionPreference(
            int dataType, int clearBrowsingDataTab, boolean value) {
        BrowsingDataBridgeJni.get()
                .setBrowsingDataDeletionPreference(
                        BrowsingDataBridge.this, mProfile, dataType, clearBrowsingDataTab, value);
    }

    /**
     * Gets the time period for which browsing data will be deleted.
     * @param clearBrowsingDataTab Indicates if this is a timeperiod on the default, basic or
     *      advanced tab to apply the right preference.
     * @return The currently selected browsing data deletion time period.
     */
    public @TimePeriod int getBrowsingDataDeletionTimePeriod(int clearBrowsingDataTab) {
        return BrowsingDataBridgeJni.get()
                .getBrowsingDataDeletionTimePeriod(
                        BrowsingDataBridge.this, mProfile, clearBrowsingDataTab);
    }

    /**
     * Sets the time period for which browsing data will be deleted.
     * @param clearBrowsingDataTab Indicates if this is a timeperiod on the default, basic or
     *      advanced tab to apply the right preference.
     * @param timePeriod The selected browsing data deletion time period.
     */
    public void setBrowsingDataDeletionTimePeriod(
            int clearBrowsingDataTab, @TimePeriod int timePeriod) {
        BrowsingDataBridgeJni.get()
                .setBrowsingDataDeletionTimePeriod(
                        BrowsingDataBridge.this, mProfile, clearBrowsingDataTab, timePeriod);
    }

    /**
     * @return The index of the tab last visited by the user in the CBD dialog.
     *         Index 0 is for the basic tab, 1 is the advanced tab.
     */
    public int getLastSelectedClearBrowsingDataTab() {
        return BrowsingDataBridgeJni.get()
                .getLastClearBrowsingDataTab(BrowsingDataBridge.this, mProfile);
    }

    /**
     * Set the index of the tab last visited by the user.
     * @param tabIndex The last visited tab index, 0 for basic, 1 for advanced.
     */
    public void setLastSelectedClearBrowsingDataTab(int tabIndex) {
        BrowsingDataBridgeJni.get()
                .setLastClearBrowsingDataTab(BrowsingDataBridge.this, mProfile, tabIndex);
    }

    /**
     * Builds the `BrowsingDataModel` from disk and returns the object async.
     *
     * @param callback Callback runs with the BrowsingDataModel object when the model is built.
     */
    public static void buildBrowsingDataModelFromDisk(
            Profile profile, Callback<BrowsingDataModel> callback) {
        BrowsingDataBridgeJni.get().buildBrowsingDataModelFromDisk(profile, callback);
    }

    @CalledByNative
    private static void onBrowsingDataModelBuilt(
            Callback<BrowsingDataModel> callback, long nativeBrowsingDataModel) {
        callback.onResult(new BrowsingDataModel(nativeBrowsingDataModel));
    }

    @NativeMethods
    public interface Natives {
        void clearBrowsingData(
                BrowsingDataBridge caller,
                Profile profile,
                OnClearBrowsingDataListener callback,
                int[] dataTypes,
                int timePeriod,
                String[] excludedDomains,
                int[] excludedDomainReasons,
                String[] ignoredDomains,
                int[] ignoredDomainReasons);

        void requestInfoAboutOtherFormsOfBrowsingHistory(
                BrowsingDataBridge caller,
                Profile profile,
                OtherFormsOfBrowsingHistoryListener listener);

        void fetchImportantSites(Profile profile, ImportantSitesCallback callback);

        int getMaxImportantSites();

        void markOriginAsImportantForTesting(Profile profile, String origin);

        boolean getBrowsingDataDeletionPreference(
                BrowsingDataBridge caller, Profile profile, int dataType, int clearBrowsingDataTab);

        void setBrowsingDataDeletionPreference(
                BrowsingDataBridge caller,
                Profile profile,
                int dataType,
                int clearBrowsingDataTab,
                boolean value);

        int getBrowsingDataDeletionTimePeriod(
                BrowsingDataBridge caller, Profile profile, int clearBrowsingDataTab);

        void setBrowsingDataDeletionTimePeriod(
                BrowsingDataBridge caller,
                Profile profile,
                int clearBrowsingDataTab,
                int timePeriod);

        int getLastClearBrowsingDataTab(BrowsingDataBridge caller, Profile profile);

        void setLastClearBrowsingDataTab(BrowsingDataBridge caller, Profile profile, int lastTab);

        void buildBrowsingDataModelFromDisk(Profile profile, Callback<BrowsingDataModel> callback);
    }
}
