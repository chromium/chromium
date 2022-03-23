// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-disk-resize-confirmation-dialog' is a
 * component warning the user that resizing a sparse disk cannot be undone.
 * By clicking 'Reserve size', the user agrees to start the operation.
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '../../settings_shared_css.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, CrostiniDiskInfo, CrostiniPortActiveSetting, CrostiniPortProtocol, CrostiniPortSetting, DEFAULT_CROSTINI_CONTAINER, DEFAULT_CROSTINI_VM, MAX_VALID_PORT_NUMBER, MIN_VALID_PORT_NUMBER, PortState} from './crostini_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-crostini-disk-resize-confirmation-dialog',

  /** @override */
  attached() {
    this.getDialog_().showModal();
  },

  /** @private */
  onCancelTap_() {
    this.getDialog_().cancel();
  },

  /** @private */
  onReserveSizeTap_() {
    this.getDialog_().close();
  },

  /**
   * @private
   * @return {!CrDialogElement}
   */
  getDialog_() {
    return /** @type{!CrDialogElement} */ (this.$.dialog);
  }
});
