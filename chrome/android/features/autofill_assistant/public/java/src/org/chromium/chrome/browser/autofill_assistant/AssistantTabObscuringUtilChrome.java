// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.components.autofill_assistant.AssistantTabObscuringUtil;
import org.chromium.ui.util.TokenHolder;
/**
 * Implementation of {@link AssistantTabObscuringUtil} for Chrome.
 */
public class AssistantTabObscuringUtilChrome implements AssistantTabObscuringUtil {
    private final TabObscuringHandler mTabObscuringHandler;

    /** A token held while the Autofill Assistant is obscuring all tabs. */
    private int mObscuringToken = TokenHolder.INVALID_TOKEN;

    public AssistantTabObscuringUtilChrome(TabObscuringHandler tabObscuringHandler) {
        mTabObscuringHandler = tabObscuringHandler;
    }

    @Override
    public void obscureAllTabs() {
        if (mObscuringToken == TokenHolder.INVALID_TOKEN) {
            mTabObscuringHandler.obscureAllTabs();
        }
    }

    @Override
    public void unobscureAllTabs() {
        if (mObscuringToken != TokenHolder.INVALID_TOKEN) {
            mTabObscuringHandler.unobscureAllTabs(mObscuringToken);
            mObscuringToken = TokenHolder.INVALID_TOKEN;
        }
    }
}
