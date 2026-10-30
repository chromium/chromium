// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import androidx.annotation.IntDef;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlicInvocationSource;
import org.chromium.chrome.browser.profiles.Profile;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Metrics utils for use in Glic. */
@JNINamespace("glic")
@NullMarked
public class GlicMetrics {
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange(GlicWebContentsContextMenuResult)
    @IntDef({WebContentsContextMenuResult.SHOWN_AND_IGNORED, WebContentsContextMenuResult.EXECUTED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface WebContentsContextMenuResult {
        int SHOWN_AND_IGNORED = 0;
        int EXECUTED = 1;
        int NUM_ENTRIES = 2;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicWebContentsContextMenuResult)

    private static String getMetricsInfix(@Nullable Profile profile) {
        if (profile == null) {
            return "Consumer";
        }
        return GlicMetricsJni.get().getMetricsInfix(profile);
    }

    @NativeMethods
    public interface Natives {
        @JniType("std::string")
        String getMetricsInfix(@JniType("Profile*") Profile profile);
    }

    private static String getProviderMetricName(@Nullable Profile profile, String suffix) {
        return "Glic." + getMetricsInfix(profile) + "." + suffix;
    }

    /** Recorded when the user clicks the 'X' to close the peek view. */
    public static void recordClosePeekView() {
        RecordUserAction.record("Glic.Instance.Close.PeekView");
    }

    /** Recorded when the Peek view appears on the screen. */
    public static void recordShowPeekView() {
        RecordUserAction.record("Glic.Instance.Show.PeekView");
    }

    /** Recorded when the Bottom Sheet appears on the screen. */
    public static void recordShowBottomSheet() {
        RecordUserAction.record("Glic.Instance.Show.BottomSheet");
    }

    /**
     * Recorded when a Glic entry point is clicked.
     *
     * @param source The GlicInvocationSource that triggered the click.
     * @param isNtp Whether the active tab is a New Tab Page.
     */
    public static void recordEntryPointClick(@GlicInvocationSource int source, boolean isNtp) {
        recordEntryPointClick(/* profile= */ null, source, isNtp);
    }

    /**
     * Recorded when a Glic entry point is clicked.
     *
     * @param profile The Profile where the click occurred, or null if unavailable.
     * @param source The GlicInvocationSource that triggered the click.
     * @param isNtp Whether the active tab is a New Tab Page.
     */
    public static void recordEntryPointClick(
            @Nullable Profile profile, @GlicInvocationSource int source, boolean isNtp) {
        String tabContext = isNtp ? "Ntp" : "Other";
        String suffix = "EntryPoint.Click." + tabContext;
        RecordHistogram.recordEnumeratedHistogram(
                "Glic." + suffix, source, GlicInvocationSource.MAX_VALUE + 1);
        RecordHistogram.recordEnumeratedHistogram(
                getProviderMetricName(profile, suffix), source, GlicInvocationSource.MAX_VALUE + 1);
    }

    /**
     * Records, when a web contents context menu closes, whether a Glic item shown in it was
     * clicked. Desktop records the same histogram.
     *
     * @param source The invocation source of the shown item: a context menu link, page, text
     *     selection or shared image source.
     * @param executed Whether the user clicked the item.
     */
    public static void recordWebContentsContextMenuResult(
            @GlicInvocationSource int source, boolean executed) {
        recordWebContentsContextMenuResult(/* profile= */ null, source, executed);
    }

    /**
     * Records, when a web contents context menu closes, whether a Glic item shown in it was
     * clicked. Desktop records the same histogram.
     *
     * @param profile The Profile where the menu was shown, or null if unavailable.
     * @param source The invocation source of the shown item: a context menu link, page, text
     *     selection or shared image source.
     * @param executed Whether the user clicked the item.
     */
    public static void recordWebContentsContextMenuResult(
            @Nullable Profile profile, @GlicInvocationSource int source, boolean executed) {
        String variant = getWebContentsContextMenuVariant(source);
        if (variant == null) {
            assert false : "Unexpected source: " + source;
            return;
        }
        String suffix = "WebContentsContextMenu." + variant;
        int result =
                executed
                        ? WebContentsContextMenuResult.EXECUTED
                        : WebContentsContextMenuResult.SHOWN_AND_IGNORED;
        RecordHistogram.recordEnumeratedHistogram(
                "Glic." + suffix, result, WebContentsContextMenuResult.NUM_ENTRIES);
        RecordHistogram.recordEnumeratedHistogram(
                getProviderMetricName(profile, suffix),
                result,
                WebContentsContextMenuResult.NUM_ENTRIES);
    }

    private static @Nullable String getWebContentsContextMenuVariant(
            @GlicInvocationSource int source) {
        switch (source) {
            case GlicInvocationSource.WEB_CONTENTS_CONTEXT_MENU_LINK:
                return "Link";
            case GlicInvocationSource.WEB_CONTENTS_CONTEXT_MENU_PAGE:
                return "Page";
            case GlicInvocationSource.WEB_CONTENTS_CONTEXT_MENU_TEXT_SELECTION:
                return "TextSelection";
            case GlicInvocationSource.SHARED_IMAGE:
                return "Image";
            default:
                return null;
        }
    }

    /**
     * Recorded when Chrome attempts to send the user's selected text to Glic, after the user taps
     * the 'Ask Gemini' item in the text selection menu. Pairs with Glic.EntryPoint.Click.{Ntp,
     * Other} to form a click to successful-send funnel.
     *
     * @param succeeded Whether the selected text was successfully handed off to the Glic service.
     */
    public static void recordSendSelectedTextSucceeded(boolean succeeded) {
        recordSendSelectedTextSucceeded(/* profile= */ null, succeeded);
    }

    /**
     * Recorded when Chrome attempts to send the user's selected text to Glic, after the user taps
     * the 'Ask Gemini' item in the text selection menu. Pairs with Glic.EntryPoint.Click.{Ntp,
     * Other} to form a click to successful-send funnel.
     *
     * @param profile The Profile where the text selection occurred, or null if unavailable.
     * @param succeeded Whether the selected text was successfully handed off to the Glic service.
     */
    public static void recordSendSelectedTextSucceeded(
            @Nullable Profile profile, boolean succeeded) {
        String suffix = "EntryPoint.SendSelectedTextSucceeded";
        RecordHistogram.recordBooleanHistogram("Glic." + suffix, succeeded);
        RecordHistogram.recordBooleanHistogram(getProviderMetricName(profile, suffix), succeeded);
    }
}
