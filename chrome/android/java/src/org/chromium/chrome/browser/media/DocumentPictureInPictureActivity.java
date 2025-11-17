// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.profiles.ProfileProvider;

@NullMarked
public class DocumentPictureInPictureActivity extends AsyncInitializationActivity {
    public static final String WEB_CONTENTS_KEY =
            "org.chromium.chrome.browser.media.DocumentPictureInPicture.WebContents";

    @Override
    protected void triggerLayoutInflation() {
        onInitialLayoutInflationComplete();
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        // TODO(https://crbug.com/422715286): implement this method.
        return new OneshotSupplierImpl<>();
    }
}
