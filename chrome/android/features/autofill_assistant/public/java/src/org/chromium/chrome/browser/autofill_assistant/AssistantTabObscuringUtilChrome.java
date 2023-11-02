// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.components.autofill_assistant.AssistantTabObscuringUtil;
/**
 * Implementation of {@link AssistantTabObscuringUtil} for Chrome.
 */
public class AssistantTabObscuringUtilChrome implements AssistantTabObscuringUtil {
    private final TabObscuringHandler mTabObscuringHandler;

    /** A token held while the Autofill Assistant is obscuring all tabs. */
    private TabObscuringHandler.Token mObscuringToken;

    public AssistantTabObscuringUtilChrome(TabObscuringHandler tabObscuringHandler) {
        mTabObscuringHandler = tabObscuringHandler;
    }

    @Override
    public void obscureAllTabs() {
        if (mObscuringToken == null) {
            mObscuringToken =
                    mTabObscuringHandler.obscure(TabObscuringHandler.Target.ALL_TABS_AND_TOOLBAR);
        }
    }

    @Override
    public void unobscureAllTabs() {
        if (mObscuringToken != null) {
            mTabObscuringHandler.unobscure(mObscuringToken);
            mObscuringToken = null;
        }
    }
}
