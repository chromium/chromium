// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ModuleDescriptor} from '../module_descriptor.js';
import {DriveProxy} from './drive_module_proxy.js';

/**
 * @fileoverview The Drive module, which serves as an inside look in to
 * recent activity within a user's Google Drive.
 */

class DriveModuleElement extends PolymerElement {
  static get is() {
    return 'ntp-drive-module';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {Array<!drive.mojom.Document>} */
      documents: Array
    };
  }
}

customElements.define(DriveModuleElement.is, DriveModuleElement);

/**
 * @return {!Promise<DriveModuleElement>}
 */
async function createDriveElement() {
  const {documents} = await DriveProxy.getInstance().handler.getDocuments();
  if (documents.length === 0) {
    return null;
  }
  const element = new DriveModuleElement();
  element.documents = documents;
  return element;
}

/** @type {!ModuleDescriptor} */
export const driveDescriptor = new ModuleDescriptor(
    /*id=*/ 'drive',
    /*heightPx=*/ 314, createDriveElement);
