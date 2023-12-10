// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test;

import android.app.assist.AssistStructure;
import android.app.assist.AssistStructure.ViewNode;
import android.os.CancellationSignal;
import android.service.autofill.AutofillService;
import android.service.autofill.Dataset;
import android.service.autofill.FillCallback;
import android.service.autofill.FillContext;
import android.service.autofill.FillRequest;
import android.service.autofill.FillResponse;
import android.service.autofill.SaveCallback;
import android.service.autofill.SaveRequest;
import android.util.Log;
import android.util.Pair;
import android.view.View;
import android.view.autofill.AutofillId;
import android.view.autofill.AutofillValue;
import android.widget.RemoteViews;

import androidx.annotation.NonNull;

import org.chromium.webview_ui_test.test.util.AutofillProfile;

import java.util.ArrayList;
import java.util.List;

/** An {@link AutofillService} implementation which provides static responses, useful for testing. */
public class TestAutofillService extends AutofillService {
    private static final long EPOCH_TIME = 946684800000L; // Jan 1, 2000.
    private static final String TAG = "TestAutofillService";
    private static final String PACKAGE_NAME = "org.chromium.webview_ui_test.test";
    private static final String DEFAULT_FILL = "default";

    // Called by Autofill architecture to fill autofillable fields.
    @Override
    public void onFillRequest(
            FillRequest request, CancellationSignal cancellationSignal, FillCallback callback)
            throws Error {
        try {
            handleRequest(request, callback);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            raiseError("onFillRequest() interrupted", e, callback);
        } catch (Exception e) {
            raiseError("exception on onFillRequest()", e, callback);
        }
    }

    // Not useful for our testing purposes.
    @Override
    public void onSaveRequest(SaveRequest request, SaveCallback callback) {
        callback.onFailure(
                "onSaveRequest() should not have been called."
                        + "This service only supplies onFillRequest()");
    }

    // Collects all Autofill nodes in a UI ViewNode.
    private List<ViewNode> collectAutofillNodes(ViewNode node) {
        List<ViewNode> nodes = new ArrayList<>();
        if (node.getAutofillType() != View.AUTOFILL_TYPE_NONE) {
            nodes.add(node);
        }
        int childrenSize = node.getChildCount();
        for (int i = 0; i < childrenSize; i++) {
            nodes.addAll(collectAutofillNodes(node.getChildAt(i)));
        }
        return nodes;
    }

    /*
     * Returns a response based upon the values stored in the AutofillProfile provided.
     * For Autofill Types TOGGLE, and DATE, we ignore the field but otherwise fill in the form.
     */
    private void handleRequest(FillRequest request, FillCallback callback) throws Exception {
        // Grab all ViewNodes.
        List<FillContext> contexts = request.getFillContexts();
        if (contexts.isEmpty()) {
            Log.w(TAG, "Request has no contexts");
            callback.onSuccess(null);
            return; // We can't fulfill a request without the current context.
        }
        List<ViewNode> nodes = new ArrayList<>();
        FillContext context = contexts.get(contexts.size() - 1); // Get current context
        AssistStructure struct = context.getStructure();

        if (struct == null || struct.getWindowNodeCount() == 0) {
            Log.w(TAG, "Request has no ViewNodes");
            callback.onSuccess(null);
            return; // Nothing to autofill
        }

        for (int i = 0; i < struct.getWindowNodeCount(); i++) {
            ViewNode rootNode = struct.getWindowNodeAt(i).getRootViewNode();
            nodes.addAll(collectAutofillNodes(rootNode));
        }
        // Build response
        Dataset.Builder dataset = new Dataset.Builder(createRemoteViews("dataset"));
        for (ViewNode node : nodes) {
            AutofillId autofillId = node.getAutofillId();
            AutofillValue value = null;

            String fill = DEFAULT_FILL;
            switch (node.getAutofillType()) {
                case View.AUTOFILL_TYPE_TEXT:
                    fill = getFillFromProfile(node);
                    value = AutofillValue.forText(fill);
                    dataset.setValue(autofillId, value, createRemoteViews(fill));
                    break;
                case View.AUTOFILL_TYPE_LIST:
                    fill = getFillFromProfile(node);
                    value = AutofillValue.forList(findFillIndex(node, fill));
                    dataset.setValue(autofillId, value, createRemoteViews(fill));
                    break;
                case View.AUTOFILL_TYPE_TOGGLE:
                    Log.d(TAG, "Ignoring ViewNode: " + autofillId + "because it has type toggle.");
                    break;
                case View.AUTOFILL_TYPE_DATE:
                    Log.d(TAG, "Ignoring ViewNode: " + autofillId + "because it has type date.");
                    break;
                default:
                    throw new Exception(
                            "TestAutofillService should not fill node with AutofillType NONE");
            }
        }
        if (nodes.size() > 0) {
            FillResponse.Builder fillResponse = new FillResponse.Builder();
            FillResponse response = fillResponse.addDataset(dataset.build()).build();
            Log.w(TAG, "Success: returning FillResponse: " + response.toString());
            callback.onSuccess(response);
        } else {
            Log.w(TAG, "Dataset contains zero ViewNodes");
            callback.onSuccess(null);
        }
    }

    // Takes the AutofillHint from the given ViewNode and returns the matching value from profile.
    private String getFillFromProfile(ViewNode node) throws Exception {
        // (crbug/1473156) Move this to initialization of service once 'uber profile' made.
        AutofillProfile profile = new AutofillProfile("us.profile");
        List<Pair<String, String>> attributes = node.getHtmlInfo().getAttributes();
        String hint = DEFAULT_FILL;
        for (Pair<String, String> attribute : attributes) {
            if (attribute.first.equals("ua-autofill-hints")) {
                hint = attribute.second;
            }
        }
        if (hint.equals(DEFAULT_FILL)) {
            throw new NullPointerException("No ua-autofill-hint for element");
        }
        if (profile.profileMap.containsKey(hint)) {
            return profile.profileMap.get(hint);
        } else {
            throw new NullPointerException("Profile has no type with hint " + hint);
        }
    }

    // Finds the index to select in dropdown list.
    private int findFillIndex(ViewNode node, String fillString) throws Exception {
        CharSequence[] options = node.getAutofillOptions();
        for (int i = 0; i < options.length; i++) {
            if (options[i].toString().equals(fillString)) {
                return i;
            }
        }
        throw new Exception("No dropdown found with fill string " + fillString);
    }

    private void raiseError(
            @NonNull String msg, @NonNull Exception e, @NonNull FillCallback callback) {
        Log.e(TAG, msg, e);
        callback.onFailure(msg);
    }

    @NonNull
    private static RemoteViews createRemoteViews(@NonNull CharSequence text) {
        RemoteViews presentation =
                new RemoteViews(PACKAGE_NAME, R.layout.autofill_dataset_picker_text_only);
        presentation.setTextViewText(R.id.text, text);
        return presentation;
    }
}
