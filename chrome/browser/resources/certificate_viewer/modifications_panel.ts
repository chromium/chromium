// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '/strings.m.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {CertViewerBrowserProxyImpl} from './browser_proxy.js';
import type {CertMetadataChangeResult, ConstraintChangeResult} from './browser_proxy.js';
import {getCss} from './modifications_panel.css.js';
import {getHtml} from './modifications_panel.html.js';

const ModificationsPanelElementBase = I18nMixinLit(CrLitElement);

export interface ModificationsPanelElement {
  $: {
    addConstraintSection: HTMLElement,
    addConstraintInput: CrInputElement,
    addConstraintButton: CrButtonElement,

    constraintListSection: HTMLElement,
    constraintDeleteError: HTMLElement,

    trustStateSelect: HTMLSelectElement,
    trustStateSelectError: HTMLElement,
  };
}

export class ModificationsPanelElement extends ModificationsPanelElementBase {
  static get is() {
    return 'modifications-panel';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      constraints: {type: Array},
      trustStateValue: {type: String},
      isEditable: {type: Boolean},
      editControlsEnabled: {type: Boolean},
      addConstraintErrorMessage: {type: String},
      deleteConstraintErrorMessage: {type: String},
      trustStateErrorMessage: {type: String},
    };
  }

  accessor constraints: string[] = [];
  accessor trustStateValue: string = '0';
  accessor isEditable: boolean = false;

  protected accessor editControlsEnabled: boolean = true;
  protected accessor addConstraintErrorMessage: string = '';
  protected accessor deleteConstraintErrorMessage: string = '';
  protected accessor trustStateErrorMessage: string = '';

  // Clear all error messages in this element.
  private clearErrorMessages() {
    this.deleteConstraintErrorMessage = '';
    this.addConstraintErrorMessage = '';
    this.trustStateErrorMessage = '';
  }

  protected onDeleteConstraintClick_(e: Event) {
    this.clearErrorMessages();

    assert(e.target);
    const constraintToDeleteIndex =
        Number((e.target as HTMLElement).dataset['index']);
    if (this.constraints[constraintToDeleteIndex]) {
      // Disable editing so we only have one change outstanding at any one time.
      this.editControlsEnabled = false;
      CertViewerBrowserProxyImpl.getInstance()
          .deleteConstraint(this.constraints[constraintToDeleteIndex])
          .then(this.onDeleteConstraintFinished_.bind(this));
    } else {
      this.deleteConstraintErrorMessage =
          this.i18n('deleteConstraintErrorMessage');
    }
  }

  private onDeleteConstraintFinished_(result: ConstraintChangeResult) {
    if (result.status.success) {
      assert(result.constraints);
      this.constraints = result.constraints;
    } else {
      if (result.status.errorMessage !== undefined) {
        this.deleteConstraintErrorMessage = result.status.errorMessage;
      } else {
        this.deleteConstraintErrorMessage =
            this.i18n('deleteConstraintErrorMessage');
      }
    }
    this.editControlsEnabled = true;
  }

  protected onAddConstraintClick_() {
    // If no input, assume this is a misclick.
    const trimmedInput = this.$.addConstraintInput.value.trim();
    if (trimmedInput.length === 0) {
      return;
    }

    // Disable editing so we only have one change outstanding at any one time.
    this.editControlsEnabled = false;
    this.clearErrorMessages();
    CertViewerBrowserProxyImpl.getInstance()
        .addConstraint(trimmedInput)
        .then(this.onAddConstraintFinished_.bind(this));
  }

  private onAddConstraintFinished_(result: ConstraintChangeResult) {
    if (result.status.success) {
      assert(result.constraints);
      this.constraints = result.constraints;
      // Only clear input on successful add.
      this.$.addConstraintInput.value = '';
    } else {
      if (result.status.errorMessage !== undefined) {
        this.addConstraintErrorMessage = result.status.errorMessage;
      } else {
        this.addConstraintErrorMessage = this.i18n('addConstraintErrorMessage');
      }
    }
    this.editControlsEnabled = true;
  }

  protected onTrustStateChange_() {
    // Disable editing so we only have one change outstanding at any one time.
    this.editControlsEnabled = false;
    this.clearErrorMessages();

    CertViewerBrowserProxyImpl.getInstance()
        .updateTrustState(Number(this.$.trustStateSelect.value))
        .then(this.onTrustStateChangeFinished_.bind(this));
  }

  private async onTrustStateChangeFinished_(result: CertMetadataChangeResult) {
    if (result.success) {
      // Update state to the new trust value.
      this.trustStateValue = this.$.trustStateSelect.value;
    } else {
      // Restore UI to the old trust value.
      this.$.trustStateSelect.value = this.trustStateValue;
      if (result.errorMessage !== undefined) {
        this.trustStateErrorMessage = result.errorMessage;
      } else {
        this.trustStateErrorMessage = this.i18n('trustStateErrorMessage');
      }
    }
    this.editControlsEnabled = true;
    // Can't focus disabled elements; wait for update to complete first.
    await this.updateComplete;
    this.$.trustStateSelect.focus();
  }

  protected onConstraintKeyPress_(event: KeyboardEvent): void {
    if (event.key !== 'Enter') {
      return;
    }
    event.stopPropagation();

    this.onAddConstraintClick_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'modifications-panel': ModificationsPanelElement;
  }
}

customElements.define(ModificationsPanelElement.is, ModificationsPanelElement);
