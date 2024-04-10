// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-checkbox' is a component similar to native checkbox.
 *
 * Forked from ui/webui/resources/cr_elements/cr_checkbox/cr_checkbox.ts
 *
 * Fires a 'change' event *only* when its state changes as a result of a user
 * interaction. By default it assumes there will be child(ren) passed in to be
 * used as labels. If no label will be provided, a .no-label class should be
 * added to hide the spacing between the checkbox and the label container.
 *
 * If a label is provided, it will be shown by default after the checkbox. A
 * .label-first CSS class can be added to show the label before the checkbox.
 *
 * List of customizable styles:
 *  --cr-checkbox-border-size
 *  --cr-checkbox-checked-box-background-color
 *  --cr-checkbox-checked-box-color
 *  --cr-checkbox-label-color
 *  --cr-checkbox-label-padding-start
 *  --cr-checkbox-mark-color
 *  --cr-checkbox-ripple-checked-color
 *  --cr-checkbox-ripple-size
 *  --cr-checkbox-ripple-unchecked-color
 *  --cr-checkbox-size
 *  --cr-checkbox-unchecked-box-color
 */
import '../cr_shared_vars.css.js';

import {PaperRippleMixin} from '//resources/polymer/v3_0/paper-behaviors/paper-ripple-mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_checkbox.html.js';

const CrCheckboxElementBase = PaperRippleMixin(PolymerElement);

export interface CrCheckboxElement {
  $: {
    checkbox: HTMLElement,
  };
}

export class CrCheckboxElement extends CrCheckboxElementBase {
  static get is() {
    return 'cr-checkbox';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      checked: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        observer: 'checkedChanged_',
        notify: true,
      },

      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        observer: 'disabledChanged_',
      },

      ariaDescription: String,

      ariaLabelOverride: String,

      tabIndex: {
        type: Number,
        value: 0,
        observer: 'onTabIndexChanged_',
      },
    };
  }

  checked: boolean;
  disabled: boolean;
  override ariaDescription: string|null;
  ariaLabelOverride: string;
  override tabIndex: number;

  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  override _rippleContainer: Element;

  override ready() {
    super.ready();

    // TODO(b/309689294) Remove this once CrOS UIs migrate to Jellybean
    // components and no longer use cr-elements.
    // Force stamp the ripple element to enable CrOS focus styles. Ripple
    // visibility is controlled by the event listeners below.
    if (document.documentElement.hasAttribute('chrome-refresh-2023')) {
      this.getRipple();
    }

    this.removeAttribute('unresolved');
    this.addEventListener('click', this.onClick_.bind(this));
    this.addEventListener('pointerup', this.hideRipple_.bind(this));
    if (document.documentElement.hasAttribute('chrome-refresh-2023')) {
      this.addEventListener('pointerdown', this.showRipple_.bind(this));
      this.addEventListener('pointerleave', this.hideRipple_.bind(this));
    } else {
      this.addEventListener('blur', this.hideRipple_.bind(this));
      this.addEventListener('focus', this.showRipple_.bind(this));
    }
  }

  override focus() {
    this.$.checkbox.focus();
  }

  getFocusableElement(): HTMLElement {
    return this.$.checkbox;
  }

  private checkedChanged_() {
    this.$.checkbox.setAttribute(
        'aria-checked', this.checked ? 'true' : 'false');
  }

  private disabledChanged_(_current: boolean, previous: boolean) {
    if (previous === undefined && !this.disabled) {
      return;
    }

    this.tabIndex = this.disabled ? -1 : 0;
    this.$.checkbox.setAttribute(
        'aria-disabled', this.disabled ? 'true' : 'false');
  }

  private showRipple_() {
    if (this.noink) {
      return;
    }

    this.getRipple().showAndHoldDown();
  }

  private hideRipple_() {
    this.getRipple().clear();
  }

  private onClick_(e: Event) {
    if (this.disabled || (e.target as HTMLElement).tagName === 'A') {
      return;
    }

    // Prevent |click| event from bubbling. It can cause parents of this
    // elements to erroneously re-toggle this control.
    e.stopPropagation();
    e.preventDefault();

    this.checked = !this.checked;
    this.dispatchEvent(new CustomEvent(
        'change', {bubbles: true, composed: true, detail: this.checked}));
  }

  private onKeyDown_(e: KeyboardEvent) {
    if (e.key !== ' ' && e.key !== 'Enter') {
      return;
    }

    e.preventDefault();
    e.stopPropagation();
    if (e.repeat) {
      return;
    }

    if (e.key === 'Enter') {
      this.click();
    }
  }

  private onKeyUp_(e: KeyboardEvent) {
    if (e.key === ' ' || e.key === 'Enter') {
      e.preventDefault();
      e.stopPropagation();
    }

    if (e.key === ' ') {
      this.click();
    }
  }

  private onTabIndexChanged_() {
    // :host shouldn't have a tabindex because it's set on #checkbox.
    this.removeAttribute('tabindex');
  }

  // Overridden from PaperRippleMixin
  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  override _createRipple() {
    this._rippleContainer = this.$.checkbox;
    const ripple = super._createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle');
    return ripple;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-checkbox': CrCheckboxElement;
  }
}

customElements.define(CrCheckboxElement.is, CrCheckboxElement);
