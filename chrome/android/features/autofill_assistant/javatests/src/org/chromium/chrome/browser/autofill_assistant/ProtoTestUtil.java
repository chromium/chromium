// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.chrome.browser.autofill_assistant.proto.ClientIdProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto.Filter;

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
}
