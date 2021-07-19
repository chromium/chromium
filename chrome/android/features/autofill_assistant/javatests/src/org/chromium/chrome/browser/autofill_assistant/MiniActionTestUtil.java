// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toClientId;

import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CheckElementIsOnTopProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientIdProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementConditionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.FocusFieldProto;
import org.chromium.chrome.browser.autofill_assistant.proto.JsClickProto;
import org.chromium.chrome.browser.autofill_assistant.proto.KeyEvent;
import org.chromium.chrome.browser.autofill_assistant.proto.ReleaseElementsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ScrollIntoViewProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectFieldValueProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SendChangeEventProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SendClickEventProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SendKeyEventProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SendKeystrokeEventsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SendTapEventProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SetElementAttributeProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextValue;
import org.chromium.chrome.browser.autofill_assistant.proto.WaitForDocumentToBecomeInteractiveProto;
import org.chromium.chrome.browser.autofill_assistant.proto.WaitForDomProto;
import org.chromium.chrome.browser.autofill_assistant.proto.WaitForElementToBecomeStableProto;

import java.util.List;

class MiniActionTestUtil {
    private static void addWaitForDomStep(
            SelectorProto selector, ClientIdProto clientId, List<ActionProto> list) {
        list.add(ActionProto.newBuilder()
                         .setWaitForDom(
                                 WaitForDomProto.newBuilder().setTimeoutMs(1000).setWaitCondition(
                                         ElementConditionProto.newBuilder()
                                                 .setMatch(selector)
                                                 .setClientId(clientId)))
                         .build());
    }

    private static void addReleaseElementStep(ClientIdProto clientId, List<ActionProto> list) {
        list.add(ActionProto.newBuilder()
                         .setReleaseElements(
                                 ReleaseElementsProto.newBuilder().addClientIds(clientId))
                         .build());
    }

    private static void addClickOrTapPrepareSteps(ClientIdProto clientId, List<ActionProto> list) {
        list.add(ActionProto.newBuilder()
                         .setWaitForDocumentToBecomeInteractive(
                                 WaitForDocumentToBecomeInteractiveProto.newBuilder()
                                         .setClientId(clientId)
                                         .setTimeoutInMs(1000))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setScrollIntoView(ScrollIntoViewProto.newBuilder().setClientId(clientId))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setWaitForElementToBecomeStable(
                                 WaitForElementToBecomeStableProto.newBuilder()
                                         .setClientId(clientId)
                                         .setStableCheckMaxRounds(10)
                                         .setStableCheckIntervalMs(200))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setCheckElementIsOnTop(
                                 CheckElementIsOnTopProto.newBuilder().setClientId(clientId))
                         .build());
    }

    static void addClickSteps(ClientIdProto clientId, List<ActionProto> list) {
        addClickOrTapPrepareSteps(clientId, list);
        list.add(ActionProto.newBuilder()
                         .setSendClickEvent(SendClickEventProto.newBuilder().setClientId(clientId))
                         .build());
    }

    static void addClickSteps(SelectorProto selector, List<ActionProto> list) {
        ClientIdProto clientId = toClientId("e");

        addWaitForDomStep(selector, clientId, list);
        addClickSteps(clientId, list);
        addReleaseElementStep(clientId, list);
    }

    static void addTapSteps(ClientIdProto clientId, List<ActionProto> list) {
        addClickOrTapPrepareSteps(clientId, list);
        list.add(ActionProto.newBuilder()
                         .setSendTapEvent(SendTapEventProto.newBuilder().setClientId(clientId))
                         .build());
    }

    static void addTapSteps(SelectorProto selector, List<ActionProto> list) {
        ClientIdProto clientId = toClientId("e");

        addWaitForDomStep(selector, clientId, list);
        addTapSteps(clientId, list);
        addReleaseElementStep(clientId, list);
    }

    static void addJsClickSteps(SelectorProto selector, List<ActionProto> list) {
        ClientIdProto clientId = toClientId("e");

        addWaitForDomStep(selector, clientId, list);
        list.add(ActionProto.newBuilder()
                         .setJsClick(JsClickProto.newBuilder().setClientId(clientId))
                         .build());
        addReleaseElementStep(clientId, list);
    }

