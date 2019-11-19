// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.signin.InvestigatedScenario;

/**
 * A bridge to call shared investigator logic.
 */
public final class SigninInvestigator {
    private SigninInvestigator() {}

    /**
     * Calls into native code to investigate potential ramifications of a
     * successful signin from the account corresponding to the given email.
     *
     * @return int value that corresponds to enum InvestigatedScenario.
     */
    public static @InvestigatedScenario int investigate(String currentEmail) {
        return SigninInvestigatorJni.get().investigate(currentEmail);
    }

    @NativeMethods
    interface Natives {
        int investigate(String currentEmail);
    }
}
