// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button_style.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../settings_shared.css.js';

import type {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {CrRadioButtonMixin} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PaperRippleMixin} from 'chrome://resources/polymer/v3_0/paper-behaviors/paper-ripple-mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './collapse_radio_button.html.js';

export interface SettingsCollapseRadioButtonElement {
  $: {
    expandButton: CrExpandButtonElement,
  };
}
const SettingsCollapseRadioButtonElementBase =
    PaperRippleMixin(CrRadioButtonMixin(PolymerElement));

export class SettingsCollapseRadioButtonElement extends
    SettingsCollapseRadioButtonElementBase {
  static get is() {
    return 'settings-collapse-radio-button';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      expanded: {
        type: Boolean,
        notify: true,
        value: false,
      },

      noAutomaticCollapse: {
        type: Boolean,
        value: false,
      },

      noCollapse: Boolean,

      label: String,

      indicatorAriaLabel: String,

      icon: {
        type: String,
        value: '',
      },

      /*
       * The Preference associated with the radio group.
       */
      pref: Object,

      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      subLabel: {
        type: String,
        value: '',  // Allows the $hidden= binding to run without being set.
      },

      /*
       * The aria-label attribute associated with the expand button. Used by
       * screen readers when announcing the expand button.
       */
      expandAriaLabel: String,
    };
  }

  static get observers() {
    return [
      'onCheckedChanged_(checked)',
      'onPrefChanged_(pref.*)',
    ];
  }

  expanded: boolean;
  noAutomaticCollapse: boolean;
  noCollapse: boolean;
  override label: string;
  indicatorAriaLabel: string;
  icon: string;
  pref?: chrome.settingsPrivate.PrefObject;
  override disabled: boolean;
  subLabel: string;
  expandAriaLabel: string;
  private pendingUpdateCollapsed_: boolean;

  constructor() {
    super();

    /**
     * Tracks if this button was clicked but wasn't expanded.
     */
    this.pendingUpdateCollapsed_ = false;
  }

  // Overridden from CrRadioButtonMixin
  override getPaperRipple() {
    return this.getRipple();
  }

  // Overridden from PaperRippleMixin
  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  override _createRipple() {
    this._rippleContainer = this.shadowRoot!.querySelector('.disc-wrapper');
    const ripple = super._createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle');
    return ripple;
  }

  /**
   * Updates the collapsed status of this radio button to reflect
   * the user selection actions.
   */
  updateCollapsed() {
    if (this.pendingUpdateCollapsed_) {
      this.pendingUpdateCollapsed_ = false;
      this.expanded = this.checked;
    }
  }

  getBubbleAnchor() {
    const anchor = this.shadowRoot!.querySelector<HTMLElement>('#button');
    assert(anchor);
    return anchor;
  }

  private onCheckedChanged_() {
    this.pendingUpdateCollapsed_ = true;
    if (!this.noAutomaticCollapse) {
      this.updateCollapsed();
    }
  }

  private onPrefChanged_() {
    // If the preference has been set, and is managed, this control should be
    // disabled. Unless the value associated with this control is present in
    // |pref.userSelectableValues|. This will override the disabled set on the
    // element externally.
    this.disabled = !!this.pref &&
        this.pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED &&
        !(!!this.pref.userSelectableValues &&
          this.pref.userSelectableValues.includes(this.name));
  }

  private onExpandClicked_() {
    this.dispatchEvent(
        new CustomEvent('expand-clicked', {bubbles: true, composed: true}));
  }

  private onRadioFocus_() {
    this.getRipple().showAndHoldDown();
  }

  /**
   * Clear the ripple associated with the radio button when the expand button
   * is focused. Stop propagation to prevent the ripple being re-created.
   */
  private onNonRadioFocus_(e: Event) {
    this.getRipple().clear();
    e.stopPropagation();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-collapse-radio-button': SettingsCollapseRadioButtonElement;
  }
}

customElements.define(
    SettingsCollapseRadioButtonElement.is, SettingsCollapseRadioButtonElement);
