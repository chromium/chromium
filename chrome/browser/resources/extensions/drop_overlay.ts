// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {DragWrapper} from 'chrome://resources/js/cr/ui/drag_wrapper.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DragAndDropHandler} from './drag_and_drop_handler.js';

class ExtensionsDropOverlayElement extends PolymerElement {
  static get is() {
    return 'extensions-drop-overlay';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      dragEnabled: {
        type: Boolean,
        observer: 'dragEnabledChanged_',
      }
    };
  }

  private dragWrapperHandler_: DragAndDropHandler;
  private dragWrapper_: DragWrapper;

  constructor() {
    super();

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
      this.dispatchEvent(new CustomEvent(
          'load-error', {bubbles: true, composed: true, detail: e.detail}));
    });
    this.dragWrapper_ = new DragWrapper(dragTarget, this.dragWrapperHandler_);
  }

  private dragEnabledChanged_(dragEnabled: boolean) {
    this.dragWrapperHandler_.dragEnabled = dragEnabled;
  }
}

customElements.define(
    ExtensionsDropOverlayElement.is, ExtensionsDropOverlayElement);
