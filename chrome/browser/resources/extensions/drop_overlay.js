// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {DragWrapper} from 'chrome://resources/js/cr/ui/drag_wrapper.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DragAndDropHandler} from './drag_and_drop_handler.js';

Polymer({
  is: 'extensions-drop-overlay',

  _template: html`{__html_template__}`,

  properties: {
    /** @private {boolean} */
    dragEnabled: {
      type: Boolean,
      observer: 'dragEnabledChanged_',
    }
  },

  /** @override */
  created: function() {
    this.hidden = true;
    const dragTarget = document.documentElement;
    this.dragWrapperHandler_ = new DragAndDropHandler(true, dragTarget);
    // TODO(devlin): All these dragTarget listeners leak (they aren't removed
    // when the element is). This only matters in tests at the moment, but would
    // be good to fix.
    dragTarget.addEventListener('extension-drag-started', () => {
      this.hidden = false;
    });
    dragTarget.addEventListener('extension-drag-ended', () => {
      this.hidden = true;
    });
    dragTarget.addEventListener('drag-and-drop-load-error', (e) => {
      this.fire('load-error', e.detail);
    });
    this.dragWrapper_ = new DragWrapper(dragTarget, this.dragWrapperHandler_);
  },

  /**
   * @param {boolean} dragEnabled
   * @private
   */
  dragEnabledChanged_: function(dragEnabled) {
    this.dragWrapperHandler_.dragEnabled = dragEnabled;
  },
});
