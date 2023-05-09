// Copyright 2021 The Chromium Authors
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
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.List;

/**
 * Bridge class for native code to access ULP data for a profile.
 */
public class LanguageBridge {
    /**
     * Returns the TopULPMatchType for |language| and the top ULP language. Only language bases are
     * compared (e.g. en-US = en-GB).
     * @param language String of language tag to check.
     * @return TopULPMatchType
     */
    public static @AppLanguagePromoDialog.TopULPMatchType int isTopULPBaseLanguage(
            String language) {
        LinkedHashSet<String> ulpLanguages = getULPFromPreference();

        Iterator<String> ulpIterator = ulpLanguages.iterator();
        if (!ulpIterator.hasNext()) return AppLanguagePromoDialog.TopULPMatchType.EMPTY;

        String topLanguage = ulpIterator.next();
        // Convert ULP language to Chrome UI languages
        switch (LocaleUtils.toBaseLanguage(topLanguage)) {
            case "nn": // We do not support "nn" as a UI language so consider it the same as "no"
            case "no":
                topLanguage = "nb";
                break;
            case "tl":
                topLanguage = "fil";
                break;
            default:
                // use topLanguage
        }

        return LocaleUtils.isBaseLanguageEqual(language, topLanguage)
                ? AppLanguagePromoDialog.TopULPMatchType.YES
                : AppLanguagePromoDialog.TopULPMatchType.NO;
    }

    /**
     * @return The ordered set of ULP languages as saved in the Chrome preference.
     */
    public static LinkedHashSet<String> getULPFromPreference() {
        return new LinkedHashSet<>(Arrays.asList(LanguageBridgeJni.get().getULPFromPreference()));
    }

    /**
     * Blocking call used by native ULPLanguageModel to get device ULP languages.
     */
    @CalledByNative
    public static String[] getULPLanguagesFromDevice(String accountName) {
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
        String[] getULPFromPreference();
    }
}
