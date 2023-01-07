// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.chrome.browser.autofill_assistant.proto.ClientIdProto;
import org.chromium.chrome.browser.autofill_assistant.proto.RequiredDataPiece;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto.Filter;
import org.chromium.chrome.browser.autofill_assistant.proto.ValueExpression;

class ProtoTestUtil {
    public static SelectorProto toCssSelector(String cssSelector) {
        return SelectorProto.newBuilder()
                .addFilters(Filter.newBuilder().setCssSelector(cssSelector))
                .build();
    }

    public static SelectorProto toIFrameCssSelector(String frameId, String cssSelector) {
        return SelectorProto.newBuilder()
                .addFilters(Filter.newBuilder().setCssSelector(frameId))
                .addFilters(Filter.newBuilder().setNthMatch(
                        SelectorProto.NthMatchFilter.newBuilder().setIndex(0)))
                .addFilters(Filter.newBuilder().setEnterFrame(
                        SelectorProto.EmptyFilter.getDefaultInstance()))
                .addFilters(Filter.newBuilder().setCssSelector(cssSelector))
                .build();
    }

    public static SelectorProto toVisibleCssSelector(String cssSelector) {
        return SelectorProto.newBuilder()
                .addFilters(SelectorProto.Filter.newBuilder().setCssSelector(cssSelector))
                .addFilters(SelectorProto.Filter.newBuilder().setBoundingBox(
                        SelectorProto.BoundingBoxFilter.getDefaultInstance()))
                .build();
    }

    public static ClientIdProto toClientId(String id) {
        return ClientIdProto.newBuilder().setIdentifier(id).build();
    }

    public static RequiredDataPiece.Builder buildRequiredDataPiece(String message, int key) {
        return RequiredDataPiece.newBuilder().setErrorMessage(message).setCondition(
                RequiredDataPiece.Condition.newBuilder().setKey(key).setNotEmpty(
                        RequiredDataPiece.NotEmptyCondition.newBuilder()));
    }

    public static ValueExpression.Builder buildValueExpression(int... keys) {
        ValueExpression.Builder valueExpression = ValueExpression.newBuilder();
        for (int key : keys) {
            valueExpression.addChunk(ValueExpression.Chunk.newBuilder().setKey(key));
        }
        return valueExpression;
    }
}
