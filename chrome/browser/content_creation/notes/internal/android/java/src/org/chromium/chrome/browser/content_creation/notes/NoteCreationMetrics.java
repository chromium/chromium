// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import android.content.ComponentName;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.share.share_sheet.ChromeProvidedSharingOptionsProvider;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Responsible for recording metrics related to note creation.
 */
public final class NoteCreationMetrics {
    // Constants used to log the Note Creation Funnel enum histogram.
    @IntDef({NoteCreationFunnel.NOTE_CREATION_SELECTED, NoteCreationFunnel.TEMPLATE_SELECTED,
            NoteCreationFunnel.NOTE_SHARED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface NoteCreationFunnel {
        int NOTE_CREATION_SELECTED = 0;
        int TEMPLATE_SELECTED = 1;
        int NOTE_SHARED = 2;
        int NUM_ENTRIES = 3;
    }

    // Constants used to log the selected template ID.
    // This is a mirror of the NoteTemplateIds enum found in
    // components/content_creation/notes/core/templates/template_types.h
    private @interface NoteTemplateIds {
        int UNKNOWN = 0;
        int CLASSIC = 1;
        int FRIENDLY = 2;
        int FRESH = 3;
        int POWERFUL = 4;
        int IMPACTFUL = 5;
        int LOVELY = 6;
        int GROOVY = 7;
        int MONOCHROME = 8;
        int BOLD = 9;
        int DREAMY = 10;
        int NUM_ENTRIES = 11;
    }

    // Constants used to log the share destination for the created note.
    // Constants used to log the Note Creation Funnel enum histogram.
    @IntDef({NoteShareDestination.FIRST_PARTY, NoteShareDestination.THIRD_PARTY})
    private @interface NoteShareDestination {
        int FIRST_PARTY = 0;
        int THIRD_PARTY = 1;
        int NUM_ENTRIES = 2;
    }

    // Max expected number of dynamically loaded templates.

    /**
     * Records metrics related to the user starting the creation flow.
     */
    public static void recordNoteCreationSelected() {
        recordNoteCreationFunnel(NoteCreationFunnel.NOTE_CREATION_SELECTED);
    }

    /**
     * Records metrics related to the user choosing a template and creating their note.
     *
     * @param duration The time elapsed between the start of the creation flow and when the user
     *         selected a template and created their note.
     */
    public static void recordNoteTemplateSelected(
            long duration, int nbChanges, int selectedTemplateId, int selectedTemplateIndex) {
        RecordHistogram.recordMediumTimesHistogram("NoteCreation.TimeTo.SelectTemplate", duration);

        recordNoteCreationFunnel(NoteCreationFunnel.TEMPLATE_SELECTED);
        recordNoteCreated(/*created=*/true);
        recordNbTemplateChanges(nbChanges);
        recordSelectedTemplateId(selectedTemplateId);
    }

    /**
     * Records metrics related to the user dismissing the creation dialog.
     *
     * @param duration The time elapsed between the start of the creation flow and when the user
     *         dismissed the creation dialog.
     */
    public static void recordNoteCreationDismissed(long duration, int nbChanges) {
        RecordHistogram.recordMediumTimesHistogram(
                "NoteCreation.TimeTo.DismissCreationDialog", duration);

        recordNoteCreated(/*created=*/false);
        recordNbTemplateChanges(nbChanges);
    }

    /**
     * Records metrics related to the user sharing their created note.
     *
     * @param duration The time elapsed between the start of the creation flow and when the user
     *         shared their created note.
     * @param chosenComponent The component that was picked as a share desitination.
     */
    public static void recordNoteShared(long duration, ComponentName chosenComponent) {
        RecordHistogram.recordMediumTimesHistogram("NoteCreation.TimeTo.ShareCreation", duration);

        recordNoteShared(/*shared=*/true);
        recordNoteCreationFunnel(NoteCreationFunnel.NOTE_SHARED);

        RecordHistogram.recordEnumeratedHistogram("NoteCreation.ShareDestination",
                chosenComponent.equals(
                        ChromeProvidedSharingOptionsProvider.CHROME_PROVIDED_FEATURE_COMPONENT_NAME)
                        ? NoteShareDestination.FIRST_PARTY
                        : NoteShareDestination.THIRD_PARTY,
                NoteShareDestination.NUM_ENTRIES);
    }

    /**
     * Records metrics related to the user not sharing their created note.
     *
     * @param duration The time elapsed between the start of the creation flow and when the user
     *         dismissed the share sheet.
     */
    public static void recordNoteNotShared(long duration) {
        RecordHistogram.recordMediumTimesHistogram("NoteCreation.TimeTo.DismissShare", duration);

        recordNoteShared(/*shared=*/false);
    }

    /**
     * Records whether the user ended up creating a note or note after getting to the note creation
     * flow.
     *
     * @param created Whether a note was created or not.
     */
    private static void recordNoteCreated(boolean created) {
        RecordHistogram.recordBooleanHistogram("NoteCreation.CreationStatus", created);
    }

    /**
     * Records whether the user ended up sharing their created note.
     *
     * @param shared Whether the user shared the created note or not.
     */
    private static void recordNoteShared(boolean shared) {
        RecordHistogram.recordBooleanHistogram("NoteCreation.NoteShared", shared);
    }

    /**
     * Records the different states of the creation funnel that the user reaches.
     *
     * @param funnelState The state of the funnel that the user reached.
     */
    private static void recordNoteCreationFunnel(@NoteCreationFunnel int funnelState) {
        assert funnelState < NoteCreationFunnel.NUM_ENTRIES;
        assert funnelState >= 0;

        RecordHistogram.recordEnumeratedHistogram(
                "NoteCreation.Funnel", funnelState, NoteCreationFunnel.NUM_ENTRIES);
    }

    /**
     * Records the number of times the user switched between templates.
     *
     * @param nbChanges The number of times the user changes templates.
     */
    private static void recordNbTemplateChanges(int nbChanges) {
        RecordHistogram.recordCount100Histogram("NoteCreation.NumberOfTemplateChanges", nbChanges);
    }

    /**
     * Records the id of the template that was selected by the user.
     *
     * @param selectedTemplateId The id of the selected template.
     */
    private static void recordSelectedTemplateId(@NoteTemplateIds int selectedTemplateId) {
        assert selectedTemplateId < NoteTemplateIds.NUM_ENTRIES;
        assert selectedTemplateId >= 0;

        if (selectedTemplateId >= NoteTemplateIds.NUM_ENTRIES) {
            selectedTemplateId = NoteTemplateIds.UNKNOWN;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "NoteCreation.SelectedTemplate", selectedTemplateId, NoteTemplateIds.NUM_ENTRIES);
    }

    // Empty private constructor for the "static" class.
    private NoteCreationMetrics() {}
}
