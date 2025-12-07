// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for Sidebar, served from chrome://bluetooth-internals/.
 */

import {assert} from 'chrome://resources/js/assert.js';

import {PageManager} from './page_manager.js';
import type {PageManagerObserver} from './page_manager.js';

interface SidebarItem {
  pageName: string;
  text: string;
}

/**
 * A side menu that lists the currently navigable pages.
 */
export class Sidebar implements PageManagerObserver {
  private sidebarDiv_: HTMLElement;
  private sidebarContent_: HTMLElement;
  private sidebarList_: HTMLElement;
  private overlayDiv_: HTMLElement;

  /** @param sidebarDiv The div corresponding to the sidebar. */
  constructor(sidebarDiv: HTMLElement) {
    this.sidebarDiv_ = sidebarDiv;
    this.sidebarContent_ =
        this.sidebarDiv_.querySelector<HTMLElement>('.sidebar-content')!;
    assert(this.sidebarContent_);

    this.sidebarList_ = this.sidebarContent_.querySelector<HTMLElement>('ul')!;
    assert(this.sidebarList_);

    this.sidebarList_.querySelectorAll('li button').forEach((item) => {
      item.addEventListener('click', this.onItemClick_.bind(this));
    });

    this.overlayDiv_ = this.sidebarDiv_.querySelector<HTMLElement>('.overlay')!;
    assert(this.overlayDiv_);
    this.overlayDiv_.addEventListener('click', this.close.bind(this));

    window.matchMedia('screen and (max-width: 600px)').addListener((query) => {
      if (!query.matches) {
        this.close();
      }
    });
  }

  /**
   * Adds a new list item to the sidebar using the given |item|.
   */
  addItem(item: SidebarItem) {
    const sidebarItem = document.createElement('li');
    sidebarItem.dataset['pageName'] = item.pageName.toLowerCase();

    const button = document.createElement('button');
    button.classList.add('custom-appearance');
    button.textContent = item.text;
    button.addEventListener('click', this.onItemClick_.bind(this));
    sidebarItem.appendChild(button);

    this.sidebarList_.appendChild(sidebarItem);
  }

  /**
   * Closes the sidebar. Only applies to layouts with window width <= 600px.
   */
  close() {
    this.sidebarDiv_.classList.remove('open');
    document.body.style.overflow = '';
    document.dispatchEvent(new CustomEvent('contentfocus'));
  }

  /**
   * Opens the sidebar. Only applies to layouts with window width <= 600px.
   */
  open() {
    document.body.style.overflow = 'hidden';
    this.sidebarDiv_.classList.add('open');
    document.dispatchEvent(new CustomEvent('contentblur'));
  }

  /**
   * Removes a sidebar item where |pageName| matches the item's pageName.
   */
  removeItem(pageName: string) {
    pageName = pageName.toLowerCase();
    const query = 'li[data-page-name="' + pageName + '"]';
    const selection = this.sidebarList_.querySelector(query);

    // Devices are only added to the sidebar when the user pressed "Inspect" on
    // them in the main table. Only try to remove the element if it exists.
    if (selection) {
      this.sidebarList_.removeChild(selection);
    }
  }

  /**
   * Called when a page is navigated to.
   * @param path The path of the page being visited.
   */
  updateHistory(path: string) {
    this.sidebarContent_.querySelectorAll<HTMLElement>('li').forEach((item) => {
      item.classList.toggle('selected', item.dataset['pageName'] === path);
    });
  }

  updateTitle(_title: string) {}

  /**
   * Switches the page based on which sidebar list button was clicked.
   */
  private onItemClick_(event: Event) {
    this.close();
    const target = event.target as HTMLElement;
    const parent = target.parentNode as HTMLElement;
    PageManager.getInstance().showPageByName(parent.dataset['pageName']!);
  }
}
