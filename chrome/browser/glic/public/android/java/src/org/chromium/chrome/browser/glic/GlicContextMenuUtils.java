// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetUtils;

/** Enabling checks and parameters for Ask Gemini context menu entry points. */
@NullMarked
public final class GlicContextMenuUtils {

    private static final String PARAM_SHOW_ASK_GEMINI_ON_LINK = "show_on_link";
    private static final String PARAM_SHOW_ASK_GEMINI_ON_PAGE = "show_on_page";
    private static final String PARAM_SHOW_ASK_GEMINI_ON_IMAGE_MOBILE = "show_on_image_mobile";
    private static final String PARAM_SHOW_ASK_GEMINI_ON_SELECTION = "show_on_selection";

    @VisibleForTesting
    public static final String PARAM_ASK_GEMINI_SELECTION_MENU_POSITION = "selection_menu_position";

    @VisibleForTesting
    public static final String PARAM_ASK_GEMINI_SEND_SELECTED_TEXT = "send_selected_text";

    @VisibleForTesting
    public static final String PARAM_MAX_SELECTED_TEXT_LENGTH = "max_selected_text_length";

    @VisibleForTesting
    public static final int DEFAULT_MAX_SELECTED_TEXT_LENGTH = 1000;

    public static final String ASK_GEMINI_POSITION_SECONDARY = "secondary";

    public static final String ASK_GEMINI_POSITION_ASSIST = "assist";

    @VisibleForTesting
    public static final String PARAM_SUPPRESS_DUPLICATE_PROCESS_TEXT =
            "suppress_duplicate_process_text";

    @VisibleForTesting
    public static final String PARAM_SUPPRESSED_PROCESS_TEXT_ACTIVITY_PREFIXES =
            "suppressed_process_text_activity_prefixes";

    /**
     * Default value of {@link #PARAM_SUPPRESSED_PROCESS_TEXT_ACTIVITY_PREFIXES}. The param holds a
     * comma-separated list of activity name prefixes; this default is the single namespace prefix
     * (trailing dot included) covering the Google app activities that register an
     * ACTION_PROCESS_TEXT handler.
     */
    @VisibleForTesting
    public static final String DEFAULT_SUPPRESSED_PROCESS_TEXT_ACTIVITY_PREFIXES =
            "com.google.android.apps.search.assistant.surfaces.voice.robin.";

    private GlicContextMenuUtils() {}

    /** Returns true if the profile is eligible for any Glic context menu entry point. */
    public static boolean isGlicContextMenuEligible(@Nullable Profile profile) {
        if (profile == null || profile.isOffTheRecord() || DeviceInfo.isAutomotive()) {
            return false;
        }
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU)
                && GlicEnabling.isReadyForProfile(profile);
    }

    private static boolean isContextMenuEligible(
            @Nullable Profile profile, String paramName, boolean defaultValue) {
        return isGlicContextMenuEligible(profile)
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU, paramName, defaultValue);
    }

    private static boolean isContainerAvailable(Context context) {
        return GlicUtils.isSidePanelFormFactor(context)
                || TabBottomSheetUtils.isTabBottomSheetEnabled();
    }

    /** Whether the "Ask Gemini" item should be shown for text selection. */
    public static boolean shouldShowAskGeminiForSelection(
            @Nullable Profile profile, Context context) {
        return isContextMenuEligible(profile, PARAM_SHOW_ASK_GEMINI_ON_SELECTION, true)
                && isContainerAvailable(context);
    }

    /**
     * Whether the "Ask Gemini" item should be shown for a link context menu. Enabled on the side
     * panel form factor, and on mobile if the bottom sheet is enabled.
     */
    public static boolean shouldShowAskGeminiForLink(@Nullable Profile profile, Context context) {
        return isContextMenuEligible(profile, PARAM_SHOW_ASK_GEMINI_ON_LINK, true)
                && isContainerAvailable(context);
    }

    /**
     * Whether the "Ask Gemini" item should be shown for an empty-space (page) context menu. This
     * entry point is only available on the side panel form factor.
     */
    public static boolean shouldShowAskGeminiForPage(@Nullable Profile profile, Context context) {
        return isContextMenuEligible(profile, PARAM_SHOW_ASK_GEMINI_ON_PAGE, false)
                && GlicUtils.isSidePanelFormFactor(context);
    }

    /**
     * Whether the "Ask Gemini" item should be shown for an image context menu on mobile, where Glic
     * is presented in the bottom sheet. Requires the native GlicShareImage feature because the
     * native share-image handler is only constructed when that feature is enabled.
     */
    public static boolean shouldShowAskGeminiForImage(@Nullable Profile profile) {
        return isContextMenuEligible(profile, PARAM_SHOW_ASK_GEMINI_ON_IMAGE_MOBILE, false)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.GLIC_SHARE_IMAGE)
                && TabBottomSheetUtils.isTabBottomSheetEnabled();
    }

    /** Returns the configured menu position for the "Ask Gemini" text selection item. */
    public static String getAskGeminiSelectionMenuPosition() {
        return ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU,
                PARAM_ASK_GEMINI_SELECTION_MENU_POSITION);
    }

    /** Whether the selected text should be sent as a prompt when invoking Ask Gemini. */
    public static boolean shouldSendSelectedText() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU,
                PARAM_ASK_GEMINI_SEND_SELECTED_TEXT,
                true);
    }

    /**
     * Returns {@code text} as it should be sent in an Ask Gemini prompt, or an empty string if
     * nothing remains.
     */
    public static String sanitizeSelectedText(@Nullable String text) {
        int maxLength =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU,
                        PARAM_MAX_SELECTED_TEXT_LENGTH,
                        DEFAULT_MAX_SELECTED_TEXT_LENGTH);
        return SelectedTextSanitizer.sanitize(text, maxLength);
    }

    /** Whether duplicate ACTION_PROCESS_TEXT activities should be suppressed. */
    public static boolean shouldSuppressDuplicateProcessText() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU,
                PARAM_SUPPRESS_DUPLICATE_PROCESS_TEXT,
                true);
    }

    /**
     * Returns the activity name prefixes that should be suppressed in process text items. Each
     * prefix is trimmed so callers can match directly without extra whitespace trimming.
     */
    public static String[] getSuppressedProcessTextActivityPrefixes() {
        String prefixesParam =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.CLANK_GLIC_CONTEXT_MENU,
                        PARAM_SUPPRESSED_PROCESS_TEXT_ACTIVITY_PREFIXES);
        if (TextUtils.isEmpty(prefixesParam)) {
            prefixesParam = DEFAULT_SUPPRESSED_PROCESS_TEXT_ACTIVITY_PREFIXES;
        }
        String[] split = prefixesParam.split(",");
        String[] trimmed = new String[split.length];
        for (int i = 0; i < split.length; i++) {
            trimmed[i] = split[i].trim();
        }
        return trimmed;
    }
}
