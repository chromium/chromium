// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-create-container-dialog' is a component
 * enabling a user to create a new container.
 */

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/cr_elements/cr_input/cr_input.m.js';
import '//resources/cr_elements/md_select_css.m.js';
import '../../settings_shared_css.js';

import {assert} from '//resources/js/assert.m.js';
import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ContainerId, CrostiniBrowserProxy, CrostiniBrowserProxyImpl, DEFAULT_CROSTINI_CONTAINER, DEFAULT_CROSTINI_VM} from './crostini_browser_proxy.js';

/** @polymer */
class ExtraContainersCreateDialog extends PolymerElement {
  static get is() {
    return 'settings-crostini-create-container-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @private {string}
       */
      inputVmName_: {
        type: String,
        value: DEFAULT_CROSTINI_VM,
      },

      /**
       * @private {string}
       */
      inputContainerName_: {
        type: String,
        value: '',
      },

      /**
       * @private {string}
       */
      inputImageServer_: {
        type: String,
        value: '',
      },

      /**
       * @private {string}
       */
      inputImageAlias_: {
        type: String,
        value: '',
      },

      /**
       * @private {boolean}
       */
      advancedToggleExpanded_: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    assert(this.isValidVmName(this.inputVmName_));
    assert(this.isValidContainerName(this.inputContainerName_));
    this.$.dialog.showModal();
    this.$.vmNameInput.value = this.inputVmName_;
    this.$.containerNameInput.value = this.inputContainerName_;
    this.$.imageServerInput.value = this.inputImageServer_;
    this.$.imageAliasInput.value = this.inputImageAlias_;
    this.$.containerNameInput.focus();
  }

  /**
   * @param {string} input The vm name to verify.
   * @return {?boolean} if the input string is a valid vm name.
   */
  isValidVmName(input) {
    // TODO(crbug:1261319) Allowing non a non-default VM based on policy (TBD).
    return true;
  }

  /**
   * @param {string} input The container name to verify.
   * @return {?boolean} if the input string is a valid vm name.
   */
  isValidContainerName(input) {
    return input !== DEFAULT_CROSTINI_CONTAINER;
  }

  /** @private */
  onCancelTap_() {
    this.$.dialog.close();
  }

  /** @private */
  onCreateTap_() {
    if (this.isValidVmName(this.$.vmNameInput.value)) {
      this.inputVmName_ = this.$.vmNameInput.value;
    }
    if (this.isValidContainerName(this.$.containerNameInput.value)) {
      this.inputContainerName_ = this.$.containerNameInput.value;
    }
    if (this.advancedToggleExpanded_) {
      // These elements are part of a dom-if on |advancedToggleExpanded_|
      this.inputImageServer_ = this.$.imageServerInput.value;
      this.inputImageAlias_ = this.$.imageAliasInput.value;
    }

    CrostiniBrowserProxyImpl.getInstance().createContainer(
        {vm_name: this.inputVmName_, container_name: this.inputContainerName_},
        this.inputImageServer_, this.inputImageAlias_);
    this.$.dialog.close();
  }

  /** @private */
  advancedToggleClicked_() {
    this.advancedToggleExpanded_ = !this.advancedToggleExpanded_;
    // Force repaint.
    this.$.dialog.getBoundingClientRect();
  }

  /**
   * @param {boolean} opened Whether the menu is expanded.
   * @return {string} Icon name.
   * @private
   */
  getArrowIcon_(opened) {
    return opened ? 'cr:arrow-drop-up' : 'cr:arrow-drop-down';
  }

  /**
   * @param {boolean} bool
   * @return {string}
   * @private
   */
  boolToString_(bool) {
    return bool.toString();
  }
}

customElements.define(
    ExtraContainersCreateDialog.is, ExtraContainersCreateDialog);
