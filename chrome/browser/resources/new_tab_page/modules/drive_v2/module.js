// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {ModuleDescriptor} from '../module_descriptor.js';

class DriveModuleElement extends PolymerElement {
  static get is() {
    return 'ntp-drive-module-redesigned';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(DriveModuleElement.is, DriveModuleElement);

/** @return {!Promise<!DriveModuleElement>} */
async function createDriveElement() {
  const element = new DriveModuleElement();
  return element;
}

/** @type {!ModuleDescriptor} */
export const driveDescriptor = new ModuleDescriptor(
    /*id*/ 'drive',
    /*name*/ loadTimeData.getString('modulesDriveSentence'),
    createDriveElement);
