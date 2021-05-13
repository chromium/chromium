// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import android.graphics.Typeface;
import android.os.Looper;

import org.chromium.chrome.browser.content_creation.notes.fonts.GoogleFontService;
import org.chromium.chrome.browser.content_creation.notes.fonts.TypefaceRequest;
import org.chromium.chrome.browser.content_creation.notes.fonts.TypefaceResponse;
import org.chromium.components.content_creation.notes.NoteService;
import org.chromium.components.content_creation.notes.models.NoteTemplate;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Mediator for the note creation component.
 */
public class NoteCreationMediator {
    private final ModelList mListModel;
    private final NoteService mNoteService;
    private final GoogleFontService mFontService;

    /**
     * Constructor which will also kick-off the asynchronous retrieval of Note
     * templates.
     */
    public NoteCreationMediator(
            ModelList listModel, GoogleFontService fontService, NoteService noteService) {
        mListModel = listModel;
        mNoteService = noteService;
        mFontService = fontService;

        mNoteService.getTemplates(this::resolveTypefaces);
    }

    private void resolveTypefaces(List<NoteTemplate> templates) {
        // Build an association between the templates and their typeface request
        // while maintaining the templates' ordering.
        List<RequestTuple> requestTuples = new ArrayList<>();
        for (NoteTemplate template : templates) {
            requestTuples.add(new RequestTuple(template));
        }

        // Extract the set of typeface requests and resolve them.
        Set<TypefaceRequest> requests = new HashSet<>();
        for (RequestTuple tuple : requestTuples) {
            requests.add(tuple.typefaceRequest);
        }

        mFontService.fetchFonts(requests, new GoogleFontService.GoogleFontRequestCallback() {
            @Override
            public void onResponsesReceived(Map<TypefaceRequest, TypefaceResponse> resultsMap) {
                populateList(requestTuples, resultsMap);
            }
        });
    }

    private void populateList(
            List<RequestTuple> requestTuples, Map<TypefaceRequest, TypefaceResponse> resultsMap) {
        // The list should still be empty.
        assert mListModel.size() == 0;

        // Ensure that this code is running on the main thread.
        assert Looper.getMainLooper() == Looper.myLooper();

        // Only add a template to the ModelList when a Typeface has successfully been loaded for it.
        for (RequestTuple tuple : requestTuples) {
            TypefaceResponse response = getOrDefault(resultsMap, tuple.typefaceRequest, null);
            if (response == null) {
                // TODO (crbug.com/1194168): Log this case.
                continue;
            }

            if (response.isError()) {
                // TODO (crbug.com/1194168): Log this case.
                continue;
            }

            ListItem listItem = new ListItem(
                    NoteProperties.NOTE_VIEW_TYPE, buildModel(tuple.template, response.typeface));
            mListModel.add(listItem);
        }
    }

    private PropertyModel buildModel(NoteTemplate template, Typeface typeface) {
        PropertyModel.Builder builder = new PropertyModel.Builder(NoteProperties.ALL_KEYS)
                                                .with(NoteProperties.TEMPLATE, template)
                                                .with(NoteProperties.TYPEFACE, typeface);

        return builder.build();
    }

    private <T, U> T getOrDefault(Map<U, T> map, U key, T defaultValue) {
        if (map == null || !map.containsKey(key)) {
            return defaultValue;
        }

        T value = map.get(key);
        return value == null ? defaultValue : value;
    }

    /**
     * Tuple object holding a NoteTemplate along with its TypefaceRequest.
     */
    private class RequestTuple {
        public final NoteTemplate template;
        public final TypefaceRequest typefaceRequest;

        public RequestTuple(NoteTemplate template) {
            this.template = template;
            this.typefaceRequest = TypefaceRequest.createFromTextStyle(template.textStyle);
        }
    }
}
