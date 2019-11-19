// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for Sidebar, served from chrome://bluetooth-internals/.
 */

cr.define('sidebar', function() {
  /** @typedef {{pageName: string, text: string}} */
  let SidebarItem;

  /** @const {!Object}*/
  const PageManager = cr.ui.pageManager.PageManager;

  /**
   * A side menu that lists the currently navigable pages.
   * @constructor
   * @param {!Element} sidebarDiv The div corresponding to the sidebar.
   * @extends {cr.ui.pageManager.PageManager.Observer}
   */
  function Sidebar(sidebarDiv) {
    /** @private {!Element} */
    this.sidebarDiv_ = sidebarDiv;
    /** @private {!Element} */
    this.sidebarContent_ =
        assert(this.sidebarDiv_.querySelector('.sidebar-content'));
    /** @private {!Element} */
    this.sidebarList_ = assert(this.sidebarContent_.querySelector('ul'));

    this.sidebarList_.querySelectorAll('li button').forEach(function(item) {
      item.addEventListener('click', this.onItemClick_.bind(this));
    }, this);

    /** @private {!Element} */
    this.overlayDiv_ = assert(this.sidebarDiv_.querySelector('.overlay'));
    this.overlayDiv_.addEventListener('click', this.close.bind(this));

    window.matchMedia('screen and (max-width: 600px)')
        .addListener(function(query) {
          if (!query.matches) {
            this.close();
          }
        }.bind(this));
  }

  Sidebar.prototype = {
    __proto__: PageManager.Observer.prototype,

    /**
     * Adds a new list item to the sidebar using the given |item|.
     * @param {!SidebarItem} item
     */
    addItem: function(item) {
      const sidebarItem = document.createElement('li');
      sidebarItem.dataset.pageName = item.pageName.toLowerCase();

      const button = document.createElement('button');
      button.classList.add('custom-appearance');
      button.textContent = item.text;
      button.addEventListener('click', this.onItemClick_.bind(this));
      sidebarItem.appendChild(button);

      this.sidebarList_.appendChild(sidebarItem);
    },

    /**
     * Closes the sidebar. Only applies to layouts with window width <= 600px.
     */
    close: function() {
      this.sidebarDiv_.classList.remove('open');
      document.body.style.overflow = '';
      document.dispatchEvent(new CustomEvent('contentfocus'));
    },

    /**
     * Opens the sidebar. Only applies to layouts with window width <= 600px.
     */
    open: function() {
      document.body.style.overflow = 'hidden';
      this.sidebarDiv_.classList.add('open');
      document.dispatchEvent(new CustomEvent('contentblur'));
    },

    /**
     * Removes a sidebar item where |pageName| matches the item's pageName.
     * @param {string} pageName
     */
    removeItem: function(pageName) {
      pageName = pageName.toLowerCase();
      const query = 'li[data-page-name="' + pageName + '"]';
      this.sidebarList_.removeChild(this.sidebarList_.querySelector(query));
    },

    /**
     * Called when a page is navigated to.
     * @override
     * @param {string} path The path of the page being visited.
     */
    updateHistory: function(path) {
      this.sidebarContent_.querySelectorAll('li').forEach(function(item) {
        item.classList.toggle('selected', item.dataset.pageName === path);
      });
    },

    /**
     * Switches the page based on which sidebar list button was clicked.
     * @param {!Event} event
     * @private
     */
    onItemClick_: function(event) {
      this.close();
      PageManager.showPageByName(event.target.parentNode.dataset.pageName);
    },
  };

  return {Sidebar: Sidebar};
});
