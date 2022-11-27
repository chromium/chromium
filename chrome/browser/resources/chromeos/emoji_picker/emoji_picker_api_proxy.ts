// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {addSingletonGetter} from 'chrome://resources/ash/common/cr_deprecated.js';

import {PageHandlerFactory, PageHandlerRemote} from './emoji_picker.mojom-webui.js';

/** @interface */
export interface EmojiPickerApiProxy {
  showUI(): void;

  insertEmoji(emoji: string, isVariant: boolean, searchLength: number): void;

  isIncognitoTextField(): Promise<{incognito: boolean}>;

  getFeatureList(): Promise<{featureList: boolean[]}>;
}

/** @implements {EmojiPickerApiProxy} */
export class EmojiPickerApiProxyImpl {
  handler = new PageHandlerRemote();
  constructor() {
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(this.handler.$.bindNewPipeAndPassReceiver());
  }

  /** @override */
  showUI() {
    this.handler.showUI();
  }
  /** @override */
  insertEmoji(emoji: string, isVariant: boolean, searchLength: number) {
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
