// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {PageHandlerFactory, PageHandlerRemote, Status, TenorGifResponse} from './emoji_picker.mojom-webui.js';
import {EmojiVariants, GifSubcategoryData, VisualContent} from './types.js';

/** @interface */
export interface EmojiPickerApiProxy {
  showUi(): void;

  insertEmoji(emoji: string, isVariant: boolean, searchLength: number): void;

  insertGif(gif: Url): void;

  isIncognitoTextField(): Promise<{incognito: boolean}>;

  getFeatureList(): Promise<{featureList: number[]}>;

  getCategories(): Promise<{gifCategories: GifSubcategoryData[]}>;

  getFeaturedGifs(pos?: string):
      Promise<{status: Status, featuredGifs: TenorGifResponse}>;

  searchGifs(query: string, pos?: string):
      Promise<{status: Status, searchGifs: TenorGifResponse}>;

  getGifsByIds(ids: string[]):
      Promise<{status: Status, selectedGifs: VisualContent[]}>;

  convertTenorGifsToEmoji(gifs: TenorGifResponse): EmojiVariants[];

  onUiFullyLoaded(): void;
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
  insertGif(gif: Url) {
    this.handler.insertGif(gif);
  }

  /** @override */
  isIncognitoTextField() {
    return this.handler.isIncognitoTextField();
  }

  /** @override */
  getFeatureList() {
    return this.handler.getFeatureList();
  }

  /** @override */
  async getCategories(): Promise<{gifCategories: GifSubcategoryData[]}> {
    const {gifCategories} = await this.handler.getCategories();
    return {
      gifCategories: gifCategories.map((category) => ({name: category})),
    };
  }

  /** @override */
  getFeaturedGifs(pos?: string):
      Promise<{status: Status, featuredGifs: TenorGifResponse}> {
    if (!navigator.onLine) {
      return Promise.resolve({
        status: Status.kNetError,
        featuredGifs: {
          next: '',
          results: [],
        },
      });
    }
    return this.handler.getFeaturedGifs(pos || null);
  }

  /** @override */
  searchGifs(query: string, pos?: string):
      Promise<{status: Status, searchGifs: TenorGifResponse}> {
    if (!navigator.onLine) {
      return Promise.resolve({
        status: Status.kNetError,
        searchGifs: {
          next: '',
          results: [],
        },
      });
    }
    return this.handler.searchGifs(query, pos || null);
  }

  /** @override */
  getGifsByIds(ids: string[]):
      Promise<{status: Status, selectedGifs: VisualContent[]}> {
    return this.handler.getGifsByIds(ids);
  }

  onUiFullyLoaded(): void {
    this.handler.onUiFullyLoaded();
  }

  convertTenorGifsToEmoji(gifs: TenorGifResponse): EmojiVariants[] {
    return gifs.results.map(({
                              id,
                              url,
                              previewSize,
                              contentDescription,
                            }) => ({
                              base: {
                                visualContent: {
                                  id,
                                  url,
                                  previewSize,
                                },
                                name: contentDescription,
                              },
                              alternates: [],
                            }));
  }

  static getInstance(): EmojiPickerApiProxy {
    if (EmojiPickerApiProxyImpl.instance === null) {
      EmojiPickerApiProxyImpl.instance = new EmojiPickerApiProxyImpl();
    }
    return EmojiPickerApiProxyImpl.instance;
  }

  static setInstance(instance: EmojiPickerApiProxy): void {
    EmojiPickerApiProxyImpl.instance = instance;
  }
}
