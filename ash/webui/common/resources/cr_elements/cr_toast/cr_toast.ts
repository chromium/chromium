// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview A lightweight toast.
 *
 * Forked from ui/webui/resources/cr_elements/cr_toast/cr_toast.ts
 */
import '../cr_shared_vars.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_toast.html.js';

export interface CrToastElement {
  _setOpen(open: boolean): void;
}

export class CrToastElement extends PolymerElement {
  static get is() {
    return 'cr-toast';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      duration: {
        type: Number,
        value: 0,
      },

      open: {
        readOnly: true,
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  duration: number;
  open: boolean;
  private hideTimeoutId_: number|null = null;

  static get observers() {
    return ['resetAutoHide_(duration, open)'];
  }

  /**
   * Cancels existing auto-hide, and sets up new auto-hide.
   */
  private resetAutoHide_() {
    if (this.hideTimeoutId_ !== null) {
      window.clearTimeout(this.hideTimeoutId_);
      this.hideTimeoutId_ = null;
    }

    if (this.open && this.duration !== 0) {
      this.hideTimeoutId_ = window.setTimeout(() => {
        this.hide();
      }, this.duration);
    }
  }

  /**
   * Shows the toast and auto-hides after |this.duration| milliseconds has
   * passed. If the toast is currently being shown, any preexisting auto-hide
   * is cancelled and replaced with a new auto-hide.
   */
  show() {
    // Force autohide to reset if calling show on an already shown toast.
    const shouldResetAutohide = this.open;

    // The role attribute is removed first so that screen readers to better
    // ensure that screen readers will read out the content inside the toast.
    // If the role is not removed and re-added back in, certain screen readers
    // do not read out the contents, especially if the text remains exactly
    // the same as a previous toast.
    this.removeAttribute('role');

    // Reset the aria-hidden attribute as screen readers need to access the
    // contents of an opened toast.
    this.removeAttribute('aria-hidden');

    this._setOpen(true);
    this.setAttribute('role', 'alert');

    if (shouldResetAutohide) {
      this.resetAutoHide_();
    }
  }

  /**
   * Hides the toast and ensures that screen readers cannot its contents while
   * hidden.
   */
  hide() {
    this.setAttribute('aria-hidden', 'true');
    this._setOpen(false);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-toast': CrToastElement;
  }
}

customElements.define(CrToastElement.is, CrToastElement);
