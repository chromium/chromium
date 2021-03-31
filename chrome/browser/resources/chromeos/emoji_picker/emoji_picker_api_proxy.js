// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';


import {PageHandlerFactory, PageHandlerRemote} from './emoji_picker.mojom-webui.js';

/** @interface */
export class EmojiPickerApiProxy {
  showUI() {}
  /**
   *
   * @param {string} emoji
   * @param {boolean} isVariant
   */
  insertEmoji(emoji, isVariant) {}

  /**
   * @returns {Promise<{incognito:boolean}>}
   */
  isIncognitoTextField() {}
}
/** @implements {EmojiPickerApiProxy} */
export class EmojiPickerApiProxyImpl {
  constructor() {
    /** @type {!PageHandlerRemote} */
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(this.handler.$.bindNewPipeAndPassReceiver());
  }

  /** @override */
  showUI() {
    this.handler.showUI();
  }
  /** @override */
  insertEmoji(emoji, isVariant) {
    this.handler.insertEmoji(emoji, isVariant);
  }

  /** @override */
  isIncognitoTextField() {
    return this.handler.isIncognitoTextField();
  }
}

addSingletonGetter(EmojiPickerApiProxyImpl);