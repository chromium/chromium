// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.example.autofill_service.fill_service.helpers;

import android.content.Context;
import android.service.autofill.Dataset;
import android.service.autofill.Field;
import android.service.autofill.FillRequest;
import android.service.autofill.FillResponse;
import android.service.autofill.Presentations;
import android.service.autofill.SaveInfo;
import android.util.ArrayMap;
import android.view.autofill.AutofillId;
import android.view.autofill.AutofillValue;
import android.widget.RemoteViews;
import android.widget.Toast;

import androidx.annotation.Nullable;

import org.chromium.example.autofill_service.R;

import java.util.Collection;
import java.util.Map;

/** Helps to build simple responses to {@link FillRequests}. */
public class ResponseHelper {

    private static final int NUMBER_DATASETS = 6; // Fixed number of datasets sent on each request.
    private final Context mContext;
    private final InlineRequestHelper mInlineHelper;

    public static @Nullable FillResponse createSimpleResponse(
            Context context, FillRequest request) {
        final ArrayMap<String, AutofillId> fields =
                ViewStructureParser.findAutofillableFields(request);
        if (fields.isEmpty()) {
            Toast.makeText(
                            context,
                            "InlineFillService could not figure out how to autofill this screen",
                            Toast.LENGTH_LONG)
                    .show();
            return null; // Valid value to pass to FillCallback: It signals no suggestions.
        }

        return new ResponseHelper(context, new InlineRequestHelper(context, request))
                .buildResponse(fields);
    }

    private ResponseHelper(Context context, InlineRequestHelper inlineRequestHelper) {
        mContext = context;
        mInlineHelper = inlineRequestHelper;
    }

    private int getDatasetCount() {
        return mInlineHelper.restrictMaxSuggestionCount(NUMBER_DATASETS);
    }

    private FillResponse buildResponse(ArrayMap<String, AutofillId> fields) {

        FillResponse.Builder response = new FillResponse.Builder();

        for (int i = 0; i < getDatasetCount(); i++) {
            response.addDataset(newUnlockedDataset(fields, i));
        }

        if (mInlineHelper.hasInlineRequest()) {
            response.addDataset(
                    mInlineHelper.createInlineActionDataset(fields, R.drawable.cr_fill));
        }

        response.setSaveInfo(createSaveInfoForIds(fields.values()));

        return response.build();
    }

    private Dataset newUnlockedDataset(Map<String, AutofillId> fields, int index) {
        Dataset.Builder dataset = new Dataset.Builder();
        fields.forEach((hint, id) -> dataset.setField(id, createField(index, hint)));
        return dataset.build();
    }

    private Field createField(int index, String hint) {
        final String value = hint + (index + 1);
        final String displayValue =
                hint.contains("password") ? "password for #" + (index + 1) : value;
        Presentations.Builder presentationsBuilder =
                new Presentations.Builder()
                        .setDialogPresentation(newDatasetPresentation(displayValue));
        if (mInlineHelper.hasInlineRequest()) {
            presentationsBuilder.setInlinePresentation(
                    mInlineHelper.createInlineDataset(displayValue, index));
        }
        return new Field.Builder()
                .setPresentations(presentationsBuilder.build())
                .setValue(AutofillValue.forText(value))
                .build();
    }

    private RemoteViews newDatasetPresentation(CharSequence text) {
        RemoteViews presentation = new RemoteViews(mContext.getPackageName(), R.layout.list_item);
        presentation.setTextViewText(R.id.text, text);
        return presentation;
    }

    private static SaveInfo createSaveInfoForIds(Collection<AutofillId> ids) {
        AutofillId[] requiredIds = new AutofillId[ids.size()];
        ids.toArray(requiredIds);
        return new SaveInfo.Builder(SaveInfo.SAVE_DATA_TYPE_GENERIC, requiredIds).build();
    }
}
