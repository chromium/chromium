// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_edit_view.js';
import '../css/shortcut_customization_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/ash/common/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {DomRepeat, flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EditDialogCompletedActions, UserAction} from '../mojom-webui/shortcut_customization.mojom-webui.js';

import {getTemplate} from './accelerator_edit_dialog.html.js';
import {ViewState} from './accelerator_view.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {AcceleratorConfigResult, AcceleratorInfo, AcceleratorSource, AcceleratorState, EditAction} from './shortcut_types.js';
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
        observer:
            AcceleratorEditDialogElement.prototype.onAcceleratorInfosChanged,
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

      shouldHideRestoreButton: {
        type: Boolean,
        value: true,
      },
    };
  }

  description: string;
  acceleratorInfos: AcceleratorInfo[];
  action: number;
  source: AcceleratorSource;
  protected isAcceleratorCapturing: boolean;
  protected shouldHideRestoreButton: boolean;
  protected observableDefaultAcceleratorsWithConflict: string[];
  private pendingNewAcceleratorState: number;
  private shouldSnapshotConflictDefaults: boolean;
  private defaultAcceleratorsWithConflict: Set<string> = new Set<string>();
  private eventTracker: EventTracker = new EventTracker();
  // Represents bitwise actions done in the dialog.
  private completedActions: number = EditDialogCompletedActions.kNoAction;

  override connectedCallback(): void {
    super.connectedCallback();
    this.$.editDialog.showModal();

    // Update the aria-label of editDialog, by default, it would include all the
    // content within the dialog.
    // 1. Remove 'aria-describedby' to avoid redundant information.
    // 2. Set a custom aria-label indicating the dialog for certain shortcut is
    // open.
    this.$.editDialog.shadowRoot!.querySelector('#dialog')!.removeAttribute(
        'aria-describedby');
    this.$.editDialog.setTitleAriaLabel(
        this.i18n('editDialogAriaLabel', this.description));

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

    getShortcutProvider().recordUserAction(UserAction.kOpenEditDialog);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.completedActions = 0;
    this.eventTracker.removeAll();
    this.set('acceleratorInfos', []);
    this.shouldSnapshotConflictDefaults = false;
    this.defaultAcceleratorsWithConflict.clear();
    this.updateObservableAcceleratorsWithConflict();
  }

  private getViewList(): DomRepeat {
    const viewList = this.shadowRoot!.querySelector<DomRepeat>('#viewList');
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
    getShortcutProvider().recordEditDialogCompletedActions(
        this.completedActions as EditDialogCompletedActions);
    this.dispatchEvent(
        new CustomEvent('edit-dialog-closed', {bubbles: true, composed: true}));
  }

  private onAcceleratorCapturingStarted(): void {
    this.isAcceleratorCapturing = true;
  }

  private onAcceleratorCapturingEnded(): void {
    this.isAcceleratorCapturing = false;
    // Focus on the next logical step after the user is done editing.
    this.focusAddOrDone();
  }

  private onDefaultConflictResolved(
      e: CustomEvent<{stringifiedAccelerator: string}>): void {
    assert(this.defaultAcceleratorsWithConflict.delete(
        e.detail.stringifiedAccelerator));
    this.updateObservableAcceleratorsWithConflict();
  }

  private onEditActionCompleted(e: CustomEvent<{editAction: EditAction}>):
      void {
    this.updateCompletedActions(e.detail.editAction);
  }

  private updateCompletedActions(editAction: EditAction): void {
    // Announce the completed action.
    this.announceCompleteActions(editAction);
    this.completedActions |= editAction;
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

  private focusAddOrDone(): void {
    const selector = this.acceleratorLimitNotReached() ?
        '#addAcceleratorButton' :
        '#doneButton';
    const buttonToFocus =
        this.$.editDialog.querySelector<HTMLButtonElement>(selector);
    assert(buttonToFocus);
    buttonToFocus.focus();
  }

  protected onAddAcceleratorClicked(): void {
    this.pendingNewAcceleratorState = ViewState.ADD;

    // Flush the dom so that the AcceleratorEditView is ready to be focused.
    flush();
    this.focusAcceleratorItemContainer();
    getShortcutProvider().recordUserAction(UserAction.kStartAddAccelerator);
  }

  protected showNewAccelerator(): boolean {
    // Show new pending accelerators when ViewState is ADD.
    return this.pendingNewAcceleratorState === ViewState.ADD &&
        this.acceleratorLimitNotReached();
  }

  protected showAddButton(): boolean {
    // Show addbutton if the state is not ADD and there is no conflict during
    // restore default process.
    return this.pendingNewAcceleratorState !== ViewState.ADD &&
        this.acceleratorLimitNotReached() &&
        this.defaultAcceleratorsWithConflict.size === 0;
  }

  protected isEmptyState(): boolean {
    return this.pendingNewAcceleratorState === ViewState.VIEW &&
        this.getSortedFilteredAccelerators(this.acceleratorInfos).length === 0;
  }

  protected acceleratorLimitNotReached(): boolean {
    let originalAcceleratorsCount = 0;
    for (const acceleratorInfo of this.acceleratorInfos) {
      if (isStandardAcceleratorInfo(acceleratorInfo)) {
        // Check if this is an aliased accelerator, if so do not count it since
        // we only care about the original accelerator that the user or system
        // originally provided.
        if (acceleratorInfo.layoutProperties.standardAccelerator
                    ?.originalAccelerator !== undefined ||
            acceleratorInfo.state !== AcceleratorState.kEnabled) {
          continue;
        }
        ++originalAcceleratorsCount;
      }
    }

    return originalAcceleratorsCount < MAX_NUM_ACCELERATORS;
  }

  protected onRestoreDefaultButtonClicked(): void {
    getShortcutProvider()
        .restoreDefault(this.source, this.action)
        .then(({result}) => {
          getShortcutProvider().recordUserAction(UserAction.kResetAction);
          if (result.result === AcceleratorConfigResult.kSuccess) {
            this.requestUpdateAccelerator(this.source, this.action);
            this.updateCompletedActions(EditAction.RESET);
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

  protected async onAcceleratorInfosChanged(): Promise<void> {
    // Hide restoreButton when current accelerators in the dialog are the same
    // as default accelerators.
    this.shouldHideRestoreButton = await this.areAcceleratorsDefault();
  }

  // Check if current accelerators match the default accelerators for given
  // action id.
  protected async areAcceleratorsDefault(): Promise<boolean> {
    const currentAccelerators =
        this.getSortedFilteredAccelerators(this.acceleratorInfos);
    const defaultAccelerators =
        await getShortcutProvider().getDefaultAcceleratorsForId(this.action);

    if (currentAccelerators.length != defaultAccelerators.accelerators.length) {
      return false;
    }
    // Check if the current accelerators are strictly matched with the default
    // accelerators.
    return currentAccelerators.every(
        acceleratorInfo => isStandardAcceleratorInfo(acceleratorInfo) &&
            defaultAccelerators.accelerators.some(
                defaultAccelerator => JSON.stringify(defaultAccelerator) ===
                    JSON.stringify(getAccelerator(acceleratorInfo))));
  }

  private announceCompleteActions(editAction: EditAction): void {
    let message = '';
    switch (editAction) {
      case EditAction.ADD:
        message = this.i18n('shortcutAdded');
        break;
      case EditAction.EDIT:
        message = this.i18n('shortcutEdited');
        break;
      case EditAction.REMOVE:
        message = this.i18n('shortcutDeleted');
        break;
      case EditAction.RESET:
        message = this.i18n('shortcutRestored');
        break;
      default:
        return;  // No action needed.
    }
    getAnnouncerInstance(this.$.editDialog.getNative()).announce(message);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'accelerator-edit-dialog': AcceleratorEditDialogElement;
  }
}

customElements.define(
    AcceleratorEditDialogElement.is, AcceleratorEditDialogElement);
