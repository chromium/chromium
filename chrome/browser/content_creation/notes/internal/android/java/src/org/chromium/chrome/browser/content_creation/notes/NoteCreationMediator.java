// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import android.graphics.Typeface;
import android.os.Looper;

import org.chromium.chrome.browser.content_creation.notes.fonts.GoogleFontService;
import org.chromium.chrome.browser.content_creation.notes.fonts.TypefaceRequest;
import org.chromium.chrome.browser.content_creation.notes.fonts.TypefaceResponse;
import org.chromium.chrome.browser.content_creation.notes.images.ImageService;
import org.chromium.components.content_creation.notes.NoteService;
import org.chromium.components.content_creation.notes.models.Background;
import org.chromium.components.content_creation.notes.models.ImageBackground;
import org.chromium.components.content_creation.notes.models.NoteTemplate;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Mediator for the note creation component. */
public class NoteCreationMediator {
    private final ModelList mListModel;
    private final NoteService mNoteService;
    private final GoogleFontService mFontService;
    private final ImageService mImageService;

    /**
     * Constructor which will also kick-off the asynchronous retrieval of Note
     * templates.
     */
    public NoteCreationMediator(
            ModelList listModel,
            GoogleFontService fontService,
            NoteService noteService,
            ImageService imageService) {
        mListModel = listModel;
        mNoteService = noteService;
        mFontService = fontService;
        mImageService = imageService;

        mNoteService.getTemplates(this::loadResources);
    }

    // Ensures that resources are loaded (async) before sending the templates to
    // the UI. These resources include background images and font typefaces.
    private void loadResources(List<NoteTemplate> templates) {
        assert templates != null;

        List<Background> backgrounds = new ArrayList<>();
        for (NoteTemplate template : templates) {
            backgrounds.add(template.mainBackground);
            if (template.contentBackground != null) {
                backgrounds.add(template.contentBackground);
            }
        }

        mImageService.resolveBackgrounds(
                backgrounds,
                () -> {
                    onBackgroundsResolved(templates);
                });
    }

    private void onBackgroundsResolved(List<NoteTemplate> templates) {
        // Templates with image backgrounds for which fetch failed should be skipped.
        for (Iterator<NoteTemplate> iter = templates.iterator(); iter.hasNext(); ) {
            NoteTemplate template = iter.next();

            if (template.mainBackground instanceof ImageBackground
                    && ((ImageBackground) template.mainBackground).isBitmapEmpty()) {
                iter.remove();
                continue;
            }

            if (template.contentBackground instanceof ImageBackground
                    && ((ImageBackground) template.contentBackground).isBitmapEmpty()) {
                iter.remove();
                continue;
            }
        }
        resolveTypefaces(templates);
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

        mFontService.fetchFonts(
                requests,
                new GoogleFontService.GoogleFontRequestCallback() {
                    @Override
                    public void onResponsesReceived(
                            Map<TypefaceRequest, TypefaceResponse> resultsMap) {
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

        // Only add a template to the ModelList when a Typeface has successfully been loaded for
        // it.
        int index = 0;
        for (RequestTuple tuple : requestTuples) {
            TypefaceResponse response = getOrDefault(resultsMap, tuple.typefaceRequest, null);
            if (response == null) {
                // TODO (crbug.com/1194168): Log this case.
                ++index;
                continue;
            }

            if (response.isError()) {
                // TODO (crbug.com/1194168): Log this case.
                ++index;
                continue;
            }

            ListItem listItem =
                    new ListItem(
                            NoteProperties.NOTE_VIEW_TYPE,
                            buildModel(
                                    index == 0,
                                    index == (requestTuples.size() - 1),
                                    tuple.template,
                                    response.typeface));
            mListModel.add(listItem);
            ++index;
        }
    }

    private PropertyModel buildModel(
            boolean isFirst, boolean isLast, NoteTemplate template, Typeface typeface) {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(NoteProperties.ALL_KEYS)
                        .with(NoteProperties.IS_FIRST, isFirst)
                        .with(NoteProperties.IS_LAST, isLast)
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

    /** Tuple object holding a NoteTemplate along with its TypefaceRequest. */
    private class RequestTuple {
        public final NoteTemplate template;
        public final TypefaceRequest typefaceRequest;

        public RequestTuple(NoteTemplate template) {
            this.template = template;
            this.typefaceRequest = TypefaceRequest.createFromTextStyle(template.textStyle);
        }
    }
}
