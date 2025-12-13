// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.example.autofill_service.fill_service.helpers;

import android.app.assist.AssistStructure;
import android.content.Context;
import android.service.autofill.Dataset;
import android.service.autofill.Field;
import android.service.autofill.FillContext;
import android.service.autofill.FillRequest;
import android.service.autofill.FillResponse;
import android.service.autofill.Presentations;
import android.service.autofill.SaveInfo;
import android.service.autofill.SaveRequest;
import android.util.ArrayMap;
import android.view.autofill.AutofillId;
import android.view.autofill.AutofillValue;
import android.widget.RemoteViews;
import android.widget.Toast;

import androidx.annotation.Nullable;

import org.chromium.example.autofill_service.R;
import org.chromium.example.autofill_service.fill_service.helpers.CredentialStorage.Credential;

import java.util.Collection;
import java.util.List;
import java.util.Map;

/** Helps to build simple responses to {@link FillRequests}. */
public class ResponseHelper {

    private final Context mContext;
    private final InlineRequestHelper mInlineHelper;
    private final List<Credential> mCredentials;

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
        CredentialStorage credentialStorage = new CredentialStorage(mContext);
        mCredentials = credentialStorage.getCredentials();
    }

    private int getDatasetCount() {
        return mInlineHelper.restrictMaxSuggestionCount(mCredentials.size());
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
        Credential credential = mCredentials.get(index);
        fields.forEach(
                (hint, id) -> {
                    if (hint.contains("username")) {
                        dataset.setField(
                                id, createField(credential.username, credential.username, index));
                    } else if (hint.contains("password")) {
                        dataset.setField(
                                id,
                                createField(
                                        credential.password,
                                        "Password for " + credential.username,
                                        index));
                    } else {
                        dataset.setField(
                                id, createField(hint, "Name for " + credential.username, index));
                    }
                });
        return dataset.build();
    }

    private Field createField(String value, String displayValue, int index) {
        Presentations.Builder presentationsBuilder =
                new Presentations.Builder()
                        .setDialogPresentation(newDatasetPresentation(displayValue))
                        .setMenuPresentation(newDatasetPresentation(displayValue));
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

    public static void handleSaveRequest(Context context, SaveRequest request) {
        List<FillContext> fillContexts = request.getFillContexts();
        AssistStructure structure = fillContexts.get(fillContexts.size() - 1).getStructure();
        final Map<String, String> credentials = new ArrayMap<>();

        // Traverse the view hierarchy to find username and password.
        int windowNodeCount = structure.getWindowNodeCount();
        for (int i = 0; i < windowNodeCount; i++) {
            AssistStructure.ViewNode rootNode = structure.getWindowNodeAt(i).getRootViewNode();
            traverseNode(rootNode, credentials);
        }

        String username = credentials.get("username");
        String password = credentials.get("password");

        if (username != null && password != null) {
            CredentialStorage credentialStorage = new CredentialStorage(context);
            credentialStorage.saveCredential(username, password);
            Toast.makeText(context, "Credentials saved.", Toast.LENGTH_SHORT).show();
        } else {
            Toast.makeText(context, "Could not save credentials.", Toast.LENGTH_LONG).show();
        }
    }

    private static void traverseNode(
            AssistStructure.ViewNode node, Map<String, String> credentials) {
        if (node.getAutofillHints() != null && node.getAutofillHints().length > 0) {
            String hint = node.getAutofillHints()[0].toLowerCase();
            CharSequence value = null;
            // For password fields, the value is in the AutofillValue.
            AutofillValue autofillValue = node.getAutofillValue();
            if (autofillValue != null && autofillValue.isText()) {
                value = autofillValue.getTextValue();
            } else if (node.getText() != null) {
                value = node.getText();
            }

            if (value != null) {
                if (hint.contains("username")) {
                    credentials.put("username", value.toString());
                } else if (hint.contains("password")) {
                    credentials.put("password", value.toString());
                }
            }
        }

        int childCount = node.getChildCount();
        for (int i = 0; i < childCount; i++) {
            traverseNode(node.getChildAt(i), credentials);
        }
    }
}
