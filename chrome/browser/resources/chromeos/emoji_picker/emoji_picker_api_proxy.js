// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';


import {Feature, PageHandlerFactory, PageHandlerRemote} from './emoji_picker.mojom-webui.js';

/** @interface */
export class EmojiPickerApiProxy {
  showUI() {}
  /**
   *
   * @param {string} emoji
   * @param {boolean} isVariant
   * @param {number} searchLength
   */
  insertEmoji(emoji, isVariant, searchLength) {}

  /**
   * @returns {Promise<{incognito:boolean}>}
   */
  isIncognitoTextField() {}

  /**
   * @returns {Promise<{featureList:!Array<!Feature>}>}
   */
  getFeatureList() {}
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
  insertEmoji(emoji, isVariant, searchLength) {
    this.handler.insertEmoji(emoji, isVariant, searchLength);
  }

  /** @override */
  isIncognitoTextField() {
    return this.handler.isIncognitoTextField();
  }

  /** @override */
  getFeatureList() {
    return this.handler.getFeatureList();
  }
}

addSingletonGetter(EmojiPickerApiProxyImpl);
