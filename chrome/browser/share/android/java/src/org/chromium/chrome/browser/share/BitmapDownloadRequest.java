// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/** A Java API for requesting bitmap download from Chrome's download manager service. */
@NullMarked
public class BitmapDownloadRequest {
    public static void downloadBitmap(Profile profile, String fileName, Bitmap bitmap) {
        BitmapDownloadRequestJni.get().downloadBitmap(profile, fileName, bitmap);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        void downloadBitmap(
                @JniType("Profile*") Profile profile,
                @JniType("std::u16string") String fileName,
                @JniType("SkBitmap") Bitmap bitmap);
    }
}
