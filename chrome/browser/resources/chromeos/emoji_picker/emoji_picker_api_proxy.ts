// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {PageHandlerFactory, PageHandlerRemote} from './emoji_picker.mojom-webui.js';

/** @interface */
export interface EmojiPickerApiProxy {
  showUi(): void;

  insertEmoji(emoji: string, isVariant: boolean, searchLength: number): void;

  isIncognitoTextField(): Promise<{incognito: boolean}>;

  getFeatureList(): Promise<{featureList: number[]}>;
}

export class EmojiPickerApiProxyImpl implements EmojiPickerApiProxy {
  handler = new PageHandlerRemote();
  static instance: EmojiPickerApiProxy|null = null;
  constructor() {
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(this.handler.$.bindNewPipeAndPassReceiver());
  }

  /** @override */
  showUi() {
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

  static getInstance(): EmojiPickerApiProxy {
    if (EmojiPickerApiProxyImpl.instance === null) {
      EmojiPickerApiProxyImpl.instance = new EmojiPickerApiProxyImpl();
    }
    return EmojiPickerApiProxyImpl.instance;
  }
}