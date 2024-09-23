// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrDrawerElement} from 'chrome://resources/ash/common/cr_elements/cr_drawer/cr_drawer.js';
import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

import {SelectorItem} from './navigation_selector.js';

interface NavigationViewPanelElement extends LegacyElementMixin, HTMLElement {
  selectedItem: SelectorItem;
  selectorItems_: SelectorItem[];
  title: string;
  showBanner: boolean;
  showToolBar: boolean;
  showNav: boolean;
  createSelectorItem(
      name: string, pageIs: string, icon: string, id?: string,
      initialData?: object): SelectorItem;
  setDefaultPage_(): void;
  addSelectors(pages: SelectorItem[]): void;
  addSelector(
      name: string, pageIs: string, icon: string, id?: string,
      initialData?: object): void;
  addSelectorItem(selectorItem: SelectorItem): void;
  removeSelectorById(id: string): void;
  selectedItemChanged_(): void;
  notifyEvent(functionName: string, params?: object): void;
  selectPageById(id: string): void;
  getPage_(item: SelectorItem): void;
  showPage_(pageComponent: HTMLElement): void;
  onMenuButtonTap_(): void;
  onScroll_(): void;
  pageExists(selectorId: string): boolean;

  $: {
    drawer: CrDrawerElement,
  };
}

export {NavigationViewPanelElement};

declare global {
  interface HTMLElementTagNameMap {
    'navigation-view-panel': NavigationViewPanelElement;
  }
}
