// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../buttons/oobe_text_button.js';
import '../common_styles/oobe_common_styles.css.js';
import '../common_styles/oobe_dialog_host_styles.css.js';
import './oobe_adaptive_dialog.js';
import './oobe_content_dialog.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeDialogHostMixin} from '../mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../mixins/oobe_i18n_mixin.js';
import {OobeCrLottie} from '../oobe_cr_lottie.js';

import {getTemplate} from './oobe_loading_dialog.html.js';

const OobeLoadingDialogBase =
    OobeDialogHostMixin(OobeI18nMixin(PolymerElement));

export class OobeLoadingDialog extends OobeLoadingDialogBase {
  static get is() {
    return 'oobe-loading-dialog' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      titleKey: {
        type: String,
      },

      titleLabelKey: {
        type: String,
      },

      subtitleKey: {
        type: String,
        value: '',
      },

      /*
       * If true loading step can be canceled by pressing a cancel button.
       */
      canCancel: {
        type: Boolean,
        value: false,
      },
    };
  }

  private titleKey: string;
  private titleLabelKey: string;
  private subtitleKey: string;
  private canCancel: boolean;

  private getSpinner(): OobeCrLottie {
    const spinner = this.shadowRoot?.querySelector('#spinner');
    assert(spinner instanceof OobeCrLottie);
    return spinner;
  }

  override onBeforeShow(): void {
    super.onBeforeShow();
    this.getSpinner().playing = true;
  }

  override onBeforeHide(): void {
    super.onBeforeHide();
    this.getSpinner().playing = false;
  }

  // Returns either the passed 'title-label-key', or uses the 'title-key'.
  getAriaLabel(locale: string, titleLabelKey: string, titleKey: string):
      string {
    assert(
        this.titleLabelKey || this.titleKey,
        'OOBE Loading dialog requires a title or a label for a11y!');
    return (titleLabelKey) ? this.i18nDynamic(locale, titleLabelKey) :
                             this.i18nDynamic(locale, titleKey);
  }

  cancel(): void {
    assert(this.canCancel);
    this.dispatchEvent(
        new CustomEvent('cancel-loading', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OobeLoadingDialog.is]: OobeLoadingDialog;
  }
}

customElements.define(OobeLoadingDialog.is, OobeLoadingDialog);
