// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import android.text.TextUtils;

import org.chromium.base.LocaleUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.language.LanguageProfileController;
import org.chromium.components.language.LanguageProfileDelegateImpl;

import java.util.Arrays;
import java.util.Collection;
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.List;

/**
 * Bridge class for native code to access ULP data for a profile.
 */
public class LanguageBridge {
    /**
     * Returns true if the language tag matches the top ULP language. Only
     * language bases are compared (e.g. en-US = en-GB).
     * @param language String of language tag to check.
     * @return True if the base language tag matches the top language.
     */
    public static boolean isTopULPBaseLanguage(String language) {
        LinkedHashSet<String> ulpLanguages = getULPModelLanguages();

        Iterator<String> ulpIterator = ulpLanguages.iterator();
        if (!ulpIterator.hasNext()) return false;
        return LocaleUtils.isBaseLanguageEqual(language, ulpIterator.next());
    }

    /**
     * @return The ordered set of ULP languages saved in the ULP Language Model
     */
    public static LinkedHashSet<String> getULPModelLanguages() {
        LinkedHashSet<String> set = new LinkedHashSet<>();
        LanguageBridgeJni.get().getULPModelLanguages(set);
        return set;
    }

    @CalledByNative
    private static void copyStringArrayToCollection(Collection<String> c, String[] a) {
        c.addAll(Arrays.asList(a));
    }

    /**
     * Blocking call used by native ULPLanguageModel to get device ULP languages.
     */
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

    @NativeMethods
    interface Natives {
        void getULPModelLanguages(LinkedHashSet<String> set);
    }
}
