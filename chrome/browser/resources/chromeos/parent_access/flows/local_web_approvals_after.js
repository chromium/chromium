// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const LocalWebApprovalsAfterBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

class LocalWebApprovalsAfterElement extends LocalWebApprovalsAfterBase {
  static get is() {
    return 'local-web-approvals-after';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(
    LocalWebApprovalsAfterElement.is, LocalWebApprovalsAfterElement);
