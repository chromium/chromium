// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.PreferredDisplaySurface;
import org.chromium.blink.mojom.WindowAudioPreference;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Manager for the media capture picker. This class is the entry point for showing the picker UI. It
 * will decide whether to show the old dialog or a new UI based on a feature flag.
 */
@NullMarked
public class MediaCapturePickerManager {
    private static final String TAG = "MediaCapture";
    private static final String RESULT_HISTOGRAM = "Media.MediaCapture.UI.Android.Picker.Result";
    private static final String PRE_SHOW_FAILURE_HISTOGRAM =
            "Media.MediaCapture.UI.Android.Picker.PreShowFailure";

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange
    @IntDef({Result.CANCELLED, Result.TAB_SELECTED, Result.WINDOW_SELECTED, Result.SCREEN_SELECTED})
    public @interface Result {
        int CANCELLED = 0;
        int TAB_SELECTED = 1;
        int WINDOW_SELECTED = 2;
        int SCREEN_SELECTED = 3;
        int NUM_ENTRIES = 4;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/media/enums.xml:MediaCapturePickerResultEnum)

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange
    @IntDef({
        PreShowFailure.CONTEXT_NULL_ERROR,
        PreShowFailure.PICKER_DELEGATE_NULL_ERROR,
        PreShowFailure.APP_CONTENT_SHARING_DISABLED_ERROR,
        PreShowFailure.MEDIA_PROJECTION_MANAGER_NULL_ERROR
    })
    public @interface PreShowFailure {
        int CONTEXT_NULL_ERROR = 0;
        int PICKER_DELEGATE_NULL_ERROR = 1;
        int APP_CONTENT_SHARING_DISABLED_ERROR = 2;
        int MEDIA_PROJECTION_MANAGER_NULL_ERROR = 3;
        int NUM_ENTRIES = 4;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/media/enums.xml:MediaCapturePreShowFailureEnum)

    /** A delegate for handling returning the picker result. */
    public interface Delegate extends MediaCapturePickerTabObserver.FilterDelegate {
        /**
         * Called when the user has selected a tab to share.
         *
         * @param webContents The contents to share.
         * @param audioShare True if tab audio should be shared.
         */
        void onPickTab(WebContents webContents, boolean audioShare);

        /** Called when the user has selected a window to share. */
        void onPickWindow();

        /** Called when the user has selected a screen to share. */
        void onPickScreen();

        /** Called when the user has elected to not share anything. */
        void onCancel();
    }

    /** The parameters for showing the media capture picker. */
    public static class Params {
        /**
         * The {@link WebContents} to show the dialog on behalf of. From DesktopMediaPicker::Params.
         */
        public final WebContents webContents;

        /** Name of the app that wants to share content. From DesktopMediaPicker::Params. */
        public final String appName;

        /**
         * Same as `appName` except when called via extension APIs. From DesktopMediaPicker::Params.
         */
        public final String targetName;

        /** True if audio sharing is also requested. From DesktopMediaPicker::Params. */
        public final boolean requestAudio;

        /** True if we should exclude system audio. From DesktopMediaPicker::Params. */
        public final boolean excludeSystemAudio;

        /** Type of audio to request for window sharing. From DesktopMediaPicker::Params. */
        public final @WindowAudioPreference.EnumType int windowAudioPreference;

        /** The preferred display surface. From DesktopMediaPicker::Params. */
        public final @PreferredDisplaySurface.EnumType int preferredDisplaySurface;

        /** True if we are just capturing this tab. Derived from MediaStreamRequest. */
        public final boolean captureThisTab;

        /** True if the current tab should be excluded. From MediaStreamRequest. */
        public final boolean excludeSelfBrowserSurface;

        /** True if screen sharing should be excluded. From MediaStreamRequest. */
        public final boolean excludeMonitorTypeSurfaces;

        /** The allowed capture level. From DesktopMediaPicker::Params. */
        public final @AllowedScreenCaptureLevel int allowedCaptureLevel;

        /**
         * @param webContents The {@link WebContents} to show the dialog on behalf of.
         * @param appName Name of the app that wants to share content.
         * @param targetName Same as `appName` except when called via extension APIs.
         * @param requestAudio True if audio sharing is also requested.
         * @param excludeSystemAudio True if we should exclude system audio.
         * @param windowAudioPreference Type of audio to request for window sharing.
         * @param preferredDisplaySurface The preferred display surface.
         * @param captureThisTab True if we are just capturing this tab.
         * @param excludeSelfBrowserSurface True if the current tab should be excluded.
         * @param excludeMonitorTypeSurfaces True if screen sharing should be excluded.
         * @param allowedCaptureLevel The allowed capture level.
         */
        public Params(
                WebContents webContents,
                String appName,
                String targetName,
                boolean requestAudio,
                boolean excludeSystemAudio,
                @WindowAudioPreference.EnumType int windowAudioPreference,
                @PreferredDisplaySurface.EnumType int preferredDisplaySurface,
                boolean captureThisTab,
                boolean excludeSelfBrowserSurface,
                boolean excludeMonitorTypeSurfaces,
                @AllowedScreenCaptureLevel int allowedCaptureLevel) {
            this.webContents = webContents;
            this.appName = appName;
            this.targetName = targetName;
            this.requestAudio = requestAudio;
            this.excludeSystemAudio = excludeSystemAudio;
            this.windowAudioPreference = windowAudioPreference;
            this.preferredDisplaySurface = preferredDisplaySurface;
            this.captureThisTab = captureThisTab;
            this.excludeSelfBrowserSurface = excludeSelfBrowserSurface;
            this.excludeMonitorTypeSurfaces = excludeMonitorTypeSurfaces;
            this.allowedCaptureLevel = allowedCaptureLevel;
        }
    }

    private static @Nullable Context maybeGetContext(WebContents webContents) {
        final WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return null;
        return window.getContext().get();
    }

    /**
     * Shows the media capture picker dialog.
     *
     * @param params The parameters for showing the media capture picker.
     * @param delegate Invoked with a WebContents if a tab is selected, or {@code null} if the
     *     dialog is dismissed.
     */
    public static void showDialog(Params params, Delegate delegate) {
        final Context context = maybeGetContext(params.webContents);
        if (context == null) {
            Log.e(
                    TAG,
                    "Cannot get Context from params web contents to show picker dialog, cancel "
                            + "media capture request");
            recordPreShowFailure(PreShowFailure.CONTEXT_NULL_ERROR);
            delegate.onCancel();
            return;
        }

        if (ChromeFeatureList.sAndroidNewMediaPicker.isEnabled()) {
            Log.d(TAG, "New media picker is enabled, showing MediaCapturePickerInvoker");
            MediaCapturePickerInvoker.show(context, params, delegate);
        } else {
            Log.d(TAG, "New media picker is disabled, showing MediaCapturePickerDialog");
            new MediaCapturePickerDialog(context, params, delegate).show();
        }
    }

    static void recordResult(@Result int result) {
        RecordHistogram.recordEnumeratedHistogram(RESULT_HISTOGRAM, result, Result.NUM_ENTRIES);
    }

    public static void recordPreShowFailure(@PreShowFailure int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                PRE_SHOW_FAILURE_HISTOGRAM, reason, PreShowFailure.NUM_ENTRIES);
    }
}
