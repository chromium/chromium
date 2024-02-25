// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-create-container-dialog' is a component
 * enabling a user to create a new container.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import '../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ContainerInfo} from '../guest_os/guest_os_browser_proxy.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, DEFAULT_CROSTINI_CONTAINER, DEFAULT_CROSTINI_VM} from './crostini_browser_proxy.js';
import {getTemplate} from './crostini_extra_containers_create_dialog.html.js';

export interface ExtraContainersCreateDialog {
  $: {
    dialog: CrDialogElement,
    containerFileInput: CrInputElement,
    containerNameInput: CrInputElement,
    imageAliasInput: CrInputElement,
    imageServerInput: CrInputElement,
    vmNameInput: CrInputElement,
  };
}

export class ExtraContainersCreateDialog extends PolymerElement {
  static get is() {
    return 'settings-crostini-create-container-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * List of container properties that are already stored in settings.
       */
      allContainers: {
        type: Array,
        value: [],
      },

      containerFile_: {
        type: String,
        value: '',
      },

      inputVmName_: {
        type: String,
        value: DEFAULT_CROSTINI_VM,
      },

      inputContainerName_: {
        type: String,
        value: '',
      },

      inputImageServer_: {
        type: String,
        value: '',
      },

      inputImageAlias_: {
        type: String,
        value: '',
      },

      advancedToggleExpanded_: {
        type: Boolean,
        value: false,
      },

      disableCreateButton_: {
        type: Boolean,
        value: true,
      },

      validContainerName_: {
        type: Boolean,
        value: true,
      },

      containerNameError_: {
        type: String,
        value: '',
      },
    };
  }

  allContainers: ContainerInfo[];
  private advancedToggleExpanded_: boolean;
  private browserProxy_: CrostiniBrowserProxy;
  private containerFile_: string;
  private containerNameError_: string;
  private disableCreateButton_: boolean;
  private inputContainerName_: string;
  private inputImageAlias_: string;
  private inputImageServer_: string;
  private inputVmName_: string;
  private validContainerName_: boolean;

  constructor() {
    super();

    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();
    this.$.dialog.showModal();
    this.$.vmNameInput.value = this.inputVmName_;
    this.$.containerNameInput.value = this.inputContainerName_;
    this.$.imageServerInput.value = this.inputImageServer_;
    this.$.imageAliasInput.value = this.inputImageAlias_;
    this.$.containerNameInput.focus();
  }

  /**
   * @param input The vm name to verify.
   * @return if the input string is a valid vm name.
   */
  isValidVmName(_input: string): boolean {
    // TODO(crbug:1261319) Allowing non a non-default VM based on policy (TBD).
    return true;
  }

  private validateNames_(): void {
    this.inputVmName_ = this.$.vmNameInput.value.length === 0 ?
        DEFAULT_CROSTINI_VM :
        this.$.vmNameInput.value;
    this.inputContainerName_ = this.$.containerNameInput.value;

    this.containerNameError_ = '';
    if (this.inputContainerName_.length === 0) {
      this.containerNameError_ = loadTimeData.getString(
          'crostiniExtraContainersCreateDialogEmptyContainerNameError');
    } else if (
        this.inputContainerName_ === DEFAULT_CROSTINI_CONTAINER ||
        this.allContainers.find(
            e => e.id.vm_name === this.inputVmName_ &&
                e.id.container_name === this.inputContainerName_)) {
      this.containerNameError_ = loadTimeData.getString(
          'crostiniExtraContainersCreateDialogContainerExistsError');
    }

    this.validContainerName_ = this.containerNameError_.length === 0;
    this.disableCreateButton_ =
        !this.validContainerName_ || !this.isValidVmName(this.inputVmName_);
  }

  private onCancelClick_(): void {
    this.$.dialog.close();
  }

  private onCreateClick_(): void {
    if (this.advancedToggleExpanded_) {
      // These elements are part of a dom-if on |advancedToggleExpanded_|
      this.inputImageServer_ = this.$.imageServerInput.value;
      this.inputImageAlias_ = this.$.imageAliasInput.value;
      this.containerFile_ = this.$.containerFileInput.value;
    }

    this.browserProxy_.createContainer(
        {vm_name: this.inputVmName_, container_name: this.inputContainerName_},
        this.inputImageServer_, this.inputImageAlias_, this.containerFile_);

    this.$.dialog.close();
  }

  private async onSelectContainerFileClick_(): Promise<void> {
    this.$.containerFileInput.value =
        await this.browserProxy_.openContainerFileSelector();
  }

  private advancedToggleClicked_(): void {
    this.advancedToggleExpanded_ = !this.advancedToggleExpanded_;
    // Force repaint.
    this.$.dialog.getBoundingClientRect();
  }

  /**
   * @param opened Whether the menu is expanded.
   * @return Icon name.
   */
  private getArrowIcon_(opened: boolean): string {
    return opened ? 'cr:arrow-drop-up' : 'cr:arrow-drop-down';
  }

  private boolToString_(bool: boolean): string {
    return bool.toString();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-crostini-create-container-dialog': ExtraContainersCreateDialog;
  }
}

customElements.define(
    ExtraContainersCreateDialog.is, ExtraContainersCreateDialog);
