// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertI18nString} from '../assert.js';
import * as dom from '../dom.js';
import {I18nString} from '../i18n_string.js';
import {getI18nMessage} from '../models/load_time_data.js';
import {ViewName} from '../type.js';

import {FlashEnterOptions, View} from './view.js';

export class Flash extends View {
  private readonly processingMessage: HTMLElement;

  constructor() {
    super(ViewName.FLASH);
    this.processingMessage = dom.get('#view-flash .msg', HTMLElement);
  }

  // TODO(b/288043983): Fill the message for every call, otherwise set the
  // message to an empty string.
  override entering(options?: FlashEnterOptions): void {
    const processingText = options !== undefined ?
        assertI18nString(options) :
        I18nString.MSG_PROCESSING_IMAGE;
    this.processingMessage.textContent = getI18nMessage(processingText);
  }
}
