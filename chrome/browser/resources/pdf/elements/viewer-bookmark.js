// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';

import {IronA11yKeysBehavior} from 'chrome://resources/polymer/v3_0/iron-a11y-keys-behavior/iron-a11y-keys-behavior.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * The |title| is the text label displayed for the bookmark.
 *
 * The bookmark may point at a location in the PDF or a URI.
 * If it points at a location, |page| indicates which 0-based page it leads to.
 * Optionally, |x| is the x position in that page, |y| is the y position in that
 * page, in pixel coordinates and |zoom| is the new zoom value. If it points at
 * an URI, |uri| is the target for that bookmark.
 *
 * |children| is an array of the |Bookmark|s that are below this in a table of
 * contents tree
 * structure.
 * @typedef {{
 *   title: string,
 *   page: (number | undefined),
 *   x: (number | undefined),
 *   y: (number | undefined),
 *   zoom: (number | undefined),
 *   uri: (string | undefined),
 *   children: !Array<!Bookmark>
 * }}
 */
export let Bookmark;

/** Amount that each level of bookmarks is indented by (px). */
const BOOKMARK_INDENT = 20;

Polymer({
  is: 'viewer-bookmark',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {Bookmark} */
    bookmark: {
      type: Object,
      observer: 'bookmarkChanged_',
    },

    depth: {
      type: Number,
      observer: 'depthChanged_'
    },

    /** @private */
    childDepth_: Number,

    /** @private */
    childrenShown_: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
    },

    /** @type {?EventTarget} The target for the key bindings below. */
    keyEventTarget: Object,
  },

  behaviors: [IronA11yKeysBehavior],

  keyBindings: {'enter': 'onEnter_', 'space': 'onSpace_'},

  /** @override */
  attached: function() {
    this.keyEventTarget = this.$.item;
  },

  /** @private */
  bookmarkChanged_: function() {
    this.$.expand.style.visibility =
        this.bookmark.children.length > 0 ? 'visible' : 'hidden';
  },

  /** @private */
  depthChanged_: function() {
    this.childDepth_ = this.depth + 1;
    this.$.item.style.paddingInlineStart =
        (this.depth * BOOKMARK_INDENT) + 'px';
  },

  /** @private */
  onClick_: function() {
    if (this.bookmark.page != null) {
      if (this.bookmark.zoom != null) {
        this.fire('change-zoom', {zoom: this.bookmark.zoom});
      }
      if (this.bookmark.x != null &&
          this.bookmark.y != null) {
        this.fire('change-page-and-xy', {
          page: this.bookmark.page,
          x: this.bookmark.x,
          y: this.bookmark.y,
          origin: 'bookmark'
        });
      } else {
        this.fire(
            'change-page', {page: this.bookmark.page, origin: 'bookmark'});
      }
    } else if (this.bookmark.uri != null) {
      this.fire('navigate', {uri: this.bookmark.uri, newtab: true});
    }
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onEnter_: function(e) {
    // Don't allow events which have propagated up from the expand button to
    // trigger a click.
    if (e.detail.keyboardEvent.target != this.$.expand) {
      this.onClick_();
    }
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onSpace_: function(e) {
    // cr-icon-button stops propagation of space events, so there's no need
    // to check the event source here.
    this.onClick_();
    // Prevent default space scroll behavior.
    e.detail.keyboardEvent.preventDefault();
  },

  /**
   * @param {!Event} e
   * @private
   */
  toggleChildren_: function(e) {
    this.childrenShown_ = !this.childrenShown_;
    e.stopPropagation();  // Prevent the above onClick_ handler from firing.
  }
});
