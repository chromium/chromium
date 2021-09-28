// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `settings-toggle-button` is a toggle that controls a supplied preference.
 */
import '//resources/cr_elements/shared_vars_css.m.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.m.js';
import '//resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../settings_shared_css.js';

import {afterNextRender, html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// <if expr="chromeos">
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.m.js';
// </if>

import {SettingsBooleanControlBehavior, SettingsBooleanControlBehaviorInterface} from './settings_boolean_control_behavior.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {SettingsBooleanControlBehaviorInterface}
 */
const SettingsToggleButtonElementBase =
    mixinBehaviors([SettingsBooleanControlBehavior], PolymerElement);

/** @polymer */
export class SettingsToggleButtonElement extends
    SettingsToggleButtonElementBase {
  static get is() {
    return 'settings-toggle-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      ariaLabel: {
        type: String,
        reflectToAttribute: false,  // Handled by #control.
        observer: 'onAriaLabelSet_',
        value: '',
      },

      elideLabel: {
        type: Boolean,
        reflectToAttribute: true,
      },

      learnMoreUrl: {
        type: String,
        reflectToAttribute: true,
      },

      // <if expr="chromeos">
      subLabelWithLink: {
        type: String,
        reflectToAttribute: true,
      },
      // </if>

      subLabelIcon: {
        type: String,
      },
    };
  }

  static get observers() {
    return [
      'onDisableOrPrefChange_(disabled, pref.*)',
    ];
  }


  /** @override */
  ready() {
    super.ready();

    this.addEventListener('click', this.onHostTap_);
  }

  /**
   * @param {string} eventName
   * @private
   */
  fire_(eventName) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true}));
  }

  /** @override */
  focus() {
    this.$.control.focus();
  }

  /**
   * Removes the aria-label attribute if it's added by $i18n{...}.
   * @private
   */
  onAriaLabelSet_() {
    if (this.hasAttribute('aria-label')) {
      const ariaLabel = this.ariaLabel;
      this.removeAttribute('aria-label');
      this.ariaLabel = ariaLabel;
    }
  }

  /**
   * @return {string}
   * @private
   */
  getAriaLabel_() {
    return this.label || this.ariaLabel;
  }

  /** @private */
  onDisableOrPrefChange_() {
    this.toggleAttribute('effectively-disabled_', this.controlDisabled());
  }

  /**
   * Handles non cr-toggle button clicks (cr-toggle handles its own click events
   * which don't bubble).
   * @param {!Event} e
   * @private
   */
  onHostTap_(e) {
    e.stopPropagation();
    if (this.controlDisabled()) {
      return;
    }

    this.checked = !this.checked;
    this.notifyChangedByUserInteraction();
    this.fire_('change');
  }

  /**
   * @param {!CustomEvent<boolean>} e
   * @private
   */
  onLearnMoreClick_(e) {
    e.stopPropagation();
    this.fire_('learn-more-clicked');
  }

  // <if expr="chromeos">
  /**
   * Set up the contents of sub label with link.
   * @param {string} contents
   * @private
   */
  getSubLabelWithLinkContent_(contents) {
    return sanitizeInnerHtml(
        contents,
        {attrs: ['id', 'aria-hidden', 'aria-labelledby', 'tabindex']});
  }

  /**
   * @param {!Event} e
   * @private
   */
  onSubLabelTextWithLinkClick_(e) {
    if (e.target.tagName === 'A') {
      this.fire_('sub-label-link-clicked');
      e.preventDefault();
      e.stopPropagation();
    }
  }
  // </if>

  /**
   * @param {!CustomEvent<boolean>} e
   * @private
   */
  onChange_(e) {
    this.checked = e.detail;
    this.notifyChangedByUserInteraction();
  }
}

customElements.define(
    SettingsToggleButtonElement.is, SettingsToggleButtonElement);
