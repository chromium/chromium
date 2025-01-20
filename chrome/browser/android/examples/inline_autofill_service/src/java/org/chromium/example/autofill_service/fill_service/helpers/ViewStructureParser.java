// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.example.autofill_service.fill_service.helpers;

import android.app.assist.AssistStructure;
import android.service.autofill.FillContext;
import android.service.autofill.FillRequest;
import android.text.TextUtils;
import android.util.ArrayMap;
import android.util.Pair;
import android.view.View;
import android.view.autofill.AutofillId;

import androidx.annotation.NonNull;

import java.util.List;
import java.util.Map;

final class ViewStructureParser {
    /** Extracts the autofillable fields from the request through assist structure. */
    static ArrayMap<String, AutofillId> findAutofillableFields(@NonNull FillRequest request) {
        return findAutofillableFields(getLatestAssistStructure(request));
    }

    /**
     * ViewStructureParser method to get the {@link AssistStructure} associated with the latest
     * request in an autofill context.
     */
    @NonNull
    private static AssistStructure getLatestAssistStructure(@NonNull FillRequest request) {
        List<FillContext> fillContexts = request.getFillContexts();
        return fillContexts.get(fillContexts.size() - 1).getStructure();
    }

    /**
     * Parses the {@link AssistStructure} representing the activity being autofilled, and returns a
     * map of autofillable fields (represented by their autofill ids) mapped by the hint associate
     * with them.
     *
     * <p>An autofillable field is a {@link AssistStructure.ViewNode} whose getHint(ViewNode)
     * method.
     */
    @NonNull
    private static ArrayMap<String, AutofillId> findAutofillableFields(
            @NonNull AssistStructure structure) {
        ArrayMap<String, AutofillId> fields = new ArrayMap<>();
        int nodes = structure.getWindowNodeCount();
        for (int i = 0; i < nodes; i++) {
            AssistStructure.ViewNode node = structure.getWindowNodeAt(i).getRootViewNode();
            addAutofillableFields(fields, node);
        }
        ArrayMap<String, AutofillId> result = new ArrayMap<>();
        int filedCount = fields.size();
        for (int i = 0; i < filedCount; i++) {
            String key = fields.keyAt(i);
            AutofillId value = fields.valueAt(i);
            // For fields with no hint we just use Field
            if (key.equals(value.toString())) {
                result.put("Field:" + i + "-", fields.valueAt(i));
            } else {
                result.put(key, fields.valueAt(i));
            }
        }
        return result;
    }

    /**
     * Adds any autofillable view from the {@link AssistStructure.ViewNode} and its descendants to
     * the map.
     */
    private static void addAutofillableFields(
            @NonNull Map<String, AutofillId> fields, @NonNull AssistStructure.ViewNode node) {
        if (node.getAutofillType() == View.AUTOFILL_TYPE_TEXT) {
            if (!fields.containsValue(node.getAutofillId())) {
                fields.put(getFieldKey(fields, node), node.getAutofillId());
            }
        }
        int childrenSize = node.getChildCount();
        for (int i = 0; i < childrenSize; i++) {
            addAutofillableFields(fields, node.getChildAt(i));
        }
    }

    private static String getFieldKey(
            @NonNull Map<String, AutofillId> fields, AssistStructure.ViewNode node) {
        if (!TextUtils.isEmpty(node.getHint())) {
            final String key = node.getHint().toLowerCase();
            if (!fields.containsKey(key)) return key;
        }
        if (node.getAutofillHints() != null && node.getAutofillHints().length > 0) {
            final String key = node.getAutofillHints()[0].toLowerCase();
            if (!fields.containsKey(key)) return key;
        }
        String name = null;
        String id = null;
        if (node.getHtmlInfo() != null) {
            for (Pair<String, String> kv : node.getHtmlInfo().getAttributes()) {
                if ("type".equals(kv.first.toLowerCase())) {
                    return kv.second.toLowerCase();
                }
                if ("name".equals(kv.first.toLowerCase())) {
                    name = kv.second.toLowerCase();
                }
                if ("id".equals(kv.first.toLowerCase())) {
                    id = kv.second.toLowerCase();
                }
            }
        }
        if (!TextUtils.isEmpty(name)) return name;
        if (!TextUtils.isEmpty(id)) return id;
        return node.getAutofillId().toString();
    }
}
