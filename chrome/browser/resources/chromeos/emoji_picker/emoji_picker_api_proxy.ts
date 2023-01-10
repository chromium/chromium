// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {PageHandlerFactory, PageHandlerRemote} from './emoji_picker.mojom-webui.js';
import {EmojiVariants, GifSubcategoryData, TenorGifResults, VisualContent} from './types.js';

/** @interface */
export interface EmojiPickerApiProxy {
  showUi(): void;

  insertEmoji(emoji: string, isVariant: boolean, searchLength: number): void;

  isIncognitoTextField(): Promise<{incognito: boolean}>;

  getFeatureList(): Promise<{featureList: number[]}>;

  getCategories(): Promise<{categories: GifSubcategoryData[]}>;

  getFeaturedGifs(pos?: string): Promise<{featured: TenorGifResults}>;

  searchGifs(query: string, pos?: string): Promise<{gifs: TenorGifResults}>;

  convertTenorGifsToEmoji(gifs: TenorGifResults): EmojiVariants[];
}

// https://developers.google.com/tenor/guides/response-objects-and-errors#category-object
declare interface CategoryObject {
  searchterm: string;  // the search term that corresponds to the category
  path: string;        // the search url to request
  image: string;       // a url to the category's example GIF
  name: string;        // category name
}

// https://developers.google.com/tenor/guides/response-objects-and-errors#media-object
declare interface MediaObject {
  url: string;       // a url to the media source
  dims: number[];    // width and height of the media in pixels
  duration: number;  // the time in seconds for one loop of the content
  size: number;      // size of the file in bytes
}

// https://developers.google.com/tenor/guides/response-objects-and-errors#response-object
declare interface ResponseObject {
  id: string;  // tenor result identifier
  media_formats: {
    gif: MediaObject,
    mediumgif: MediaObject,
  };
  content_description: string;  // a textual description of the content for user
                                // accessibility features
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

  /** @override */
  async getCategories(): Promise<{categories: GifSubcategoryData[]}> {
    const {categories} = await this.handler.getCategories();
    return {
      categories: JSON.parse(categories)
                      .tags.map((tag: CategoryObject) => ({name: tag.name})),
    };
  }

  private formatGifResults = (res: string): TenorGifResults => {
    const gifs = JSON.parse(res);
    return {
      next: gifs.next,
      results: gifs.results.map((response: ResponseObject) => {
        const {gif, mediumgif} = response.media_formats;
        const [width, height] = mediumgif.dims;
        return {
          url: {full: gif.url, preview: mediumgif.url},
          previewDims: {width, height},
          contentDescription: response.content_description,
        };
      }),
    };
  };

  /** @override */
  async getFeaturedGifs(pos?: string): Promise<{featured: TenorGifResults}> {
    const {featured} = await this.handler.getFeaturedGifs(pos || null);
    return {
      featured: this.formatGifResults(featured),
    };
  }

  /** @override */
  async searchGifs(query: string, pos?: string):
      Promise<{gifs: TenorGifResults}> {
    const {gifs} = await this.handler.searchGifs(query, pos || null);
    return {
      gifs: this.formatGifResults(gifs),
    };
  }

  convertTenorGifsToEmoji(gifs: TenorGifResults): EmojiVariants[] {
    return gifs.results.map((visualContent: VisualContent) => ({
                              base: {
                                visualContent,
                                name: visualContent.contentDescription,
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