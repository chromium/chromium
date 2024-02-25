// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Forked from
// ui/webui/resources/cr_elements/cr_radio_group/cr_radio_group.ts

import '../cr_radio_button/cr_radio_button.js';
import '../cr_shared_vars.css.js';

import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrRadioButtonElement} from '../cr_radio_button/cr_radio_button.js';

import {getTemplate} from './cr_radio_group.html.js';

function isEnabled(radio: HTMLElement): boolean {
  return radio.matches(':not([disabled]):not([hidden])') &&
      radio.style.display !== 'none' && radio.style.visibility !== 'hidden';
}

export class CrRadioGroupElement extends PolymerElement {
  static get is() {
    return 'cr-radio-group';
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
        observer: 'update_',
      },

      selected: {
        type: String,
        notify: true,
        observer: 'update_',
      },

      selectableElements: {
        type: String,
        value: 'cr-radio-button, cr-card-radio-button, controlled-radio-button',
      },

      nestedSelectable: {
        type: Boolean,
        value: false,
        observer: 'populate_',
      },

      selectableRegExp_: {
        value: Object,
        computed: 'computeSelectableRegExp_(selectableElements)',
      },
    };
  }

  disabled: boolean;
  selected: string;
  selectableElements: string;
  nestedSelectable: boolean;
  private selectableRegExp_: RegExp;

  private buttons_: CrRadioButtonElement[]|null = null;
  private buttonEventTracker_: EventTracker = new EventTracker();
  private deltaKeyMap_: Map<string, number>|null = null;
  private isRtl_: boolean = false;
  private populateBound_: (() => void)|null = null;

  override ready() {
    super.ready();
    this.addEventListener(
        'keydown', e => this.onKeyDown_(/** @type {!KeyboardEvent} */ (e)));
    this.addEventListener('click', this.onClick_.bind(this));

    if (!this.hasAttribute('role')) {
      this.setAttribute('role', 'radiogroup');
    }
    this.setAttribute('aria-disabled', 'false');
  }

  override connectedCallback() {
    super.connectedCallback();
    this.isRtl_ = this.matches(':host-context([dir=rtl]) cr-radio-group');
    this.deltaKeyMap_ = new Map([
      ['ArrowDown', 1],
      ['ArrowLeft', this.isRtl_ ? 1 : -1],
      ['ArrowRight', this.isRtl_ ? -1 : 1],
      ['ArrowUp', -1],
      ['PageDown', 1],
      ['PageUp', -1],
    ]);

    this.populateBound_ = () => this.populate_();
    assert(this.populateBound_);
    this.shadowRoot!.querySelector('slot')!.addEventListener(
        'slotchange', this.populateBound_);

    this.populate_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.populateBound_);
    this.shadowRoot!.querySelector('slot')!.removeEventListener(
        'slotchange', this.populateBound_);
    this.buttonEventTracker_.removeAll();
  }

  override focus() {
    if (this.disabled || !this.buttons_) {
      return;
    }

    const radio =
        this.buttons_.find(radio => this.isButtonEnabledAndSelected_(radio));
    if (radio) {
      radio.focus();
    }
  }

  private onKeyDown_(event: KeyboardEvent) {
    if (this.disabled) {
      return;
    }

    if (event.ctrlKey || event.shiftKey || event.metaKey || event.altKey) {
      return;
    }

    const targetElement = event.target as CrRadioButtonElement;
    if (!this.buttons_ || !this.buttons_.includes(targetElement)) {
      return;
    }

    if (event.key === ' ' || event.key === 'Enter') {
      event.preventDefault();
      this.select_(targetElement);
      return;
    }

    const enabledRadios = this.buttons_.filter(isEnabled);
    if (enabledRadios.length === 0) {
      return;
    }

    assert(this.deltaKeyMap_);
    let selectedIndex;
    const max = enabledRadios.length - 1;
    if (event.key === 'Home') {
      selectedIndex = 0;
    } else if (event.key === 'End') {
      selectedIndex = max;
    } else if (this.deltaKeyMap_.has(event.key)) {
      const delta = this.deltaKeyMap_.get(event.key)!;
      // If nothing selected, start from the first radio then add |delta|.
      const lastSelection = enabledRadios.findIndex(radio => radio.checked);
      selectedIndex = Math.max(0, lastSelection) + delta;
      // Wrap the selection, if needed.
      if (selectedIndex > max) {
        selectedIndex = 0;
      } else if (selectedIndex < 0) {
        selectedIndex = max;
      }
    } else {
      return;
    }

    const radio = enabledRadios[selectedIndex]!;
    const name = `${radio.name}`;
    if (this.selected !== name) {
      event.preventDefault();
      event.stopPropagation();
      this.selected = name;
      radio.focus();
    }
  }

  private computeSelectableRegExp_(): RegExp {
    const tags = this.selectableElements.split(', ').join('|');
    return new RegExp(`^(${tags})$`, 'i');
  }

  private onClick_(event: Event) {
    const path = event.composedPath();
    if (path.some(target => /^a$/i.test((target as HTMLElement).tagName))) {
      return;
    }
    const target =
        path.find(
            n => this.selectableRegExp_.test((n as HTMLElement).tagName)) as
        CrRadioButtonElement;
    if (target && this.buttons_ && this.buttons_.includes(target)) {
      this.select_(target);
    }
  }

  private populate_() {
    const nodes =
        this.shadowRoot!.querySelector('slot')!.assignedNodes({flatten: true});
    this.buttons_ = Array.from(nodes).flatMap(node => {
      if (node.nodeType !== Node.ELEMENT_NODE) {
        return [];
      }
      const el = node as HTMLElement;

      let result = [];
      if (el.matches(this.selectableElements)) {
        result.push(el);
      }

      if (this.nestedSelectable) {
        result = result.concat(
            Array.from(el.querySelectorAll(this.selectableElements)));
      }
      return result;
    }) as CrRadioButtonElement[];
    this.buttonEventTracker_.removeAll();
    this.buttons_!.forEach(el => {
      this.buttonEventTracker_!.add(
          el, 'disabled-changed', () => this.populate_());
      this.buttonEventTracker_!.add(el, 'name-changed', () => this.populate_());
    });
    this.update_();
  }

  private select_(button: CrRadioButtonElement) {
    if (!isEnabled(button)) {
      return;
    }

    const name = `${button.name}`;
    if (this.selected !== name) {
      this.selected = name;
    }
  }

  private isButtonEnabledAndSelected_(button: CrRadioButtonElement): boolean {
    return !this.disabled && button.checked && isEnabled(button);
  }

  private update_() {
    if (!this.buttons_) {
      return;
    }
    let noneMadeFocusable = true;
    this.buttons_.forEach(radio => {
      radio.checked =
          this.selected !== undefined && `${radio.name}` === `${this.selected}`;
      const disabled = this.disabled || !isEnabled(radio);
      const canBeFocused = radio.checked && !disabled;
      if (canBeFocused) {
        radio.focusable = true;
        noneMadeFocusable = false;
      } else {
        radio.focusable = false;
      }
      radio.setAttribute('aria-disabled', `${disabled}`);
    });
    this.setAttribute('aria-disabled', `${this.disabled}`);
    if (noneMadeFocusable && !this.disabled) {
      const radio = this.buttons_.find(isEnabled);
      if (radio) {
        radio.focusable = true;
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-radio-group': CrRadioGroupElement;
  }
}

customElements.define(CrRadioGroupElement.is, CrRadioGroupElement);
