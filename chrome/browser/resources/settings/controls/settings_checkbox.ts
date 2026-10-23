// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `settings-checkbox` is a checkbox that controls a supplied preference.
 */
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';

import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SettingsBooleanControlMixin} from './settings_boolean_control_mixin.js';
import {getCss} from './settings_checkbox.css.js';
import {getHtml} from './settings_checkbox.html.js';

export interface SettingsCheckboxElement {
  $: {
    checkbox: CrCheckboxElement,
    subLabel: HTMLElement,
  };
}

const SettingsCheckboxElementBase = SettingsBooleanControlMixin(CrLitElement);

export class SettingsCheckboxElement extends SettingsCheckboxElementBase {
  static get is() {
    return 'settings-checkbox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Alternative source for the sub-label that can contain html markup.
       * Only use with trusted input.
       */
      subLabelHtml: {type: String},
    };
  }

  accessor subLabelHtml: string = '';

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('subLabel') ||
        changedProperties.has('subLabelHtml')) {
      this.$.checkbox.ariaDescription = this.$.subLabel.textContent;
    }
  }

  /** Focus on the inner cr-checkbox. */
  override focus() {
    this.$.checkbox.focus();
  }

  protected onChange_(e: CustomEvent<boolean>) {
    this.checked = e.detail;
    this.notifyChangedByUserInteraction();
  }

  protected hasSubLabel_(): boolean {
    return !!this.subLabel || !!this.subLabelHtml;
  }

  protected sanitizeInnerHtml_(): TrustedHTML {
    return sanitizeInnerHtml(this.subLabelHtml, {
      attrs: [
        'id',
        'aria-label',
      ],
    });
  }

  protected onSubLabelClick_(e: Event) {
    const target = e.target as HTMLElement;
    if (target.tagName === 'A') {
      this.fire('sub-label-link-clicked', {id: target.id});
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
