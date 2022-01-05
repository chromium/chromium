// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.language.LanguageProfileController;
import org.chromium.components.language.LanguageProfileDelegateImpl;

import java.util.List;

/**
 * Bridge class for native code to access ULP data for a profile.
 * @param accountName Account name for current profile for querying ULP.
 */
public class LanguageBridge {
    @CalledByNative
    public static String[] getULPLanguages(String accountName) {
        LanguageProfileDelegateImpl delegate = new LanguageProfileDelegateImpl();
        LanguageProfileController controller = new LanguageProfileController(delegate);

        if (TextUtils.isEmpty(accountName)) accountName = null;
        List<String> languages_list = controller.getLanguagePreferences(accountName);
        String[] languages_array = new String[languages_list.size()];
        languages_array = languages_list.toArray(languages_array);
        return languages_array;
    }
}
