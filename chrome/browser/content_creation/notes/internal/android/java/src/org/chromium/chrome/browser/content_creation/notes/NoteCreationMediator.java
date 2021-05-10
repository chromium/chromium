// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import org.chromium.components.content_creation.notes.NoteService;
import org.chromium.components.content_creation.notes.models.NoteTemplate;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Mediator for the note creation component.
 */
public class NoteCreationMediator {
    private final ModelList mListModel;
    private final NoteService mNoteService;

    /**
     * Constructor which will also kick-off the asynchronous retrieval of Note
     * templates.
     */
    public NoteCreationMediator(ModelList listModel, NoteService noteService) {
        mListModel = listModel;
        mNoteService = noteService;

        mNoteService.getTemplates(this::populateList);
    }

    private void populateList(List<NoteTemplate> templates) {
        assert mListModel.size() == 0;
        for (NoteTemplate template : templates) {
            ListItem listItem =
                    new ListItem(NoteProperties.NOTE_VIEW_TYPE, buildModelFromTemplate(template));
            mListModel.add(listItem);
        }
    }

    private PropertyModel buildModelFromTemplate(NoteTemplate template) {
        PropertyModel.Builder builder = new PropertyModel.Builder(NoteProperties.ALL_KEYS)
                                                .with(NoteProperties.TEMPLATE, template);

        return builder.build();
    }
}
