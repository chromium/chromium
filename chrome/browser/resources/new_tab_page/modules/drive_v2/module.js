// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nBehavior, loadTimeData} from '../../i18n_setup.js';
import {DriveProxy} from '../drive/drive_module_proxy.js';
import {ModuleDescriptor} from '../module_descriptor.js';

/**
 * The Drive module, which serves as an inside look in to recent activity within
 * a user's Google Drive.
 * @polymer
 * @extends {PolymerElement}
 */
class DriveModuleElement extends mixinBehaviors
([I18nBehavior], PolymerElement) {
  static get is() {
    return 'ntp-drive-module-redesigned';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Array<!drive.mojom.File>} */
      files: Array,
    };
  }

  /**
   * @param {drive.mojom.File} file
   * @return {string}
   * @private
   */
  getImageSrc_(file) {
    return 'https://drive-thirdparty.googleusercontent.com/32/type/' +
        file.mimeType;
  }
}

customElements.define(DriveModuleElement.is, DriveModuleElement);

/** @return {!Promise<DriveModuleElement>} */
async function createDriveElement() {
  const {files} = await DriveProxy.getInstance().handler.getFiles();
  if (files.length === 0) {
    return null;
  }
  const element = new DriveModuleElement();
  element.files = files;
  return element;
}

/** @type {!ModuleDescriptor} */
export const driveDescriptor = new ModuleDescriptor(
    /*id*/ 'drive',
    /*name*/ loadTimeData.getString('modulesDriveSentence'),
    createDriveElement);
