// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Number of pixels required to move to consider the pointermove event as
 * intentional.
 */
export const MOVE_THRESHOLD_PX: number = 5;

/**
 * @fileoverview 'cr-toggle' is a component for showing an on/off switch. It
 * fires a 'change' event *only* when its state changes as a result of a user
 * interaction. Besides just clicking the element, its state can be changed by
 * dragging (pointerdown+pointermove) the element towards the desired direction.
 *
 * Forked from ui/webui/resources/cr_elements/cr_toggle/cr_toggle.ts
 */
import {PaperRippleMixin} from '//resources/polymer/v3_0/paper-behaviors/paper-ripple-mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assert} from '//resources/js/assert.js';
import '../cr_shared_vars.css.js';
import {getTemplate} from './cr_toggle.html.js';

const CrToggleElementBase = PaperRippleMixin(PolymerElement);

export interface CrToggleElement {
  $: {
    knob: HTMLElement,
  };
}

export class CrToggleElement extends CrToggleElementBase {
  static get is() {
    return 'cr-toggle';
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

      dark: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        observer: 'disabledChanged_',
      },
    };
  }

  checked: boolean;
  dark: boolean;
  disabled: boolean;

  private boundPointerMove_: ((e: PointerEvent) => void)|null = null;
  /**
   * Whether the state of the toggle has already taken into account by
   * |pointeremove| handlers. Used in the 'click' handler.
   */
  private handledInPointerMove_: boolean = false;
  private pointerDownX_: number = 0;

  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  override _rippleContainer: Element;

  override ready() {
    super.ready();
    if (!this.hasAttribute('role')) {
      this.setAttribute('role', 'button');
    }
    if (!this.hasAttribute('tabindex')) {
      this.setAttribute('tabindex', '0');
    }
    this.setAttribute('aria-pressed', this.checked ? 'true' : 'false');
    this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');

    if (!document.documentElement.hasAttribute('chrome-refresh-2023')) {
      this.addEventListener('blur', this.hideRipple_.bind(this));
      this.addEventListener('focus', this.onFocus_.bind(this));
    }

    this.addEventListener('click', this.onClick_.bind(this));
    this.addEventListener('keydown', this.onKeyDown_.bind(this));
    this.addEventListener('keyup', this.onKeyUp_.bind(this));
    this.addEventListener('pointerdown', this.onPointerDown_.bind(this));
    this.addEventListener('pointerup', this.onPointerUp_.bind(this));
  }

  override connectedCallback() {
    super.connectedCallback();

    const direction =
        this.matches(':host-context([dir=rtl]) cr-toggle') ? -1 : 1;
    this.boundPointerMove_ = (e: PointerEvent) => {
      // Prevent unwanted text selection to occur while moving the pointer, this
      // is important.
      e.preventDefault();

      const diff = e.clientX - this.pointerDownX_;
      if (Math.abs(diff) < MOVE_THRESHOLD_PX) {
        return;
      }

      this.handledInPointerMove_ = true;

      const shouldToggle = (diff * direction < 0 && this.checked) ||
          (diff * direction > 0 && !this.checked);
      if (shouldToggle) {
        this.toggleState_(/* fromKeyboard= */ false);
      }
    };
  }

  private checkedChanged_() {
    this.setAttribute('aria-pressed', this.checked ? 'true' : 'false');
  }

  private disabledChanged_() {
    this.setAttribute('tabindex', this.disabled ? '-1' : '0');
    this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
  }

  private onFocus_() {
    this.getRipple().showAndHoldDown();
  }

  private hideRipple_() {
    this.getRipple().clear();
  }

  private onPointerUp_() {
    assert(this.boundPointerMove_);
    this.removeEventListener('pointermove', this.boundPointerMove_);
    this.hideRipple_();
  }

  private onPointerDown_(e: PointerEvent) {
    // Don't do anything if this was not a primary button click or touch event.
    if (e.button !== 0) {
      return;
    }

    // This is necessary to have follow up pointer events fire on |this|, even
    // if they occur outside of its bounds.
    this.setPointerCapture(e.pointerId);
    this.pointerDownX_ = e.clientX;
    this.handledInPointerMove_ = false;
    assert(this.boundPointerMove_);
    this.addEventListener('pointermove', this.boundPointerMove_);
  }

  private onClick_(e: Event) {
    // Prevent |click| event from bubbling. It can cause parents of this
    // elements to erroneously re-toggle this control.
    e.stopPropagation();
    e.preventDefault();

    // User gesture has already been taken care of inside |pointermove|
    // handlers, Do nothing here.
    if (this.handledInPointerMove_) {
      return;
    }

    // If no pointermove event fired, then user just clicked on the
    // toggle button and therefore it should be toggled.
    this.toggleState_(/* fromKeyboard= */ false);
  }

  private toggleState_(fromKeyboard: boolean) {
    // Ignore cases where the 'click' or 'keypress' handlers are triggered while
    // disabled.
    if (this.disabled) {
      return;
    }

    if (!fromKeyboard) {
      this.hideRipple_();
    }

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
      this.toggleState_(/* fromKeyboard= */ true);
    }
  }

  private onKeyUp_(e: KeyboardEvent) {
    if (e.key !== ' ' && e.key !== 'Enter') {
      return;
    }

    e.preventDefault();
    e.stopPropagation();

    if (e.key === ' ') {
      this.toggleState_(/* fromKeyboard= */ true);
    }
  }

  // Overridden from PaperRippleMixin
  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  override _createRipple() {
    this._rippleContainer = this.$.knob;
    const ripple = super._createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle');
    return ripple;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-toggle': CrToggleElement;
  }
}

customElements.define(CrToggleElement.is, CrToggleElement);
