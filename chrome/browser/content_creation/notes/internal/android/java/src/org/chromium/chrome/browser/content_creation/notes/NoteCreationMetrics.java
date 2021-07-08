// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

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

    public static void recordNoteCreationSelected() {
        RecordHistogram.recordEnumeratedHistogram("NoteCreation.Funnel",
                NoteCreationFunnel.NOTE_CREATION_SELECTED, NoteCreationFunnel.NUM_ENTRIES);
    }

    public static void recordNoteTemplateSelected() {
        RecordHistogram.recordEnumeratedHistogram("NoteCreation.Funnel",
                NoteCreationFunnel.TEMPLATE_SELECTED, NoteCreationFunnel.NUM_ENTRIES);
    }

    public static void recordNoteShared() {
        RecordHistogram.recordEnumeratedHistogram("NoteCreation.Funnel",
                NoteCreationFunnel.NOTE_SHARED, NoteCreationFunnel.NUM_ENTRIES);
        RecordHistogram.recordBooleanHistogram("NoteCreation.NoteShared", true);
    }

    public static void recordNoteNotShared() {
        RecordHistogram.recordBooleanHistogram("NoteCreation.NoteShared", false);
    }

    public static void recordNoteCreationStatus(boolean created) {
        RecordHistogram.recordBooleanHistogram("NoteCreation.CreationStatus", created);
    }

    public static void recordNbTemplateChanges(int nbChanges) {
        RecordHistogram.recordCount100Histogram("NoteCreation.NumberOfTemplateChanges", nbChanges);
    }

    public static void recordSelectedTemplateId(int selectedTemplateId) {
        assert selectedTemplateId < NoteTemplateIds.NUM_ENTRIES;

        if (selectedTemplateId >= NoteTemplateIds.NUM_ENTRIES) {
            selectedTemplateId = NoteTemplateIds.UNKNOWN;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "NoteCreation.SelectedTemplate", selectedTemplateId, NoteTemplateIds.NUM_ENTRIES);
    }

    // Empty private constructor for the "static" class.
    private NoteCreationMetrics() {}
}