    static void addSetValueSteps(
            ClientIdProto clientId, TextValue textValue, List<ActionProto> list) {
        list.add(ActionProto.newBuilder()
                         .setSetElementAttribute(SetElementAttributeProto.newBuilder()
                                                         .setClientId(clientId)
                                                         .addAttribute("value")
                                                         .setValue(textValue))
                         .build());
        list.add(
                ActionProto.newBuilder()
                        .setSendChangeEvent(SendChangeEventProto.newBuilder().setClientId(clientId))
                        .build());
    }

    static void addSetValueSteps(ClientIdProto clientId, String value, List<ActionProto> list) {
        addSetValueSteps(clientId, TextValue.newBuilder().setText(value).build(), list);
    }

    static void addSetValueSteps(
            SelectorProto selector, TextValue textValue, List<ActionProto> list) {
        ClientIdProto clientId = toClientId("e");

        addWaitForDomStep(selector, clientId, list);
        addSetValueSteps(clientId, textValue, list);
        addReleaseElementStep(clientId, list);
    }

    static void addSetValueSteps(SelectorProto selector, String value, List<ActionProto> list) {
        addSetValueSteps(selector, TextValue.newBuilder().setText(value).build(), list);
    }

    static void addKeyboardSteps(
            SelectorProto selector, TextValue textValue, List<ActionProto> list) {
        ClientIdProto clientId = toClientId("e");

        addWaitForDomStep(selector, clientId, list);
        addSetValueSteps(clientId, "", list);
        if (!textValue.hasText() || !textValue.getText().isEmpty()) {
            addClickSteps(clientId, list);
            list.add(ActionProto.newBuilder()
                             .setSendKeystrokeEvents(SendKeystrokeEventsProto.newBuilder()
                                                             .setClientId(clientId)
                                                             .setDelayInMs(0)
                                                             .setValue(textValue))
                             .build());
        }
        addReleaseElementStep(clientId, list);
    }

    static void addKeyboardSteps(SelectorProto selector, String value, List<ActionProto> list) {
        addKeyboardSteps(selector, TextValue.newBuilder().setText(value).build(), list);
    }

    static void addKeyboardWithSelectSteps(
            SelectorProto selector, String value, List<ActionProto> list) {
        ClientIdProto clientId = toClientId("e");

        addWaitForDomStep(selector, clientId, list);
        list.add(ActionProto.newBuilder()
                         .setSelectFieldValue(
                                 SelectFieldValueProto.newBuilder().setClientId(clientId))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setSendKeyEvent(
                                 SendKeyEventProto.newBuilder().setClientId(clientId).setKeyEvent(
                                         KeyEvent.newBuilder()
                                                 .addCommand("SelectAll")
                                                 .addCommand("DeleteBackward")
                                                 .setKey("Backspace")))
                         .build());
        if (!value.isEmpty() && !"\b".equals(value)) {
            list.add(ActionProto.newBuilder()
                             .setSendKeystrokeEvents(
                                     SendKeystrokeEventsProto.newBuilder()
                                             .setClientId(clientId)
                                             .setDelayInMs(0)
                                             .setValue(TextValue.newBuilder().setText(value)))
                             .build());
        }
        addReleaseElementStep(clientId, list);
    }

    static void addKeyboardWithFocusSteps(
            SelectorProto selector, String value, List<ActionProto> list) {
        ClientIdProto clientId = toClientId("e");

        addWaitForDomStep(selector, clientId, list);
        addSetValueSteps(clientId, "", list);
        if (!value.isEmpty()) {
            list.add(ActionProto.newBuilder()
                             .setFocusField(FocusFieldProto.newBuilder().setClientId(clientId))
                             .build());
            list.add(ActionProto.newBuilder()
                             .setSendKeystrokeEvents(
                                     SendKeystrokeEventsProto.newBuilder()
                                             .setClientId(clientId)
                                             .setDelayInMs(0)
                                             .setValue(TextValue.newBuilder().setText(value)))
                             .build());
        }
        addReleaseElementStep(clientId, list);
    }
}
