// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FormFieldFocusType} from './constants.js';
import type {DocumentDimensionsMessageData} from './pdf_viewer_utils.js';

export function convertDocumentDimensionsMessage(message: any) {
  return message as unknown as DocumentDimensionsMessageData;
}

export function convertFormFocusChangeMessage(message: any) {
  return message as unknown as {focused: FormFieldFocusType};
}

export function convertLoadProgressMessage(message: any) {
  return message as unknown as {progress: number};
}
