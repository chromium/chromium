// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `settings-checkbox` is a checkbox that controls a supplied preference.
 */
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';

import {SettingsBooleanControlMixin} from '/shared/settings/controls/settings_boolean_control_mixin.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './settings_checkbox.html.js';

export interface SettingsCheckboxElement {
  $: {
    checkbox: CrCheckboxElement,
    subLabel: HTMLElement,
  };
}

const SettingsCheckboxElementBase = SettingsBooleanControlMixin(PolymerElement);

export class SettingsCheckboxElement extends SettingsCheckboxElementBase {
  static get is() {
    return 'settings-checkbox';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Alternative source for the sub-label that can contain html markup.
       * Only use with trusted input.
       */
      subLabelHtml: {
        type: String,
        value: '',
      },
    };
  }

  declare subLabelHtml: string;

  static get observers() {
    return [
      'onSubLabelChanged_(subLabel, subLabelHtml)',
    ];
  }

  /** Focus on the inner cr-checkbox. */
  override focus() {
    this.$.checkbox.focus();
  }

  private onSubLabelChanged_() {
    this.$.checkbox.ariaDescription = this.$.subLabel.textContent!;
  }

  private stopPropagation_(event: Event) {
    event.stopPropagation();
  }

  private hasSubLabel_(subLabel: string, subLabelHtml: string): boolean {
    return !!subLabel || !!subLabelHtml;
  }

  private sanitizeInnerHtml_(rawString: string): TrustedHTML {
    return sanitizeInnerHtml(rawString, {
      attrs: [
        'id',
        'aria-label',
      ],
    });
  }

  private onSubLabelClick_(e: Event) {
    const target = e.target as HTMLElement;
    if (target.tagName === 'A') {
      this.dispatchEvent(new CustomEvent(
          'sub-label-link-clicked',
          {bubbles: true, composed: true, detail: {id: target.id}}));
      e.preventDefault();

      // Don't let link click events from the sub-label reach the checkbox.
      e.stopPropagation();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-checkbox': SettingsCheckboxElement;
  }
}

customElements.define(SettingsCheckboxElement.is, SettingsCheckboxElement);
