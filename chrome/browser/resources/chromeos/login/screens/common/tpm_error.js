// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for TPM error screen.
 */

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {LoginScreenBehaviorInterface}
 */
const TPMErrorMessageElementBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    Polymer.Element);

class TPMErrorMessage extends TPMErrorMessageElementBase {
  static get is() {
    return 'tpm-error-message-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {};
  }

  constructor() {
    super();
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('TPMErrorMessageScreen', {
      resetAllowed: true,
    });
  }

  /** @override */
  get EXTERNAL_API() {
    return [];
  }

  onRestartTap_() {
    this.userActed('reboot-system');
  }

  /**
   * @override
   * @suppress {missingProperties}
   */
  get defaultControl() {
    return this.$.errorDialog;
  }
}

customElements.define(TPMErrorMessage.is, TPMErrorMessage);
