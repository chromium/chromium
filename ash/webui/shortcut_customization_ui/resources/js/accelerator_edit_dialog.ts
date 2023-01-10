// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_edit_view.js';
import '../css/shortcut_customization_shared.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {StrictQueryMixin} from 'chrome://resources/ash/common/typescript_utils/strict_query_mixin.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {DomRepeat, flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './accelerator_edit_dialog.html.js';
import {AcceleratorEditViewElement} from './accelerator_edit_view.js';
import {AcceleratorViewElement, ViewState} from './accelerator_view.js';
import {AcceleratorInfo, AcceleratorSource} from './shortcut_types.js';

declare global {
  interface HTMLElementEventMap {
    'accelerator-capturing-started': CustomEvent<void>;
    'accelerator-capturing-ended': CustomEvent<void>;
  }
}

/**
 * @fileoverview
 * 'accelerator-edit-dialog' is a dialog that displays the accelerators for
 * a given shortcut. Allows users to edit the accelerators.
 * TODO(jimmyxgong): Implement editing accelerators.
 */
const AcceleratorEditDialogElementBase =
    StrictQueryMixin(I18nMixin(PolymerElement));

export class AcceleratorEditDialogElement extends
    AcceleratorEditDialogElementBase {
  static get is(): string {
    return 'accelerator-edit-dialog';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      description: {
        type: String,
        value: '',
      },

      acceleratorInfos: {
        type: Array,
        value: () => [],
      },

      pendingNewAcceleratorState: {
        type: Number,
        value: ViewState.VIEW,
      },

      action: {
        type: Number,
        value: 0,
      },

      source: {
        type: Number,
        value: 0,
      },

      isAcceleratorCapturing: {
        type: Boolean,
        value: false,
      },
    };
  }

  description: string;
  acceleratorInfos: AcceleratorInfo[];
  action: number;
  source: AcceleratorSource;
  protected isAcceleratorCapturing: boolean;
  private pendingNewAcceleratorState: number;
  private acceleratorCapturingStartedListener = (): void =>
      this.onAcceleratorCapturingStarted();
  private acceleratorCapturingEndedListener = (): void =>
      this.onAcceleratorCapturingEnded();

  override connectedCallback(): void {
    super.connectedCallback();
    this.strictQuery(CrDialogElement.is, CrDialogElement).showModal();

    window.addEventListener(
        'accelerator-capturing-started',
        this.acceleratorCapturingStartedListener);
    window.addEventListener(
        'accelerator-capturing-ended', this.acceleratorCapturingEndedListener);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    window.removeEventListener(
        'accelerator-capturing-started',
        this.acceleratorCapturingStartedListener);
    window.removeEventListener(
        'accelerator-capturing-ended', this.acceleratorCapturingEndedListener);
  }

  private getViewList(): DomRepeat {
    return this.strictQuery('#viewList', DomRepeat);
  }

  updateDialogAccelerators(updatedAccels: AcceleratorInfo[]): void {
    this.set('acceleratorInfos', []);
    this.getViewList().render();
    this.acceleratorInfos = updatedAccels;
  }

  protected onDoneButtonClicked(): void {
    this.strictQuery(CrDialogElement.is, CrDialogElement).close();
  }

  protected onDialogClose(): void {
    this.dispatchEvent(
        new CustomEvent('edit-dialog-closed', {bubbles: true, composed: true}));
  }

  private onAcceleratorCapturingStarted(): void {
    this.isAcceleratorCapturing = true;
  }

  private onAcceleratorCapturingEnded(): void {
    this.isAcceleratorCapturing = false;
  }

  private focusAcceleratorItemContainer(): void {
    const editDialog = this.strictQuery(CrDialogElement.is, CrDialogElement);
    const editView = editDialog.querySelector<AcceleratorEditViewElement>(
        '#pendingAccelerator');
    assert(editView);
    const accelItem =
        editView.strictQuery(AcceleratorViewElement.is, AcceleratorViewElement);
    const container = accelItem.strictQueryDiv('#container');
    container!.focus();
  }

  protected onAddAcceleratorClicked(): void {
    this.pendingNewAcceleratorState = ViewState.ADD;

    // Flush the dom so that the AcceleratorEditView is ready to be focused.
    flush();
    this.focusAcceleratorItemContainer();
  }

  protected showAddButton(): boolean {
    // If the state is VIEW, no new pending accelerators are being added.
    return this.pendingNewAcceleratorState === ViewState.VIEW;
  }

  protected onRestoreDefaultButtonClicked(): void {
    // TODO(jimmyxgong): Implement this function.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'accelerator-edit-dialog': AcceleratorEditDialogElement;
  }
}

customElements.define(
    AcceleratorEditDialogElement.is, AcceleratorEditDialogElement);
