// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.android_webview.autofill.AndroidAutofillSafeModeAction;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.origin_trial.DisableOriginTrialsSafeModeAction;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingSafeModeAction;
import org.chromium.android_webview.variations.FastVariationsSeedSafeModeAction;
import org.chromium.android_webview.variations.VariationsSeedSafeModeAction;

/** Exposes the SafeModeActions supported by the browser process. */
@Lifetime.Singleton
public final class BrowserSafeModeActionList {
    // Do not instantiate this class.
    private BrowserSafeModeActionList() {}

    /**
     * A list of SafeModeActions supported in the browser process. The set of actions to be executed
     * will be specified by the nonembedded SafeModeService, however each action (if specified by
     * the service) will be executed in the order listed below.
     */
    public static final SafeModeAction[] sList = {
        new VariationsSeedSafeModeAction(),
        new AndroidAutofillSafeModeAction(),
        new NoopSafeModeAction(),
        new FastVariationsSeedSafeModeAction(),
        new AwSafeBrowsingSafeModeAction(),
        new DisableOriginTrialsSafeModeAction(),
    };
}
