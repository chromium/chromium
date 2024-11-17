// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DriveSuggestionHandlerRemote} from '../../../drive_suggestion.mojom-webui.js';
import {DriveSuggestionHandler} from '../../../drive_suggestion.mojom-webui.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending requests from NTP dummy module JS to the browser
 * and receiving the browser response.
 */

let handler: DriveSuggestionHandlerRemote|null = null;

export class FileProxy {
  static getHandler(): DriveSuggestionHandlerRemote {
    return handler || (handler = DriveSuggestionHandler.getRemote());
  }

  static setHandler(newHandler: DriveSuggestionHandlerRemote) {
    handler = newHandler;
  }

  private constructor() {}
}
