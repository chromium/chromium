// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** A helper class for using the DOM Distiller. */
@JNINamespace("android")
public class DomDistillerTabUtils {
    /** Triggering heuristics encoded in native enum DistillerHeuristicsType. */
    private static Integer sHeuristics;

    /** Used to specify whether mobile friendly is enabled for testing purposes. */
    private static Boolean sExcludeMobileFriendlyForTesting;

    @DistillerHeuristicsType private static Integer sHeuristicsForTesting;

    private DomDistillerTabUtils() {}

    /**
     * Creates a new WebContents and navigates the {@link WebContents} to view the URL of the
     * current page, while in the background starts distilling the current page. This method takes
     * ownership over the old WebContents after swapping in the new one.
     *
     * @param webContents the WebContents to distill.
     */
    public static void distillCurrentPageAndView(WebContents webContents) {
        DomDistillerTabUtilsJni.get().distillCurrentPageAndView(webContents);
    }

    /**
     * Starts distillation in the source {@link WebContents}. The viewer needs to be handled
     * elsewhere.
     *
     * @param webContents the WebContents to distill.
     */
    public static void distillCurrentPage(WebContents webContents) {
        DomDistillerTabUtilsJni.get().distillCurrentPage(webContents);
    }

    /**
     * Starts distillation in the source {@link WebContents} while navigating the destination
     * {@link WebContents} to view the distilled content. This does not take ownership of any
     * of the WebContents.
     *
     * @param sourceWebContents the WebContents to distill.
     * @param destinationWebContents the WebContents to display the distilled content in.
     */
    public static void distillAndView(
            WebContents sourceWebContents, WebContents destinationWebContents) {
        DomDistillerTabUtilsJni.get().distillAndView(sourceWebContents, destinationWebContents);
    }

    /**
     * Returns the formatted version of the original URL of a distillation, given the original URL.
     *
     * @param url The original URL.
     * @return the formatted URL of the original page.
     */
    public static String getFormattedUrlFromOriginalDistillerUrl(GURL url) {
        return DomDistillerTabUtilsJni.get().getFormattedUrlFromOriginalDistillerUrl(url);
    }

    /**
     * Detect if any heuristic is being used to determine if a page is distillable.
     * This is testing if the heuristic is not "NONE".
     *
     * @return True if heuristics are being used to detect distillable pages.
     */
    public static boolean isDistillerHeuristicsEnabled() {
        return getDistillerHeuristics() != DistillerHeuristicsType.NONE;
    }

    /**
     * Check if the distiller is reporting every page as distillable.
     *
     * @return True if heuristic is ALWAYS_TRUE.
     */
    public static boolean isHeuristicAlwaysTrue() {
        return getDistillerHeuristics() == DistillerHeuristicsType.ALWAYS_TRUE;
    }

    /** Returns whether the reader mode accessibility setting is enabled. */
    public static boolean isReaderModeAccessibilitySettingEnabled(Profile profile) {
        return UserPrefs.get(profile).getBoolean(Pref.READER_FOR_ACCESSIBILITY);
    }

    /**
     * Check if the distiller should report mobile-friendly pages as non-distillable.
     *
     * @return True if heuristic is ADABOOST_MODEL, and "Simplified view for accessibility" is
     *     disabled. Or false under certain experimental conditions.
     */
    public static boolean shouldExcludeMobileFriendly(Tab tab) {
        if (sExcludeMobileFriendlyForTesting != null) return sExcludeMobileFriendlyForTesting;
        // Including mobile-friendly by default only applies to the CPA, otherwise we fallback to
        // the accessibility setting.
        if (DomDistillerFeatures.triggerOnMobileFriendlyPages()
                && !ReaderModeManager.shouldUseReaderModeMessages(tab)) {
            return false;
        }

        return !isReaderModeAccessibilitySettingEnabled(tab.getProfile())
                && getDistillerHeuristics() == DistillerHeuristicsType.ADABOOST_MODEL;
    }

    public static void setExcludeMobileFriendlyForTesting(Boolean excludeForTesting) {
        sExcludeMobileFriendlyForTesting = excludeForTesting;
        ResettersForTesting.register(() -> sExcludeMobileFriendlyForTesting = null);
    }

    /** Set a test value of DistillerHeuristicsType. */
    public static void setDistillerHeuristicsForTesting(
            @DistillerHeuristicsType Integer distillerHeuristicsType) {
        sHeuristicsForTesting = distillerHeuristicsType;
        ResettersForTesting.register(() -> sHeuristicsForTesting = null);
    }

    /** Cached version of DomDistillerTabUtilsJni.get().getDistillerHeuristics(). */
    public static @DistillerHeuristicsType int getDistillerHeuristics() {
        if (sHeuristicsForTesting != null) {
            return sHeuristicsForTesting;
        }
        if (sHeuristics == null) {
            sHeuristics = DomDistillerTabUtilsJni.get().getDistillerHeuristics();
        }
        return sHeuristics;
    }

    /**
     * Set an InterceptNavigationDelegate on a WebContents.
     * @param delegate The navigation delegate.
     * @param webContents The WebContents to bind the delegate to.
     */
    public static void setInterceptNavigationDelegate(
            InterceptNavigationDelegate delegate, WebContents webContents) {
        DomDistillerTabUtilsJni.get().setInterceptNavigationDelegate(delegate, webContents);
    }

    @NativeMethods
    interface Natives {
        void distillCurrentPageAndView(WebContents webContents);

        void distillCurrentPage(WebContents webContents);

        void distillAndView(WebContents sourceWebContents, WebContents destinationWebContents);

        @JniType("std::u16string")
        String getFormattedUrlFromOriginalDistillerUrl(GURL url);

        int getDistillerHeuristics();

        void setInterceptNavigationDelegate(
                InterceptNavigationDelegate delegate, WebContents webContents);
    }
}
