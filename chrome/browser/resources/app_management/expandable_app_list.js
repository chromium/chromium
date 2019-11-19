// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This is an expanding container for a list of apps that shows some items by
 * default and can be expanded to show more.
 *
 * Note: The implementation assumes children are all the same height.
 *
 * Example usage:
 *  <app-management-expandable-app-list apps="[[appsList]]">
 *    <template is="dom-repeat" items="[[appsList]]" as="app" notify-dom-change>
 *      <app-management-app-item app="[[app]]"></app-management-app-item>
 *    </template>
 *  </app-management-expandable-app-list>
 */
Polymer({
  is: 'app-management-expandable-app-list',

  properties: {
    /**
     * Title of the expandable list.
     * @type {String}
     */
    listTitle: {
      type: String,
      value: '',
      observer: 'onListTitleChanged_',
    },

    /** The number of apps to collapse down to. */
    collapsedSize: {
      type: Number,
      value: NUMBER_OF_APPS_DISPLAYED_DEFAULT,
    },

    /** @private {boolean} */
    listExpanded_: {
      type: Boolean,
      observer: 'onListExpandedChanged_',
    },

    /** @private {number} */
    numChildrenForTesting_: {
      type: Number,
      value: 0,
      notify: true,
    },
  },

  listeners: {
    'dom-change': 'onDomChange_',
  },

  attached: function() {
    // Hide on reattach.
    this.listExpanded_ = false;
    this.$.collapse.hide();
  },

  /** @private */
  onAppsChanged_: function(change) {},

  /** @private */
  onListTitleChanged_() {
    this.$['app-list-title'].hidden = !this.listTitle;
  },

  /** @private */
  onDomChange_: function() {
    let collapsedHeight = 0;
    let numChildren = 0;
    for (const child of this.$.collapse.getContentChildren()) {
      // Wait until we have an actual child element rather than just the
      // dom-repeat.
      if (child.tagName == 'DOM-REPEAT' || child.tagName == 'TEMPLATE') {
        continue;
      }

      if (numChildren < this.collapsedSize) {
        collapsedHeight += child.offsetHeight;
      }

      numChildren++;
    }

    this.style.setProperty(
        '--collapsed-height', String(collapsedHeight) + 'px');
    this.$['expander-row'].hidden = numChildren <= this.collapsedSize;
    this.numChildrenForTesting_ = numChildren;
  },

  /** @private */
  toggleListExpanded_: function() {
    this.listExpanded_ = !this.listExpanded_;
  },

  /** @private */
  onListExpandedChanged_() {
    // TODO(calamity): Hiding should display:none after the animation to prevent
    // tabbing into hidden items.
    const collapse = this.$.collapse;
    // Since iron-collapse does not support a 'min-height' property, we force it
    // to animate to the collapsed height.
    if (this.listExpanded_) {
      // Reset the opened state, or show won't work.
      collapse.hide();
      collapse.show();
    } else {
      // This technically leaves the collapse open.
      collapse.updateSize('var(--collapsed-height)', true);
    }
  },

  /**
   * @param {boolean} listExpanded
   * @return {string}
   * @private
   */
  getCollapsedIcon_: function(listExpanded) {
    return listExpanded ? 'cr:expand-less' : 'cr:expand-more';
  },

  /**
   * @param {number} numApps
   * @param {boolean} listExpanded
   * @return {string}
   * @private
   */
  moreAppsString_: function(numApps, listExpanded) {
    return listExpanded ?
        loadTimeData.getString('lessApps') :
        loadTimeData.getStringF('moreApps', numApps - this.collapsedSize);
  },
});
