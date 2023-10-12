// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for Sidebar, served from chrome://bluetooth-internals/.
 */

import {assert} from 'chrome://resources/js/assert.js';

import {PageManager, PageManagerObserver} from './page_manager.js';

/** @typedef {{pageName: string, text: string}} */
let SidebarItem;

/**
 * A side menu that lists the currently navigable pages.
 */
export class Sidebar extends PageManagerObserver {
  /** @param {!Element} sidebarDiv The div corresponding to the sidebar. */
  constructor(sidebarDiv) {
    super();
    /** @private {!Element} */
    this.sidebarDiv_ = sidebarDiv;
    /** @private {!Element} */
    this.sidebarContent_ = this.sidebarDiv_.querySelector('.sidebar-content');
    assert(this.sidebarContent_);

    /** @private {!Element} */
    this.sidebarList_ = this.sidebarContent_.querySelector('ul');
    assert(this.sidebarList_);

    this.sidebarList_.querySelectorAll('li button').forEach(function(item) {
      item.addEventListener('click', this.onItemClick_.bind(this));
    }, this);

    /** @private {!Element} */
    this.overlayDiv_ = this.sidebarDiv_.querySelector('.overlay');
    assert(this.overlayDiv_);
    this.overlayDiv_.addEventListener('click', this.close.bind(this));

    window.matchMedia('screen and (max-width: 600px)')
        .addListener(function(query) {
          if (!query.matches) {
            this.close();
          }
        }.bind(this));
  }

  /**
   * Adds a new list item to the sidebar using the given |item|.
   * @param {!SidebarItem} item
   */
  addItem(item) {
    const sidebarItem = document.createElement('li');
    sidebarItem.dataset.pageName = item.pageName.toLowerCase();

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
   * @param {string} pageName
   */
  removeItem(pageName) {
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
   * @override
   * @param {string} path The path of the page being visited.
   */
  updateHistory(path) {
    this.sidebarContent_.querySelectorAll('li').forEach(function(item) {
      item.classList.toggle('selected', item.dataset.pageName === path);
    });
  }

  /**
   * Switches the page based on which sidebar list button was clicked.
   * @param {!Event} event
   * @private
   */
  onItemClick_(event) {
    this.close();
    PageManager.getInstance().showPageByName(
        event.target.parentNode.dataset.pageName);
  }
}
