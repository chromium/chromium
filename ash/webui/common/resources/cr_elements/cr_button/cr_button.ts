// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-button' is a button which displays slotted elements. It can
 * be interacted with like a normal button using click as well as space and
 * enter to effectively click the button and fire a 'click' event. It can also
 * style an icon inside of the button with the [has-icon] attribute.
 *
 * Forked from ui/webui/resources/cr_elements/cr_button/cr_button.ts
 */
import '../cr_hidden_style.css.js';
import '../cr_shared_vars.css.js';

import {FocusOutlineManager} from '//resources/js/focus_outline_manager.js';
import {PaperRippleMixin} from '//resources/polymer/v3_0/paper-behaviors/paper-ripple-mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_button.html.js';

export interface CrButtonElement {
  $: {
    prefixIcon: HTMLSlotElement,
    suffixIcon: HTMLSlotElement,
  };
}

const CrButtonElementBase = PaperRippleMixin(PolymerElement);

export class CrButtonElement extends CrButtonElementBase {
  static get is() {
    return 'cr-button';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        observer: 'disabledChanged_',
      },

      /**
       * Use this property in order to configure the "tabindex" attribute.
       */
      customTabIndex: {
        type: Number,
        observer: 'applyTabIndex_',
      },

      /**
       * Flag used for formatting ripples on circle shaped cr-buttons.
       * @private
       */
      circleRipple: {
        type: Boolean,
        value: false,
      },

      hasPrefixIcon_: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      hasSuffixIcon_: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
    };
  }

  disabled: boolean;
  customTabIndex: number;
  circleRipple: boolean;
  private hasPrefixIcon_: boolean;
  private hasSuffixIcon_: boolean;

  /**
   * It is possible to activate a tab when the space key is pressed down. When
   * this element has focus, the keyup event for the space key should not
   * perform a 'click'. |spaceKeyDown_| tracks when a space pressed and
   * handled by this element. Space keyup will only result in a 'click' when
   * |spaceKeyDown_| is true. |spaceKeyDown_| is set to false when element
   * loses focus.
   */
  private spaceKeyDown_: boolean = false;
  private timeoutIds_: Set<number> = new Set();

  constructor() {
    super();

    this.addEventListener('blur', this.onBlur_.bind(this));
    // Must be added in constructor so that stopImmediatePropagation() works as
    // expected.
    this.addEventListener('click', this.onClick_.bind(this));
    this.addEventListener('keydown', this.onKeyDown_.bind(this));
    this.addEventListener('keyup', this.onKeyUp_.bind(this));
    this.addEventListener('pointerdown', this.onPointerDown_.bind(this));
  }

  override ready() {
    super.ready();
    if (!this.hasAttribute('role')) {
      this.setAttribute('role', 'button');
    }
    if (!this.hasAttribute('tabindex')) {
      this.setAttribute('tabindex', '0');
    }
    if (!this.hasAttribute('aria-disabled')) {
      this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
    }

    FocusOutlineManager.forDocument(document);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.timeoutIds_.forEach(clearTimeout);
    this.timeoutIds_.clear();
  }

  private setTimeout_(fn: () => void, delay?: number) {
    if (!this.isConnected) {
      return;
    }
    const id = setTimeout(() => {
      this.timeoutIds_.delete(id);
      fn();
    }, delay);
    this.timeoutIds_.add(id);
  }

  private disabledChanged_(newValue: boolean, oldValue: boolean|undefined) {
    if (!newValue && oldValue === undefined) {
      return;
    }
    if (this.disabled) {
      this.blur();
    }
    this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
    this.applyTabIndex_();
  }

  /**
   * Updates the tabindex HTML attribute to the actual value.
   */
  private applyTabIndex_() {
    let value = this.customTabIndex;
    if (value === undefined) {
      value = this.disabled ? -1 : 0;
    }
    this.setAttribute('tabindex', value.toString());
  }

  private onBlur_() {
    this.spaceKeyDown_ = false;
    // If a keyup event is never fired (e.g. after keydown the focus is moved to
    // another element), we need to clear the ripple here. 100ms delay was
    // chosen manually as a good time period for the ripple to be visible.
    this.setTimeout_(() => this.getRipple().uiUpAction(), 100);
  }

  private onClick_(e: Event) {
    if (this.disabled) {
      e.stopImmediatePropagation();
    }
  }

  private onPrefixIconSlotChanged_() {
    this.hasPrefixIcon_ = this.$.prefixIcon.assignedElements().length > 0;
  }

  private onSuffixIconSlotChanged_() {
    this.hasSuffixIcon_ = this.$.suffixIcon.assignedElements().length > 0;
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

    this.getRipple().uiDownAction();
    if (e.key === 'Enter') {
      this.click();
      // Delay was chosen manually as a good time period for the ripple to be
      // visible.
      this.setTimeout_(() => this.getRipple().uiUpAction(), 100);
    } else if (e.key === ' ') {
      this.spaceKeyDown_ = true;
    }
  }

  private onKeyUp_(e: KeyboardEvent) {
    if (e.key !== ' ' && e.key !== 'Enter') {
      return;
    }

    e.preventDefault();
    e.stopPropagation();

    if (this.spaceKeyDown_ && e.key === ' ') {
      this.spaceKeyDown_ = false;
      this.click();
      this.getRipple().uiUpAction();
    }
  }

  private onPointerDown_() {
    this.ensureRipple();
  }

  /**
   * Customize the element's ripple. Overriding the '_createRipple' function
   * from PaperRippleMixin.
   */
  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  override _createRipple() {
    const ripple = super._createRipple();

    if (this.circleRipple) {
      ripple.setAttribute('center', '');
      ripple.classList.add('circle');
    }

    return ripple;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-button': CrButtonElement;
  }
}

customElements.define(CrButtonElement.is, CrButtonElement);
