// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionViewDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** The base set of properties for most omnibox suggestions. */
public class BaseSuggestionViewProperties {
    /** Describes density of the suggestions. */
    @IntDef({Density.COMFORTABLE, Density.SEMICOMPACT, Density.COMPACT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Density {
        int COMFORTABLE = 0;
        int SEMICOMPACT = 1;
        int COMPACT = 2;
    }

    /**
     * Describes the content and behavior of the interactive Action Icon.
     */
    public static final class Action {
        public final SuggestionDrawableState icon;
        public final Runnable callback;
        public final String accessibilityDescription;

        /**
         * Create a new action for suggestion.
         *
         * @param icon SuggestionDrawableState describing the icon to show.
         * @param description Content description for the action view.
         * @param callback Callback to invoke when user interacts with the icon.
         */
        public Action(SuggestionDrawableState icon, String description, Runnable callback) {
            this.icon = icon;
            this.accessibilityDescription = description;
            this.callback = callback;
        }

        /**
         * Create a new action for suggestion, using Accessibility description from a resource.
         *
         * @param context Current context
         * @param icon SuggestionDrawableState describing the icon to show.
         * @param descriptionRes Resource to use as a content description for the action view.
         * @param callback Callback to invoke when user interacts with the icon.
         */
        public Action(Context context, SuggestionDrawableState icon, @StringRes int descriptionRes,
                Runnable callback) {
            this(icon, context.getResources().getString(descriptionRes), callback);
        }
    }

    /** SuggestionDrawableState to show as a suggestion icon. */
    public static final WritableObjectPropertyKey<SuggestionDrawableState> ICON =
            new WritableObjectPropertyKey<>();

    /** Action Icons description. */
    public static final WritableObjectPropertyKey<List<Action>> ACTIONS =
            new WritableObjectPropertyKey();

    /** Delegate receiving user events. */
    public static final WritableObjectPropertyKey<SuggestionViewDelegate> SUGGESTION_DELEGATE =
            new WritableObjectPropertyKey<>();

    /** Specifies how densely suggestions should be packed. */
    public static final WritableIntPropertyKey DENSITY = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_UNIQUE_KEYS =
            new PropertyKey[] {ACTIONS, ICON, DENSITY, SUGGESTION_DELEGATE};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, SuggestionCommonProperties.ALL_KEYS);
}
