// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FileSuggestionHandlerRemote} from '../../file_suggestion.mojom-webui.js';
import {FileSuggestionHandler} from '../../file_suggestion.mojom-webui.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending requests from NTP dummy module JS to the browser
 * and receiving the browser response.
 */

let handler: FileSuggestionHandlerRemote|null = null;

export class FileProxy {
  static getHandler(): FileSuggestionHandlerRemote {
    return handler || (handler = FileSuggestionHandler.getRemote());
  }

  static setHandler(newHandler: FileSuggestionHandlerRemote) {
    handler = newHandler;
  }

  private constructor() {}
}
