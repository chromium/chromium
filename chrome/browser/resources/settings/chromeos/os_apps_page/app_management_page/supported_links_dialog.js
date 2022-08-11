// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const AppManagementSupportedLinksDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class AppManagementSupportedLinksDialogElement extends
    AppManagementSupportedLinksDialogElementBase {
  static get is() {
    return 'app-management-supported-links-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!App} */
      app: Object,
    };
  }
}

customElements.define(
    AppManagementSupportedLinksDialogElement.is,
    AppManagementSupportedLinksDialogElement);
