// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.LocaleUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.language.LanguageProfileController;

import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.List;

/** Bridge class for native code to access ULP data for a profile. */
public class LanguageBridge {
    /**
     * Returns the TopULPMatchType for |language| and the top ULP language. Only language bases are
     * compared (e.g. en-US = en-GB).
     *
     * @param language String of language tag to check.
     * @return TopULPMatchType
     */
    public static @AppLanguagePromoDialog.TopULPMatchType int isTopULPBaseLanguage(
            Profile profile, String language) {
        LinkedHashSet<String> ulpLanguages = getULPFromPreference(profile);

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
    public static LinkedHashSet<String> getULPFromPreference(Profile profile) {
        return new LinkedHashSet<>(LanguageBridgeJni.get().getULPFromPreference(profile));
    }

    /** Blocking call used by native ULPLanguageModel to get device ULP languages. */
    @CalledByNative
    public static @JniType("std::vector<std::string>") List<String> getULPLanguagesFromDevice(
            @JniType("std::string") String accountName) {
        if (TextUtils.isEmpty(accountName)) accountName = null;
        return LanguageProfileController.getLanguagePreferences(accountName);
    }

    @NativeMethods
    interface Natives {
        @JniType("std::vector<std::string>")
        List<String> getULPFromPreference(@JniType("Profile*") Profile profile);
    }
}
