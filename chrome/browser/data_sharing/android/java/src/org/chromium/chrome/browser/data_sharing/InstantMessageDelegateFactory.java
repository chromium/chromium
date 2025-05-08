// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.collaboration.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

/** Creates/provides {@link InstantMessageDelegateImpl} by {@link Profile}. */
@NullMarked
public final class InstantMessageDelegateFactory {
    private static @Nullable ProfileKeyedMap<InstantMessageDelegateImpl> sProfileMap;
    private static @Nullable InstantMessageDelegateImpl sInstantMessageDelegateImplForTesting;

    // No instantiation.
    private InstantMessageDelegateFactory() {}

    /**
     * A factory method to create or retrieve a {@link InstantMessageDelegateImpl} object for a
     * given profile.
     *
     * @return The {@link InstantMessageDelegateImpl} for the given profile.
     */
    public static InstantMessageDelegateImpl getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        if (sInstantMessageDelegateImplForTesting != null) {
            return sInstantMessageDelegateImplForTesting;
        }

        if (sProfileMap == null) {
            sProfileMap = new ProfileKeyedMap<>(ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION);
        }

        return sProfileMap.getForProfile(profile, InstantMessageDelegateFactory::buildForProfile);
    }

    private static InstantMessageDelegateImpl buildForProfile(Profile profile) {
        profile = profile.getOriginalProfile();
        MessagingBackendService messagingBackendService =
                MessagingBackendServiceFactory.getForProfile(profile);
        DataSharingService dataSharingService = DataSharingServiceFactory.getForProfile(profile);
        TabGroupSyncService tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        assumeNonNull(tabGroupSyncService);
        return new InstantMessageDelegateImpl(
                messagingBackendService, dataSharingService, tabGroupSyncService);
    }

    /**
     * @param testService The {@InstantMessageDelegateImpl} to use for testing.
     */
    public static void setForTesting(@Nullable InstantMessageDelegateImpl testService) {
        sInstantMessageDelegateImplForTesting = testService;
        ResettersForTesting.register(() -> sInstantMessageDelegateImplForTesting = null);
    }
}
