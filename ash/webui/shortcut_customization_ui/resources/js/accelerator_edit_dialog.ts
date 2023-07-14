// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_edit_view.js';
import '../css/shortcut_customization_shared.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {DomRepeat, flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './accelerator_edit_dialog.html.js';
import {ViewState} from './accelerator_view.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {AcceleratorConfigResult, AcceleratorInfo, AcceleratorSource, AcceleratorState} from './shortcut_types.js';
import {compareAcceleratorInfos, getAccelerator, isStandardAcceleratorInfo} from './shortcut_utils.js';

export type DefaultConflictResolvedEvent = CustomEvent<{accelerator: string}>;

export interface AcceleratorEditDialogElement {
  $: {
    editDialog: CrDialogElement,
  };
}

declare global {
  interface HTMLElementEventMap {
    'accelerator-capturing-started': CustomEvent<void>;
    'accelerator-capturing-ended': CustomEvent<void>;
    'default-conflict-resolved': DefaultConflictResolvedEvent;
  }
}

// A maximum of 5 accelerators are allowed.
const MAX_NUM_ACCELERATORS = 5;

/**
 * @fileoverview
 * 'accelerator-edit-dialog' is a dialog that displays the accelerators for
 * a given shortcut. Allows users to edit the accelerators.
 * TODO(jimmyxgong): Implement editing accelerators.
 */
