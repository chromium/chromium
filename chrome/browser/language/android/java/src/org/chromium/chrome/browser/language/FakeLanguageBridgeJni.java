// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashSet;

/**
 * Fake implementation of LanguageBridge native methods used for testing.
 */
public class FakeLanguageBridgeJni implements LanguageBridge.Natives {
    private ArrayList<String> mULPLanguages;

    public FakeLanguageBridgeJni() {
        mULPLanguages = new ArrayList<String>();
    }

    public void setULPLanguages(String[] languageCodes) {
        mULPLanguages = new ArrayList<>(Arrays.asList(languageCodes));
    }

    @Override
    public void getULPModelLanguages(LinkedHashSet<String> set) {
        set.addAll(mULPLanguages);
    }
}
