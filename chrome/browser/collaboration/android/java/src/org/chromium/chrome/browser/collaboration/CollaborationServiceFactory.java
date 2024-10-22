// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration;

import androidx.annotation.Nullable;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.collaboration.CollaborationService;

/** This factory creates CollaborationService for the given {@link Profile}. */
@JNINamespace("collaboration")
public final class CollaborationServiceFactory {
    private static CollaborationService sCollaborationServiceForTesting;

    /**
     * A factory method to create or retrieve a {@link CollaborationService} object for a given
     * profile.
     *
     * @return The {@link CollaborationService} for the given profile.
     */
    public static CollaborationService getForProfile(Profile profile) {
        if (sCollaborationServiceForTesting != null) {
            return sCollaborationServiceForTesting;
        }

        return CollaborationServiceFactoryJni.get().getForProfile(profile);
    }

    /**
     * Set a {@CollaborationService} to use for testing. All subsequent calls to {@link
     * #getForProfile( Profile )} will return the test object rather than the real object.
     *
     * @param testService The {@CollaborationService} to use for testing, or null if the real
     *     service should be used.
     */
    public static void setForTesting(@Nullable CollaborationService testService) {
        sCollaborationServiceForTesting = testService;
        ResettersForTesting.register(() -> sCollaborationServiceForTesting = null);
    }

    private CollaborationServiceFactory() {}

    @NativeMethods
    interface Natives {
        CollaborationService getForProfile(@JniType("Profile*") Profile profile);
    }
}
