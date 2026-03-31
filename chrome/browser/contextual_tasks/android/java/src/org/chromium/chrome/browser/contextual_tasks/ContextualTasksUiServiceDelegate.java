// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import android.app.Activity;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.WindowAndroid;

/** An interface to handle platform specific implementations of ContextualTasksUiService. */
@JNINamespace("contextual_tasks")
@NullMarked
public class ContextualTasksUiServiceDelegate {
    private final Profile mProfile;

    @CalledByNative
    private static ContextualTasksUiServiceDelegate create(Profile profile) {
        return new ContextualTasksUiServiceDelegate(profile);
    }

    private ContextualTasksUiServiceDelegate(Profile profile) {
        mProfile = profile;
    }

    @CalledByNative
    private void openFeedbackUi(WindowAndroid windowAndroid, String pageUrl) {
        Activity activity = windowAndroid.getActivity().get();
        assert activity != null : "ActivityWindowAndroid should have an Activity.";
        if (activity == null) {
            return;
        }

        HelpAndFeedbackLauncherFactory.getForProfile(mProfile)
                .showFeedback(activity, pageUrl, "cobrowse");
    }
}
