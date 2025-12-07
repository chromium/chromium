// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SOMETHING_WENT_WRONG_ERROR_MSG} from './constants.js';
import {getTemplate} from './emoji_error.html.js';
import {createCustomEvent, GIF_ERROR_TRY_AGAIN} from './events.js';
import {Status} from './tenor_types.mojom-webui.js';

export class EmojiErrorComponent extends PolymerElement {
  static get is() {
    return 'emoji-error' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      status: {type: Status},
      errorMessage: {type: String},
    };
  }
  private status: Status;
  private errorMessage: string;

  isGifInHttpErrorState(status: Status): boolean {
    return status === Status.kHttpError;
  }

  isGifInNetworkErrorState(status: Status): boolean {
    return status === Status.kNetError;
  }

  getErrorMessage(status: Status): string {
    return status === Status.kNetError ? this.errorMessage :
                                         SOMETHING_WENT_WRONG_ERROR_MSG;
  }

  onClickTryAgain() {
    this.dispatchEvent(createCustomEvent(GIF_ERROR_TRY_AGAIN, {}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EmojiErrorComponent.is]: EmojiErrorComponent;
  }
  interface HTMLElementEventMap {
    [GIF_ERROR_TRY_AGAIN]: CustomEvent;
  }
}

customElements.define(EmojiErrorComponent.is, EmojiErrorComponent);
