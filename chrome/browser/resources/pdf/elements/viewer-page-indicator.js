// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {isRTL} from 'chrome://resources/js/util.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'viewer-page-indicator',

  _template: html`{__html_template__}`,

  properties: {
    label: {type: String, value: '1'},

    index: {type: Number, observer: 'indexChanged'},

    pageLabels: {type: Array, value: null, observer: 'pageLabelsChanged'}
  },

  /** @type {number|undefined} */
  timerId: undefined,

  /** @override */
  ready: function() {
    const callback = this.fadeIn_.bind(this);
    window.addEventListener('scroll', function() {
      requestAnimationFrame(callback);
    });
  },

  /** @private */
  fadeIn_: function() {
    const percent = window.scrollY /
        (document.scrollingElement.scrollHeight -
         document.documentElement.clientHeight);
    this.style.top =
        percent * (document.documentElement.clientHeight - this.offsetHeight) +
        'px';
    // <if expr="is_macosx">
    // If overlay scrollbars are enabled, prevent them from overlapping the
    // triangle. TODO(dbeam): various platforms can enable overlay scrolling,
    // not just Mac. The scrollbars seem to have different widths/appearances on
    // those platforms, though.
    assert(document.documentElement.dir);
    const endEdge = isRTL() ? 'left' : 'right';
    if (window.innerWidth == document.scrollingElement.scrollWidth) {
      this.style[endEdge] = '16px';
    } else {
      this.style[endEdge] = '0px';
    }
    // </if>
    this.style.opacity = 1;
    clearTimeout(this.timerId);

    this.timerId = setTimeout(() => {
      this.style.opacity = 0;
      this.timerId = undefined;
    }, 2000);
  },

  pageLabelsChanged: function() {
    this.indexChanged();
  },

  indexChanged: function() {
    if (this.pageLabels) {
      this.label = this.pageLabels[this.index];
    } else {
      this.label = String(this.index + 1);
    }
  }
});