const AcceleratorEditDialogElementBase = I18nMixin(PolymerElement);

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

      // `Set` is not an observable type in Polymer, so this `Array` mirrors
      // `defaultAcceleratorsWithConflict` but is observable to template
      // observers functions.
      observableDefaultAcceleratorsWithConflict: {
        type: Array,
        value: () => [],
      },
    };
  }

  description: string;
  acceleratorInfos: AcceleratorInfo[];
  action: number;
  source: AcceleratorSource;
  protected isAcceleratorCapturing: boolean;
  protected observableDefaultAcceleratorsWithConflict: string[];
  private pendingNewAcceleratorState: number;
  private shouldSnapshotConflictDefaults: boolean;
  private defaultAcceleratorsWithConflict: Set<string> = new Set<string>();
  private eventTracker: EventTracker = new EventTracker();

  override connectedCallback(): void {
    super.connectedCallback();
    this.$.editDialog.showModal();

    this.eventTracker.add(
        window, 'accelerator-capturing-started',
        () => this.onAcceleratorCapturingStarted());
    this.eventTracker.add(
        window, 'accelerator-capturing-ended',
        () => this.onAcceleratorCapturingEnded());
    this.eventTracker.add(
        this, 'default-conflict-resolved',
        (e: CustomEvent<{stringifiedAccelerator: string}>) =>
            this.onDefaultConflictResolved(e));
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.eventTracker.removeAll();
    this.set('acceleratorInfos', []);
    this.shouldSnapshotConflictDefaults = false;
    this.defaultAcceleratorsWithConflict.clear();
    this.updateObservableAcceleratorsWithConflict();
  }

  private getViewList(): DomRepeat {
    const viewList = this.shadowRoot!.querySelector('#viewList') as DomRepeat;
    assert(viewList);
    return viewList;
  }

  updateDialogAccelerators(updatedAccelerators: AcceleratorInfo[]): void {
    // After accelerators have been updated from restoring defaults, snapshot
    // default accelerators that have been disabled.
    if (this.shouldSnapshotConflictDefaults) {
      this.shouldSnapshotConflictDefaults = false;
      for (const acceleratorInfo of updatedAccelerators) {
        if (acceleratorInfo.state === AcceleratorState.kDisabledByUser &&
            isStandardAcceleratorInfo(acceleratorInfo)) {
          this.defaultAcceleratorsWithConflict.add(
              JSON.stringify(getAccelerator(acceleratorInfo)));
          this.updateObservableAcceleratorsWithConflict();
        }
      }
    }

    this.set('acceleratorInfos', []);
    this.getViewList().render();
    this.acceleratorInfos = updatedAccelerators.filter(
        accel => accel.state !== AcceleratorState.kDisabledByUnavailableKeys);
  }

  protected onDoneButtonClicked(): void {
    this.$.editDialog.close();
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

  private onDefaultConflictResolved(
      e: CustomEvent<{stringifiedAccelerator: string}>): void {
    assert(this.defaultAcceleratorsWithConflict.delete(
        e.detail.stringifiedAccelerator));
    this.updateObservableAcceleratorsWithConflict();
  }

  private focusAcceleratorItemContainer(): void {
    const editView = this.$.editDialog.querySelector('#pendingAccelerator');
    assert(editView);
    const accelItem = editView.shadowRoot!.querySelector('#acceleratorItem');
    assert(accelItem);
    const container =
        accelItem.shadowRoot!.querySelector<HTMLElement>('#container');
    assert(container);
    container!.focus();
  }

  protected onAddAcceleratorClicked(): void {
    this.pendingNewAcceleratorState = ViewState.ADD;

    // Flush the dom so that the AcceleratorEditView is ready to be focused.
    flush();
    this.focusAcceleratorItemContainer();
  }

  protected showNewAccelerator(): boolean {
    // Show new pending accelerators when ViewState is not VIEW.
    return this.pendingNewAcceleratorState != ViewState.VIEW &&
        this.acceleratorLimitNotReached();
  }

  protected showAddButton(): boolean {
    // If the state is VIEW, no new pending accelerators are being added.
    return this.pendingNewAcceleratorState === ViewState.VIEW &&
        this.acceleratorLimitNotReached() && !this.shouldHideRestoreDefaults();
  }

  protected acceleratorLimitNotReached(): boolean {
    return this.acceleratorInfos.length < MAX_NUM_ACCELERATORS;
  }

  protected onRestoreDefaultButtonClicked(): void {
    getShortcutProvider()
        .restoreDefault(this.source, this.action)
        .then(({result}) => {
          // TODO(jimmyxgong): Potentially show partial resets as an error.
          if (result.result === AcceleratorConfigResult.kSuccess) {
            this.requestUpdateAccelerator(this.source, this.action);
          } else if (
              result.result ===
              AcceleratorConfigResult.kRestoreSuccessWithConflicts) {
            this.shouldSnapshotConflictDefaults = true;
            this.requestUpdateAccelerator(this.source, this.action);
          }
        });
  }

  protected getSortedFilteredAccelerators(accelerators: AcceleratorInfo[]):
      AcceleratorInfo[] {
    const filteredAccelerators = accelerators.filter(accel => {
      // If restore default is clicked, we allow `kDisabledByUser`.
      const hasDefaultConflicts =
          this.defaultAcceleratorsWithConflict.size !== 0;
      if (hasDefaultConflicts && isStandardAcceleratorInfo(accel)) {
        return this.defaultAcceleratorsWithConflict.has(
                   JSON.stringify(getAccelerator(accel))) &&
            accel.state !== AcceleratorState.kDisabledByUnavailableKeys;
      }

      return accel.state !== AcceleratorState.kDisabledByUser &&
          accel.state !== AcceleratorState.kDisabledByUnavailableKeys;
    });
    return filteredAccelerators.sort(compareAcceleratorInfos);
  }

  private requestUpdateAccelerator(source: number, action: number): void {
    this.dispatchEvent(new CustomEvent('request-update-accelerator', {
      bubbles: true,
      composed: true,
      detail: {source, action},
    }));
  }

  private updateObservableAcceleratorsWithConflict(): void {
    this.set(
        'observableDefaultAcceleratorsWithConflict',
        Array.from(this.defaultAcceleratorsWithConflict));
  }

  protected shouldHideRestoreDefaults(): boolean {
    return this.isAcceleratorCapturing ||
        this.defaultAcceleratorsWithConflict.size !== 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'accelerator-edit-dialog': AcceleratorEditDialogElement;
  }
}

customElements.define(
    AcceleratorEditDialogElement.is, AcceleratorEditDialogElement);
