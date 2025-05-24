// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.browsing_data.content.BrowsingDataModel;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

/**
 * Communicates between ClearBrowsingData, HatsService, ImportantSitesUtils (C++) and
 * ClearBrowsingDataFragment (Java UI).
 */
public final class BrowsingDataBridge implements Destroyable {
    private static ProfileKeyedMap<BrowsingDataBridge> sProfileMap;

    /**
     * List of observers to track the active tab in each {@link TabModelSelector}. This is used to
     * trigger the HaTS survey on the next page load.
     */
    private final List<CurrentTabObserver> mCurrentTabObservers = new ArrayList<>();

    private final Profile mProfile;

    /** Interface for a class that is listening to clear browser data events. */
    public interface OnClearBrowsingDataListener {
        @CalledByNative("OnClearBrowsingDataListener")
        void onBrowsingDataCleared();
    }

    /** Interface for a class that is fetching important site information. */
    public interface ImportantSitesCallback {
        /**
         * Called when the list of important registerable domains has been fetched from cpp. See
         * net/base/registry_controlled_domains/registry_controlled_domain.h for more details on
         * registrable domains and the current list of effective eTLDs.
         *
         * @param domains Important registerable domains.
         * @param exampleOrigins Example origins for each domain. These can be used to retrieve
         *     favicons.
         * @param importantReasons Bitfield of reasons why this domain was selected. Pass this back
         *     to clearBrowinsgData so we can record metrics.
         * @param dialogDisabled If the important dialog has been ignored too many times and should
         *     not be shown.
         */
        @CalledByNative("ImportantSitesCallback")
        void onImportantRegisterableDomainsReady(
                @JniType("std::vector<std::string>") String[] domains,
                @JniType("std::vector<std::string>") String[] exampleOrigins,
                @JniType("std::vector<int32_t>") int[] importantReasons,
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
            sProfileMap =
                    ProfileKeyedMap.createMapOfDestroyables(
                            ProfileKeyedMap.ProfileSelection.OWN_INSTANCE);
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
     * data. Do not use this method unless caller knows what they're doing. Not all backends are
     * supported yet, and more data than expected could be deleted. See crbug.com/113621.
     *
     * @param listener A listener to call back when the clearing is finished.
     * @param dataTypes An array of browsing data types to delete, represented as values from the
     *     shared enum {@link BrowsingDataType}.
     * @param timePeriod The time period for which to delete the data.
     * @param excludedDomains A list of registerable domains that we don't clear data for.
     * @param excludedDomainReasons A list of the reason metadata for the excluded domains.
     * @param ignoredDomains A list of ignored domains that the user chose to not exclude. We use
     *     these to remove important site entries if the user ignores them enough.
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
     *
     * @param dataTypes An array of browsing data types to delete, represented as values from the
     *     shared enum {@link BrowsingDataType}.
     * @param timePeriod The time period for which to delete the data.
     */
    public void clearBrowsingDataIncognitoForTesting(
            OnClearBrowsingDataListener listener, int[] dataTypes, @TimePeriod int timePeriod) {
        BrowsingDataBridgeJni.get()
                .clearBrowsingData(
                        mProfile.getPrimaryOtrProfile(/* createIfNeeded= */ true),
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
        BrowsingDataBridgeJni.get().requestInfoAboutOtherFormsOfBrowsingHistory(mProfile, listener);
    }

    /**
     * Checks the state of deletion preference for a certain browsing data type.
     *
     * @param dataType The requested browsing data type (from the shared enum {@link
     *     BrowsingDataType}).
     * @return The state of the corresponding deletion preference.
     */
    public boolean getBrowsingDataDeletionPreference(int dataType) {
        return BrowsingDataBridgeJni.get().getBrowsingDataDeletionPreference(mProfile, dataType);
    }

    /**
     * Sets the state of deletion preference for a certain browsing data type.
     *
     * @param dataType The requested browsing data type (from the shared enum {@link
     *     BrowsingDataType}).
     * @param value The state to be set.
     */
    public void setBrowsingDataDeletionPreference(int dataType, boolean value) {
        BrowsingDataBridgeJni.get().setBrowsingDataDeletionPreference(mProfile, dataType, value);
    }

    /**
     * Gets the time period for which browsing data will be deleted.
     *
     * @return The currently selected browsing data deletion time period.
     */
    public @TimePeriod int getBrowsingDataDeletionTimePeriod() {
        return BrowsingDataBridgeJni.get().getBrowsingDataDeletionTimePeriod(mProfile);
    }

    /**
     * Sets the time period for which browsing data will be deleted.
     *
     * @param timePeriod The selected browsing data deletion time period.
     */
    public void setBrowsingDataDeletionTimePeriod(@TimePeriod int timePeriod) {
        BrowsingDataBridgeJni.get().setBrowsingDataDeletionTimePeriod(mProfile, timePeriod);
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

    /**
     * Attempt to trigger the HaTS survey 5 seconds after the next page load on any {@link
     * TabModelSelector}.
     *
     * @param quickDelete True if the survey was requested for Quick Delete.
     */
    public void requestHatsSurvey(boolean quickDelete) {
        removeTabModelObservers();

        TabWindowManager tabWindowManager = TabWindowManagerSingleton.getInstance();
        for (int i = 0; i < tabWindowManager.getMaxSimultaneousSelectors(); i++) {
            var selector = tabWindowManager.getTabModelSelectorById(i);
            if (selector != null) {
                mCurrentTabObservers.add(
                        new CurrentTabObserver(
                                selector.getCurrentTabSupplier(),
                                new EmptyTabObserver() {
                                    @Override
                                    public void onLoadStarted(
                                            Tab tab, boolean toDifferentDocument) {
                                        WebContents webContents = tab.getWebContents();
                                        if (!tab.isOffTheRecord() && webContents != null) {
                                            BrowsingDataBridgeJni.get()
                                                    .triggerHatsSurvey(
                                                            mProfile, webContents, quickDelete);
                                            removeTabModelObservers();
                                        }
                                    }
                                },
                                /* swapCallback= */ null));
            }
        }
    }

    private void removeTabModelObservers() {
        for (CurrentTabObserver observer : mCurrentTabObservers) {
            observer.destroy();
        }

        mCurrentTabObservers.clear();
    }

    @Override
    public void destroy() {
        removeTabModelObservers();
    }

    @NativeMethods
    public interface Natives {
        void clearBrowsingData(
                @JniType("Profile*") Profile profile,
                OnClearBrowsingDataListener callback,
                @JniType("std::vector<int32_t>") int[] dataTypes,
                int timePeriod,
                @JniType("std::vector<std::string>") String[] excludedDomains,
                @JniType("std::vector<int32_t>") int[] excludedDomainReasons,
                @JniType("std::vector<std::string>") String[] ignoredDomains,
                @JniType("std::vector<int32_t>") int[] ignoredDomainReasons);

        void requestInfoAboutOtherFormsOfBrowsingHistory(
                @JniType("Profile*") Profile profile, OtherFormsOfBrowsingHistoryListener listener);

        void fetchImportantSites(
                @JniType("Profile*") Profile profile, ImportantSitesCallback callback);

        int getMaxImportantSites();

        void markOriginAsImportantForTesting(
                @JniType("Profile*") Profile profile, @JniType("std::string") String origin);

        boolean getBrowsingDataDeletionPreference(
                @JniType("Profile*") Profile profile, int dataType);

        void setBrowsingDataDeletionPreference(
                @JniType("Profile*") Profile profile, int dataType, boolean value);

        int getBrowsingDataDeletionTimePeriod(@JniType("Profile*") Profile profile);

        void setBrowsingDataDeletionTimePeriod(
                @JniType("Profile*") Profile profile, int timePeriod);

        void buildBrowsingDataModelFromDisk(
                @JniType("Profile*") Profile profile, Callback<BrowsingDataModel> callback);

        void triggerHatsSurvey(
                @JniType("Profile*") Profile profile,
                @JniType("content::WebContents*") WebContents webContents,
                boolean quickDelete);
    }
}
