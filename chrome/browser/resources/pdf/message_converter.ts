// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ExtendedKeyEvent, FormFieldFocusType} from './constants.js';
import type {MessageData} from './controller.js';
import {deserializeKeyEvent} from './pdf_scripting_api.js';
import type {SerializedKeyEvent} from './pdf_scripting_api.js';
import type {DocumentDimensionsMessageData} from './pdf_viewer_utils.js';

type KeyEventData = MessageData&{keyEvent: SerializedKeyEvent};

export function convertDocumentDimensionsMessage(message: any) {
  return message as unknown as DocumentDimensionsMessageData;
}

export function convertFormFocusChangeMessage(message: any) {
  return message as unknown as {focused: FormFieldFocusType};
}

export function convertLoadProgressMessage(message: any) {
  return message as unknown as {progress: number};
}

export function convertSendKeyEventMessage(message: any): ExtendedKeyEvent {
  const keyEventData = message as KeyEventData;
  return deserializeKeyEvent(keyEventData.keyEvent) as ExtendedKeyEvent;
}
