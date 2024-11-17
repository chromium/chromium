// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {Category, HistoryItem, PageHandlerFactory, PageHandlerRemote} from './emoji_picker.mojom-webui.js';
import {EmojiSearch} from './emoji_search.mojom-webui.js';
import {NewWindowProxy} from './new_window_proxy.mojom-webui.js';
import {PaginatedGifResponses, Status} from './tenor_types.mojom-webui.js';
import {EmojiVariants, GifSubcategoryData, VisualContent} from './types.js';

const HELP_CENTRE_URL = 'https://support.google.com/chrome?p=palette';

export class EmojiPickerApiProxy {
  private handler = new PageHandlerRemote();
  private newWindowProxy = NewWindowProxy.getRemote();
  // TODO(b/309343774): Once search is always on, remove function wrapper.
  private searchProxy = () => EmojiSearch.getRemote();
  static instance: EmojiPickerApiProxy|null = null;
  constructor() {
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(this.handler.$.bindNewPipeAndPassReceiver());
  }

  showUi() {
    this.handler.showUI();
  }

  insertEmoji(emoji: string, isVariant: boolean, searchLength: number) {
    this.handler.insertEmoji(emoji, isVariant, searchLength);
  }

  insertGif(gif: Url) {
    this.handler.insertGif(gif);
  }

  isIncognitoTextField() {
    return this.handler.isIncognitoTextField();
  }

  getFeatureList() {
    return this.handler.getFeatureList();
  }

  async getCategories(): Promise<{gifCategories: GifSubcategoryData[]}> {
    const {gifCategories} = await this.handler.getCategories();
    return {
      gifCategories: gifCategories.map((category) => ({name: category})),
    };
  }

  getFeaturedGifs(pos?: string):
      Promise<{status: Status, featuredGifs: PaginatedGifResponses}> {
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

  searchGifs(query: string, pos?: string):
      Promise<{status: Status, searchGifs: PaginatedGifResponses}> {
    if (!navigator.onLine) {
      return Promise.resolve({
        status: Status.kNetError,
        searchGifs: {
          next: '',
          results: [],
        },
      });
    }

    // Avoid sending blank queries to the backend.
    if (query.trim().length === 0) {
      return Promise.resolve({
        status: Status.kHttpOk,
        searchGifs: {
          next: '',
          results: [],
        },
      });
    }

    return this.handler.searchGifs(query, pos || null);
  }

  searchEmoji(query: string) {
    // TODO(b/346457889): Add multilingual search for emoji picker.
    // For now assume English.
    return this.searchProxy().searchEmoji(query, ['en']);
  }

  /** @override */
  getGifsByIds(ids: string[]):
      Promise<{status: Status, selectedGifs: VisualContent[]}> {
    return this.handler.getGifsByIds(ids);
  }

  openHelpCentreArticle(): void {
    this.newWindowProxy.openUrl({
      url: HELP_CENTRE_URL,
    });
  }

  getInitialCategory(): Promise<{category: Category}> {
    return this.handler.getInitialCategory();
  }

  getInitialQuery(): Promise<{query: string}> {
    return this.handler.getInitialQuery();
  }

  updateHistoryInPrefs(category: Category, history: HistoryItem[]): void {
    this.handler.updateHistoryInPrefs(category, history);
  }

  updatePreferredVariantsInPrefs(preferredVariants: Record<string, string>):
      void {
    this.handler.updatePreferredVariantsInPrefs(
        Object.keys(preferredVariants).map(base => ({
                                             'base': base,
                                             'variant': preferredVariants[base],
                                           })));
  }

  getHistoryFromPrefs(category: Category): Promise<{history: HistoryItem[]}> {
    return this.handler.getHistoryFromPrefs(category);
  }

  onUiFullyLoaded(): void {
    this.handler.onUiFullyLoaded();
  }

  convertTenorGifsToEmoji(gifs: PaginatedGifResponses): EmojiVariants[] {
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
    if (EmojiPickerApiProxy.instance === null) {
      EmojiPickerApiProxy.instance = new EmojiPickerApiProxy();
    }
    return EmojiPickerApiProxy.instance;
  }

  static setInstance(instance: EmojiPickerApiProxy): void {
    EmojiPickerApiProxy.instance = instance;
  }
}
