// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.chromium.chrome.browser.tab.TabStateStorageFlagHelper.isTabStorageEnabled;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/** Factory for fetching profile keyed {@link TabStateStorageService}. */
@JNINamespace("tabs")
@NullMarked
public final class TabStateStorageServiceFactory {
    private static final ScopedStorageBatch EMPTY_SCOPED_BATCH = () -> {};
    private static @Nullable TabStateStorageService sTabStateStorageServiceForTesting;

    public static @Nullable TabStateStorageService getForProfile(Profile profile) {
        if (sTabStateStorageServiceForTesting != null) {
            return sTabStateStorageServiceForTesting;
        }

        return TabStateStorageServiceFactoryJni.get().getForProfile(profile);
    }

    private TabStateStorageServiceFactory() {}

    /**
     * Creates a batch. This will batch write all save to storage operations performed during its
     * lifetime upon calling {@link ScopedStorageBatch#close()}.
     *
     * @param profile The profile associated with the save operations.
     */
    public static ScopedStorageBatch createBatch(Profile profile) {
        if (!isTabStorageEnabled()) return EMPTY_SCOPED_BATCH;

        TabStateStorageService service = TabStateStorageServiceFactory.getForProfile(profile);
        if (service == null) return EMPTY_SCOPED_BATCH;

        return service.createBatch();
    }

    /**
     * @param testService The test service to override with. Pass null to remove override.
     */
    public static void setForTesting(@Nullable TabStateStorageService testService) {
        sTabStateStorageServiceForTesting = testService;
        ResettersForTesting.register(() -> sTabStateStorageServiceForTesting = null);
    }

    @NativeMethods
    interface Natives {
        TabStateStorageService getForProfile(@JniType("Profile*") Profile profile);
    }
}
