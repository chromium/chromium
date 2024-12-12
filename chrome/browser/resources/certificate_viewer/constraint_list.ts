// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '/strings.m.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {CertViewerBrowserProxyImpl} from './browser_proxy.js';
import type {ConstraintChangeResult} from './browser_proxy.js';
import {getCss} from './constraint_list.css.js';
import {getHtml} from './constraint_list.html.js';

export interface ConstraintListElement {
  $: {
    addConstraintInput: CrInputElement,
    addConstraintButton: CrButtonElement,
    constraintDeleteError: HTMLElement,
  };
}

export class ConstraintListElement extends CrLitElement {
  static get is() {
    return 'constraint-list';
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
      editControlsEnabled: {type: Boolean},
      addConstraintErrorMessage: {type: String},
      deleteConstraintErrorMessage: {type: String},
    };
  }

  constraints: string[] = [];
  protected editControlsEnabled: boolean = true;
  protected addConstraintErrorMessage: string = '';
  protected deleteConstraintErrorMessage: string = '';

  // Clear all error messages in this element.
  private clearErrorMessages() {
    // TODO(crbug.com/40928765): clear trust state change error message
    // after this is merged with the rest of the modifications panel.
    this.deleteConstraintErrorMessage = '';
    this.addConstraintErrorMessage = '';
  }

  protected onDeleteConstraintClick_(e: Event) {
    this.clearErrorMessages();

    assert(e.target);
    const constraintToDeleteIndex =
        Number((e.target as HTMLElement).dataset['index']);
    if (this.constraints[constraintToDeleteIndex]) {
      // TODO(crbug.com/40928765): set trust state selector enabled state to
      // false after this is merged with the rest of the modifications panel.
      this.editControlsEnabled = false;
      CertViewerBrowserProxyImpl.getInstance()
          .deleteConstraint(this.constraints[constraintToDeleteIndex])
          .then(this.onDeleteConstraintFinished_.bind(this));
    } else {
      // TODO(crbug.com/40928765): localize.
      this.deleteConstraintErrorMessage =
          'There was an error deleting the constraint';
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
        // TODO(crbug.com/40928765): localize.
        this.deleteConstraintErrorMessage =
            'There was an error deleting the constraint';
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

    // TODO(crbug.com/40928765): set trust state selector enabled state to false
    // after this is merged with the rest of the modifications panel.
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
    } else {
      if (result.status.errorMessage !== undefined) {
        this.addConstraintErrorMessage = result.status.errorMessage;
      } else {
        this.addConstraintErrorMessage = 'Constraint could not be added';
      }
    }
    this.editControlsEnabled = true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'constraint-list': ConstraintListElement;
  }
}

customElements.define(ConstraintListElement.is, ConstraintListElement);
