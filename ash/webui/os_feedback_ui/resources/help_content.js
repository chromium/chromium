// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {mojoString16ToString} from '//resources/ash/common/mojo_utils.js';
import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HelpContent, HelpContentList} from './feedback_types.js';

/**
 * @fileoverview
 * 'help-content' displays list of help contents.
 */
export class HelpContentElement extends PolymerElement {
  static get is() {
    return 'help-content';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * An implicit array of help contents to be displayed.
       * @type {!HelpContentList}
       */
      helpContentList: {type: HelpContentList, value: () => []}
    };
  }

  /**
   * Extract the url string from help content.
   * @param {!HelpContent} helpContent
   * @return {string}
   * @protected
   */
  getUrl_(helpContent) {
    return helpContent.url.url;
  }

  /**
   * Extract the title as JS string from help content.
   * @param {!HelpContent} helpContent
   * @return {string}
   * @protected
   */
  getTitle_(helpContent) {
    return mojoString16ToString(helpContent.title);
  }
}

customElements.define(HelpContentElement.is, HelpContentElement);
