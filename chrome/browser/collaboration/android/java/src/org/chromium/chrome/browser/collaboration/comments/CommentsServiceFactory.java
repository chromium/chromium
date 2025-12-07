// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration.comments;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.collaboration.comments.CommentsService;

/** This factory class creates a CommentsService for a given {@link Profile}. */
@JNINamespace("collaboration::comments::android")
@NullMarked
public final class CommentsServiceFactory {
    private static @Nullable CommentsService sCommentsServiceForTesting;

    /**
     * A factory method to create or retrieve a {@link CommentsService} object for a given profile.
     *
     * @return The {@link CommentsService} for the given profile.
     */
    public static CommentsService getForProfile(Profile profile) {
        if (sCommentsServiceForTesting != null) {
            return sCommentsServiceForTesting;
        }

        assert !profile.isOffTheRecord();

        return CommentsServiceFactoryJni.get().getForProfile(profile);
    }

    /**
     * Sets a {@link CommentsService} to use for testing. All subsequent calls to {@link
     * #getForProfile( Profile )} will return the test object rather than the real object.
     *
     * @param testService The {@link CommentsService} to use for testing, or null if the real
     *     service should be used.
     */
    public static void setForTesting(@Nullable CommentsService testService) {
        sCommentsServiceForTesting = testService;
        ResettersForTesting.register(() -> sCommentsServiceForTesting = null);
    }

    private CommentsServiceFactory() {}

    @NativeMethods
    interface Natives {
        CommentsService getForProfile(@JniType("Profile*") Profile profile);
    }
}
