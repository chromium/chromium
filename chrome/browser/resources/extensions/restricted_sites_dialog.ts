// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './strings.m.js';
import './shared_style.css.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './restricted_sites_dialog.html.js';

export interface ExtensionsRestrictedSitesDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const ExtensionsRestrictedSitesDialogElementBase = I18nMixin(PolymerElement);

export class ExtensionsRestrictedSitesDialogElement extends
    ExtensionsRestrictedSitesDialogElementBase {
  static get is() {
    return 'extensions-restricted-sites-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      firstRestrictedSite: {type: String, value: ''},
    };
  }

  firstRestrictedSite: string;

  isOpen(): boolean {
    return this.$.dialog.open;
  }

  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  private onCancelClick_() {
    this.$.dialog.cancel();
  }

  private onSubmitClick_() {
    this.$.dialog.close();
  }

  private getDialogTitle_(): string {
    return this.i18n('matchingRestrictedSitesTitle', this.firstRestrictedSite);
  }

  private getDialogWarning_(): string {
    return this.i18n(
        'matchingRestrictedSitesWarning', this.firstRestrictedSite);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-restricted-sites-dialog':
        ExtensionsRestrictedSitesDialogElement;
  }
}

customElements.define(
    ExtensionsRestrictedSitesDialogElement.is,
    ExtensionsRestrictedSitesDialogElement);
