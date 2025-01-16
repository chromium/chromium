// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.example.autofill_service.fill_service.helpers;

import android.app.PendingIntent;
import android.app.slice.Slice;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Icon;
import android.service.autofill.Dataset;
import android.service.autofill.FillRequest;
import android.service.autofill.InlinePresentation;
import android.text.TextUtils;
import android.util.ArrayMap;
import android.view.autofill.AutofillId;
import android.view.inputmethod.InlineSuggestionsRequest;
import android.widget.inline.InlinePresentationSpec;

import androidx.annotation.Nullable;
import androidx.autofill.inline.v1.InlineSuggestionUi;
import androidx.autofill.inline.v1.InlineSuggestionUi.Content;

import org.chromium.example.autofill_service.SettingsActivity;

class InlineRequestHelper {

    private final @Nullable InlineSuggestionsRequest mInlineRequest;
    private final Context mContext;

    InlineRequestHelper(Context context, FillRequest fillRequest) {
        mContext = context;
        mInlineRequest = getInlineSuggestionsRequest(fillRequest);
    }

    private static @Nullable InlineSuggestionsRequest getInlineSuggestionsRequest(
            FillRequest request) {
        final InlineSuggestionsRequest inlineRequest = request.getInlineSuggestionsRequest();
        if (inlineRequest != null
                && inlineRequest.getMaxSuggestionCount() > 0
                && !inlineRequest.getInlinePresentationSpecs().isEmpty()) {
            return inlineRequest;
        }
        return null;
    }

    int restrictMaxSuggestionCount(int max) {
        return mInlineRequest != null ? Math.min(max, mInlineRequest.getMaxSuggestionCount()) : max;
    }

    boolean hasInlineRequest() {
        return mInlineRequest != null;
    }

    InlinePresentation createInlineDataset(String value, int index) {
        assert hasInlineRequest();
        final PendingIntent attribution =
                createAttribution("Please tap on the chip to autofill the value:" + value);
        final Slice slice = createSlice(value, null, attribution);
        index = Math.min(mInlineRequest.getInlinePresentationSpecs().size() - 1, index);
        final InlinePresentationSpec spec = mInlineRequest.getInlinePresentationSpecs().get(index);
        return new InlinePresentation(slice, spec, false);
    }

    Dataset createInlineActionDataset(ArrayMap<String, AutofillId> fields, int drawable) {
        PendingIntent pendingIntent =
                PendingIntent.getActivity(
                        mContext,
                        0,
                        new Intent(mContext, SettingsActivity.class),
                        PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_MUTABLE);
        Dataset.Builder builder =
                new Dataset.Builder()
                        .setInlinePresentation(createInlineAction(drawable))
                        .setAuthentication(pendingIntent.getIntentSender());
        for (AutofillId fieldId : fields.values()) {
            builder.setValue(fieldId, null);
        }
        return builder.build();
    }

    private InlinePresentation createInlineAction(int drawable) {
        assert hasInlineRequest();
        return new InlinePresentation(
                createSlice(
                        null,
                        Icon.createWithResource(mContext, drawable),
                        createAttribution("Please tap on the chip to launch the action.")),
                mInlineRequest.getInlinePresentationSpecs().get(0), // Reuse first spec's height.
                true);
    }

    private PendingIntent createAttribution(String msg) {
        Intent intent = new Intent(mContext, AttributionDialogActivity.class);
        intent.putExtra(AttributionDialogActivity.KEY_MSG, msg);
        return PendingIntent.getActivity(
                mContext,
                msg.hashCode(), // Different request code avoids overriding previous intents.
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_MUTABLE);
    }

    private static Slice createSlice(String title, Icon startIcon, PendingIntent attribution) {
        Content.Builder builder = InlineSuggestionUi.newContentBuilder(attribution);
        if (!TextUtils.isEmpty(title)) {
            builder.setTitle(title);
        }
        if (startIcon != null) {
            builder.setStartIcon(startIcon);
        }
        return builder.build().getSlice();
    }
}
