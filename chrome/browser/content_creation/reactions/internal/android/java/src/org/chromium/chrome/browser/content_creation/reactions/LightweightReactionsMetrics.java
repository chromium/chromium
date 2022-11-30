// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions;

import android.content.ComponentName;
import android.content.res.Configuration;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.content_creation.reactions.scene.ReactionLayout;
import org.chromium.chrome.browser.share.share_sheet.ChromeProvidedSharingOptionsProvider;
import org.chromium.components.content_creation.reactions.ReactionType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Responsible for recording metrics related to Lightweight Reactions.
 */
public final class LightweightReactionsMetrics {
    // Constants used to record how far users go into the Lightweight Reactions UX.
    @IntDef({LightweightReactionsFunnel.DIALOG_OPENED, LightweightReactionsFunnel.EDITING_DONE,
            LightweightReactionsFunnel.GIF_GENERATED, LightweightReactionsFunnel.GIF_SHARED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface LightweightReactionsFunnel {
        int DIALOG_OPENED = 0;
        int EDITING_DONE = 1;
        int GIF_GENERATED = 2;
        int GIF_SHARED = 3;
        int NUM_ENTRIES = 4;
    }

    // Constants used to log the share destination type for the generated GIF.
    @IntDef({ShareDestination.FIRST_PARTY, ShareDestination.THIRD_PARTY})
    private @interface ShareDestination {
        int FIRST_PARTY = 0;
        int THIRD_PARTY = 1;
        int NUM_ENTRIES = 2;
    }

    // Constants used to log the device orientation changes.
    @IntDef({DeviceOrientation.LANDSCAPE, DeviceOrientation.PORTRAIT})
    private @interface DeviceOrientation {
        int LANDSCAPE = 0;
        int PORTRAIT = 1;
        int NUM_ENTRIES = 2;
    }

    // The min and max values for the duration histograms, in ms. 10 ms is the minimum supported
    // value.
    private static final long DURATION_HISTOGRAM_MIN_TIME = 10;
    private static final long DURATION_HISTOGRAM_MAX_TIME = TimeUnit.MINUTES.toMillis(10);

    // 40 buckets for a histogram of 10 minutes, which divides durations into 15-second buckets.
    private static final int DURATION_HISTOGRAM_BUCKETS = 40;

    /**
     * Records metrics related to the user starting the Lightweight Reactions flow.
     */
    public static void recordDialogOpened() {
        recordFunnel(LightweightReactionsFunnel.DIALOG_OPENED);
    }

    /**
     * Records metrics related to fetching assets.
     *
     * @param success Whether the assets were all successfully downloaded.
     * @param fetchDuration How long it took to download all assets.
     */
    public static void recordAssetsFetched(boolean success, long fetchDuration) {
        RecordHistogram.recordBooleanHistogram("LightweightReactions.AssetsFetchSuccess", success);
        RecordHistogram.recordMediumTimesHistogram(
                "LightweightReactions.AssetsFetchDuration." + (success ? "Success" : "Failure"),
                fetchDuration);
    }

    /**
     * Records metrics related to the user editing their GIF and tapping the Done button.
     *
     * @param duration The time elapsed between the dialog being opened and the Done button tapped.
     */
    public static void recordEditingDone(long duration) {
        RecordHistogram.recordCustomTimesHistogram("LightweightReactions.TimeTo.FinishEditing",
                duration, DURATION_HISTOGRAM_MIN_TIME, DURATION_HISTOGRAM_MAX_TIME,
                DURATION_HISTOGRAM_BUCKETS);
        recordFunnel(LightweightReactionsFunnel.EDITING_DONE);
        recordEditingDone(/*tappedDone=*/true);
    }

    /**
     * Records metrics related to the user dismissing the GIF editing dialog.
     *
     * @param duration The time elapsed between the dialog being opened and the Close button tapped.
     */
    public static void recordDialogDismissed(long duration) {
        RecordHistogram.recordCustomTimesHistogram("LightweightReactions.TimeTo.DismissDialog",
                duration, DURATION_HISTOGRAM_MIN_TIME, DURATION_HISTOGRAM_MAX_TIME,
                DURATION_HISTOGRAM_BUCKETS);
        recordEditingDone(/*tappedDone=*/false);
    }

    /**
     * Records metrics related to the user cancelling GIF generation.
     *
     * @param generationDuration The duration between generation start and cancellation.
     * @param progress The generation progress, in %, at time of cancellation.
     */
    public static void recordGifGenerationCancelled(long generationDuration, int progress) {
        RecordHistogram.recordBooleanHistogram("LightweightReactions.GifGenerationCancelled", true);
        RecordHistogram.recordMediumTimesHistogram(
                "LightweightReactions.GifGenerationCancelled.Duration", generationDuration);
        RecordHistogram.recordCount100Histogram(
                "LightweightReactions.GifGenerationCancelled.Progress", progress);
    }

    /**
     * Records metrics related to GIF generation.
     *
     * @param duration The time elapsed between the dialog being opened and the GIF being fully
     *         generated.
     * @param success Whether the GIF was successfully generated.
     * @param generationDuration How long it took to generate / encode the GIF.
     */
    public static void recordGifGenerated(long duration, boolean success, long generationDuration) {
        RecordHistogram.recordCustomTimesHistogram("LightweightReactions.TimeTo.GenerateGif",
                duration, DURATION_HISTOGRAM_MIN_TIME, DURATION_HISTOGRAM_MAX_TIME,
                DURATION_HISTOGRAM_BUCKETS);
        recordFunnel(LightweightReactionsFunnel.GIF_GENERATED);
        RecordHistogram.recordBooleanHistogram(
                "LightweightReactions.GifGenerationCancelled", false);
        RecordHistogram.recordBooleanHistogram(
                "LightweightReactions.GifGenerationSuccess", success);
        RecordHistogram.recordMediumTimesHistogram(
                "LightweightReactions.GifGenerationDuration", generationDuration);
    }

    /**
     * Records metrics related to the user sharing their generated GIF.
     *
     * @param duration The time elapsed between the dialog being opened and when the user shared
     *                 their generated GIF.
     * @param chosenComponent The component that was picked as a share destination.
     */
    public static void recordGifShared(long duration, ComponentName chosenComponent) {
        RecordHistogram.recordCustomTimesHistogram("LightweightReactions.TimeTo.ShareGif", duration,
                DURATION_HISTOGRAM_MIN_TIME, DURATION_HISTOGRAM_MAX_TIME,
                DURATION_HISTOGRAM_BUCKETS);
        recordGifShared(/*shared=*/true);
        recordFunnel(LightweightReactionsFunnel.GIF_SHARED);
        RecordHistogram.recordEnumeratedHistogram("LightweightReactions.ShareDestination",
                chosenComponent.equals(
                        ChromeProvidedSharingOptionsProvider.CHROME_PROVIDED_FEATURE_COMPONENT_NAME)
                        ? ShareDestination.FIRST_PARTY
                        : ShareDestination.THIRD_PARTY,
                ShareDestination.NUM_ENTRIES);
    }

    /**
     * Records metrics related to the user not sharing their generated GIF.
     *
     * @param duration The time elapsed between the dialog being opened and when the user dismissed
     *                 the Share Sheet.
     */
    public static void recordGifNotShared(long duration) {
        RecordHistogram.recordCustomTimesHistogram("LightweightReactions.TimeTo.DismissShare",
                duration, DURATION_HISTOGRAM_MIN_TIME, DURATION_HISTOGRAM_MAX_TIME,
                DURATION_HISTOGRAM_BUCKETS);
        recordGifShared(/*shared=*/false);
    }

    /**
     * Records metrics related to editing the GIF.
     *
     * @param tappedDone Whether the user tapped the Done button after editing their GIF.
     * @param nbReactionsAdded The total number of reactions that were added to the scene via the
     *         toolbar.
     * @param nbTypeChange The total number of times (across all reactions) the user changed a
     *         reaction's type.
     * @param nbRotateScale The total number of times (across all reactions) the user interacted
     *         with the scale / rotate control.
     * @param nbDuplicate The total number of times (across all reactions) the user tapped the
     *         duplicate control.
     * @param nbDelete The total number of times (across all reactions) the user tapped the delete
     *         control.
     * @param nbMove The total number of times (across all reactions) the user moved a reaction.
     */
    public static void recordEditingMetrics(boolean tappedDone, int nbReactionsAdded,
            int nbTypeChange, int nbRotateScale, int nbDuplicate, int nbDelete, int nbMove) {
        if (tappedDone) {
            RecordHistogram.recordCount100Histogram(
                    "LightweightReactions.Editing.TappedDone.NumberOfReactionsAdded",
                    nbReactionsAdded);
            RecordHistogram.recordCount100Histogram(
                    "LightweightReactions.Editing.TappedDone.NumberOfTypeChanges", nbTypeChange);
            RecordHistogram.recordCount100Histogram(
                    "LightweightReactions.Editing.TappedDone.NumberOfRotateScale", nbRotateScale);
            RecordHistogram.recordCount100Histogram(
                    "LightweightReactions.Editing.TappedDone.NumberOfDuplicate", nbDuplicate);
            RecordHistogram.recordCount100Histogram(
                    "LightweightReactions.Editing.TappedDone.NumberOfDelete", nbDelete);
            RecordHistogram.recordCount100Histogram(
                    "LightweightReactions.Editing.TappedDone.NumberOfMove", nbMove);
        } else {
            RecordHistogram.recordCount100Histogram(
                    "LightweightReactions.Editing.TappedCancel.NumberOfReactionsAdded",
                    nbReactionsAdded);
            RecordHistogram.recordCount100Histogram(
                    "LightweightReactions.Editing.TappedCancel.NumberOfTypeChanges", nbTypeChange);
            RecordHistogram.recordCount100Histogram(
                    "LightweightReactions.Editing.TappedCancel.NumberOfRotateScale", nbRotateScale);
            RecordHistogram.recordCount100Histogram(
                    "LightweightReactions.Editing.TappedCancel.NumberOfDuplicate", nbDuplicate);
            RecordHistogram.recordCount100Histogram(
                    "LightweightReactions.Editing.TappedCancel.NumberOfDelete", nbDelete);
            RecordHistogram.recordCount100Histogram(
                    "LightweightReactions.Editing.TappedCancel.NumberOfMove", nbMove);
        }
    }

    /**
     * Records that a device orientation change happened during Lightweight Reactions scene editing.
     *
     * @param newOrientation The new orientation, taken from a {@link Configuration} object.
     */
    public static void recordOrientationChange(int newOrientation) {
        RecordHistogram.recordEnumeratedHistogram("LightweightReactions.OrientationChange",
                newOrientation == Configuration.ORIENTATION_PORTRAIT ? DeviceOrientation.PORTRAIT
                                                                     : DeviceOrientation.LANDSCAPE,
                DeviceOrientation.NUM_ENTRIES);
    }

    /**
     * Records the types of the reactions that were used in the final GIF.
     *
     * @param reactions The set of reactions added to the scene.
     */
    public static void recordReactionsUsed(Set<ReactionLayout> reactions) {
        for (ReactionLayout rl : reactions) {
            RecordHistogram.recordEnumeratedHistogram("LightweightReactions.ReactionsUsed",
                    rl.getReaction().getMetadata().type, ReactionType.MAX_VALUE + 1);
        }
    }

    /**
     * Records whether the user completed their GIF editing and proceeded to GIF generation by
     * tapping the Done button.
     *
     * @param tappedDone Whether the Done button was tapped or not.
     */
    private static void recordEditingDone(boolean tappedDone) {
        RecordHistogram.recordBooleanHistogram("LightweightReactions.EditingDone", tappedDone);
    }

    /**
     * Records whether the user ended up sharing their generated GIF.
     *
     * @param shared Whether the user shared the generated GIF or not.
     */
    private static void recordGifShared(boolean shared) {
        RecordHistogram.recordBooleanHistogram("LightweightReactions.GifShared", shared);
    }

    /**
     * Records the step of the Lightweight Reaction usage funnel that the user reaches.
     *
     * @param funnelState The step of the funnel that the user reached.
     */
    private static void recordFunnel(@LightweightReactionsFunnel int funnelState) {
        assert funnelState < LightweightReactionsFunnel.NUM_ENTRIES;
        assert funnelState >= 0;

        RecordHistogram.recordEnumeratedHistogram(
                "LightweightReactions.Funnel", funnelState, LightweightReactionsFunnel.NUM_ENTRIES);
    }

    // Empty private constructor for the "static" class.
    private LightweightReactionsMetrics() {}
}
