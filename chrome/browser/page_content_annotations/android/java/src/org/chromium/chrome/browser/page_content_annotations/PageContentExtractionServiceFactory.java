// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_content_annotations;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

@NullMarked
@JNINamespace("page_content_annotations")
public class PageContentExtractionServiceFactory {
    private PageContentExtractionServiceFactory() {}

    public static PageContentExtractionService getForProfile(Profile profile) {
        return PageContentExtractionServiceFactoryJni.get().getForProfile(profile);
    }

    @NativeMethods
    interface Natives {
        PageContentExtractionService getForProfile(@JniType("Profile*") Profile profile);
    }
}
