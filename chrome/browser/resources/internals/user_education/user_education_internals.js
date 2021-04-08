// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UserEducationInternalsPageHandler, UserEducationInternalsPageHandlerRemote} from '/chrome/browser/ui/webui/internals/user_education/user_education_internals.mojom-webui.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class UserEducationInternalsElement extends PolymerElement {
  static get is() {
    return 'user-education-internals';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * List of tutorials that can be started. Each tutorial has a string
       * identifier.
       * @private {!Array<string>}
       */
      tutorials_: Array,
    };
  }

  constructor() {
    super();
    /** @private {UserEducationInternalsPageHandlerRemote} */
    this.handler_ = UserEducationInternalsPageHandler.getRemote();
  }

  /** @override */
  ready() {
    super.ready();
    // TODO(crbug.com/1194751): fetch tutorial IDs from handler and display
    // them.
    this.tutorials_ = ['test1', 'test2'];
  }

  /**
   * @param {!Object} e
   * @private
   */
  startTutorial_(e) {
    // TODO(crbug.com/1194751): start tutorial through handler
  }
}

customElements.define(
    UserEducationInternalsElement.is, UserEducationInternalsElement);
