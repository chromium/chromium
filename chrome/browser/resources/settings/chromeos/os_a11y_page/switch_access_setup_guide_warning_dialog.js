// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-switch-access-setup-guide-warning-dialog' is a
 * component warning the user that re-running the setup guide will clear their
 * existing switches. By clicking 'Continue', the user acknowledges that.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../../settings_shared.css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class SettingsSwitchAccessSetupGuideWarningDialogElement extends
    PolymerElement {
  static get is() {
    return 'settings-switch-access-setup-guide-warning-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  /** @private */
  onCancelTap_() {
    this.$.dialog.cancel();
  }

  /** @private */
  onRerunSetupGuideTap_() {
    this.$.dialog.close();
  }
}

customElements.define(
    SettingsSwitchAccessSetupGuideWarningDialogElement.is,
    SettingsSwitchAccessSetupGuideWarningDialogElement);
