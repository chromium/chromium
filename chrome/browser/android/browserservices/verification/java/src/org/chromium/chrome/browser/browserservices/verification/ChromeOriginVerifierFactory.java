// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.verification;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsService;

import org.chromium.base.ResettersForTesting;
import org.chromium.content_public.browser.WebContents;

/** A factory that creates instances of {@link ChromeOriginVerifier}. */
public class ChromeOriginVerifierFactory {
    private static ChromeOriginVerifier sInstanceForTests;

    public static void setInstanceForTesting(ChromeOriginVerifier verifier) {
        sInstanceForTests = verifier;
        ResettersForTesting.register(() -> sInstanceForTests = null);
    }

    public static ChromeOriginVerifier create(
            String packageName,
            @CustomTabsService.Relation int relation,
            @Nullable WebContents webContents) {
        if (sInstanceForTests != null) return sInstanceForTests;
        return new ChromeOriginVerifier(
                packageName, relation, webContents, ChromeVerificationResultStore.getInstance());
    }
}
