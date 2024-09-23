// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/icons.html.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {DomRepeatEvent, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './oobe_categories_list.html.js';

/**
 * Data that is passed to the component during initialization.
 */
export interface OobeCategoriesListCategory {
  categoryId: string;
  title: string;
  subtitle: string;
  icon: string;
  selected: boolean;
}

const GENERATE_WEB_VIEW_CSS = (backgroundColor: string, iconColor: string) => {
  return {
    code: `svg {
      background-color: ` +
        backgroundColor + `;
      --oobe-jelly-icon-color: ` +
        iconColor + `;
    }`,
  };
};

export interface OobeCategoriesListData extends
    Array<OobeCategoriesListCategory> {}

const OobeCategoriesListBase = PolymerElement;

export class OobeCategoriesList extends OobeCategoriesListBase {
  static get is() {
    return 'oobe-categories-list' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * List of categories to display.
       */
      categoriesList: {
        type: Array,
        value: [],
        notify: true,
      },
      /**
       * List of selected categories.
       */
      categoriesSelected: {
        type: Array,
        value: [],
      },
      /**
       * Number of selected categories.
       */
      selectedCategoriesCount: {
        type: Number,
        value: 0,
        notify: true,
      },
      /**
       * Number of loaded icons categories.
       */
      loadedIconsCount: {
        type: Number,
        value: 0,
      },
      /**
       * Number of dom repeat rendered items.
       */
      itemRendered: {
        type: Number,
        value: 0,
        observer: 'itemRenderedChanged',
      },
    };
  }

  private categoriesList: OobeCategoriesListData;
  private categoriesSelected: string[];
  private selectedCategoriesCount: number;
  private loadedIconsCount: number;
  private itemRendered: number;

  /**
   * Initialize the list of categories.
   */
  init(categories: OobeCategoriesListData): void {
    this.categoriesList = categories;
    this.categoriesSelected = [];
    this.selectedCategoriesCount = 0;
    this.loadedIconsCount = 0;
    this.itemRendered = 0;
    this.categoriesList.forEach((category) => {
      if (category.selected) {
        this.selectedCategoriesCount++;
        this.categoriesSelected.push(category.categoryId);
      }
    });
  }

  reset(): void {
    this.categoriesList = [];
    this.categoriesSelected = [];
    this.selectedCategoriesCount = 0;
    this.loadedIconsCount = 0;
    this.itemRendered = 0;
  }

  itemRenderedChanged(): void {
    if (this.categoriesList.length !== 0 &&
        this.itemRendered === this.categoriesList.length &&
        this.loadedIconsCount === this.categoriesList.length) {
      this.setWebviewStyle();
      this.markCheckedUseCases();
      this.dispatchEvent(
          new CustomEvent('icons-loaded', {bubbles: true, composed: true}));
    }
  }

  /**
   * Return the list of selected categories.
   */
  getCategoriesSelected(): string[] {
    return this.categoriesSelected;
  }

  setWebviewStyle(): void {
    const iconWebviews =
        this.shadowRoot?.querySelectorAll<chrome.webviewTag.WebView>(
            '.category-icon');
    if (iconWebviews) {
      const BackgroundColor =
          getComputedStyle(document.body)
              .getPropertyValue('--cros-sys-primary_container');
      const iconColor = getComputedStyle(document.body)
                            .getPropertyValue('--cros-sys-primary');
      for (const iconWebview of iconWebviews) {
        this.injectCss(iconWebview, BackgroundColor, iconColor);
      }
    }
  }

  private markCheckedUseCases(): void {
    this.categoriesList.forEach((category) => {
      if (category.selected) {
        const element = this.shadowRoot?.querySelector(
            `#${this.getCategoryId(category.categoryId)}`);
        if (element) {
          element.setAttribute('checked', 'true');
        }
      }
    });
  }

  private getIconUrl(iconUrl: string): string {
    return iconUrl;
  }

  private onClick(e: DomRepeatEvent<OobeCategoriesListCategory, MouseEvent>):
      void {
    const clickedCategory = e.model.item;
    const previousSelectedState = clickedCategory.selected;
    const currentSelectedState = !previousSelectedState;
    const path = `categoriesList.${
        this.categoriesList.indexOf(clickedCategory)}.selected`;
    this.set(path, currentSelectedState);
    (e.currentTarget as HTMLElement)
        ?.setAttribute('checked', String(currentSelectedState));

    if (currentSelectedState) {
      this.selectedCategoriesCount++;
      this.categoriesSelected.push(clickedCategory.categoryId);
    } else {
      this.selectedCategoriesCount--;
      this.categoriesSelected.splice(
          this.categoriesSelected.indexOf(clickedCategory.categoryId), 1);
    }
    this.notifyPath('categoriesList');
  }

  private getCategoryId(categoryId: string): string {
    return 'cr-button-' + categoryId;
  }

  private getWebViewId(categoryId: string): string {
    return 'webview-' + categoryId;
  }

  private onIconLoaded(): void {
    this.loadedIconsCount += 1;
  }

  private injectCss(
      webview: chrome.webviewTag.WebView, backgroundColor: string,
      iconColor: string) {
    webview.addEventListener('contentload', () => {
      webview.insertCSS(
          GENERATE_WEB_VIEW_CSS(backgroundColor, iconColor), () => {
            if (chrome.runtime.lastError) {
              console.warn(
                  'Failed to insertCSS: ' + chrome.runtime.lastError.message);
            }
          });
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OobeCategoriesList.is]: OobeCategoriesList;
  }
}

customElements.define(OobeCategoriesList.is, OobeCategoriesList);
