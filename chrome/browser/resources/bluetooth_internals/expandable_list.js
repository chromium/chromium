// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for ExpandableList and ExpandableListItem, served from
 *     chrome://bluetooth-internals/.
 */

cr.define('expandable_list', function() {
  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;

  /**
   * A list item that has expandable content that toggles when the item is
   * clicked.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  const ExpandableListItem = cr.ui.define('li');

  ExpandableListItem.prototype = {
    __proto__: ListItem.prototype,

    /**
     * Decorates the element as an expandable list item and caches the created
     * content holders for implementations.
     * @override
     */
    decorate: function() {
      this.classList.add('expandable-list-item');
      this.briefContent_ = document.createElement('div');
      this.briefContent_.classList.add('brief-content');
      this.briefContent_.addEventListener('click', this.onExpand_.bind(this));
      this.appendChild(this.briefContent_);

      this.expandedContent_ = document.createElement('div');
      this.expandedContent_.classList.add('expanded-content');
      this.appendChild(this.expandedContent_);
    },

    /**
     * Called when the list item is expanded or collapsed.
     * @param {boolean} expanded
     */
    onExpandInternal: function(expanded) {},

    /**
     * Toggles the expanded class on the item.
     * @private
     */
    onExpand_: function() {
      this.onExpandInternal(this.classList.toggle('expanded'));
    },
  };

  /**
   * A list that contains expandable list items.
   * @constructor
   * @extends {cr.ui.List}
   */
  const ExpandableList = cr.ui.define('list');

  ExpandableList.prototype = {
    __proto__: List.prototype,

    /**
     * Decorates element as an expandable list and caches references to layout
     * elements.
     * @override
     */
    decorate: function() {
      List.prototype.decorate.call(this);
      this.classList.add('expandable-list');

      this.emptyMessage_ = document.createElement('h3');
      this.emptyMessage_.classList.add('empty-message');
      this.emptyMessage_.hidden = true;
      this.insertBefore(this.emptyMessage_, this.firstChild);

      this.spinner_ = document.createElement('div');
      this.spinner_.classList.add('spinner');
      this.insertBefore(this.spinner_, this.firstChild);

      this.autoExpands = true;
      this.boundUpdateMessage_ = this.updateMessageDisplay_.bind(this);
      this.setSpinnerShowing(true);
    },

    /**
     * Sets the data model of the list.
     * @param {cr.ui.ArrayDataModel} data
     */
    setData: function(data) {
      if (this.dataModel) {
        this.dataModel.removeEventListener('splice', this.boundUpdateMessage_);
      }

      this.dataModel = data;
      this.dataModel.addEventListener('splice', this.boundUpdateMessage_);
      this.updateMessageDisplay_();
    },

    /**
     * Sets the empty message text.
     * @param {string} message
     */
    setEmptyMessage: function(message) {
      this.emptyMessage_.textContent = message;
    },

    /**
     * Sets the spinner display state. If |showing| is true, the loading
     * spinner is dispayed.
     * @param {boolean} showing
     */
    setSpinnerShowing: function(showing) {
      this.spinner_.hidden = !showing;
    },

    /**
     * Gets the spinner display state. Returns true if the spinner is showing.
     * @return {boolean}
     */
    isSpinnerShowing: function() {
      return !this.spinner_.hidden;
    },

    /**
     * Updates the display state of the empty message. If there are no items in
     * the data model, the empty message is displayed.
     */
    updateMessageDisplay_: function() {
      this.emptyMessage_.hidden = this.dataModel.length > 0;
    },
  };

  return {
    ExpandableListItem: ExpandableListItem,
    ExpandableList: ExpandableList,
  };
});
