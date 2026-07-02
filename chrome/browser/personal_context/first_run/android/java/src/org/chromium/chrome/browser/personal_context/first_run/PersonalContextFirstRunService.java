// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.personal_context.first_run;

import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/** Java proxy for the C++ PersonalContextFirstRunService. */
@NullMarked
public class PersonalContextFirstRunService {

    public static boolean shouldShowAmbientAutofillNotice(Profile profile) {
        return PersonalContextFirstRunServiceJni.get().shouldShowAmbientAutofillNotice(profile);
    }

    public static void ambientAutofillNoticeAcknowledged(Profile profile) {
        PersonalContextFirstRunServiceJni.get().ambientAutofillNoticeAcknowledged(profile);
    }

    public static boolean shouldShowAtMemoryNotice(Profile profile) {
        return PersonalContextFirstRunServiceJni.get().shouldShowAtMemoryNotice(profile);
    }

    public static void atMemoryNoticeAcknowledged(Profile profile) {
        PersonalContextFirstRunServiceJni.get().atMemoryNoticeAcknowledged(profile);
    }

    @NativeMethods
    public interface Natives {
        boolean shouldShowAmbientAutofillNotice(Profile profile);

        void ambientAutofillNoticeAcknowledged(Profile profile);

        boolean shouldShowAtMemoryNotice(Profile profile);

        void atMemoryNoticeAcknowledged(Profile profile);
    }
}
