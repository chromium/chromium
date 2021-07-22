// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nBehavior, loadTimeData} from '../../i18n_setup.js';
import {ModuleDescriptor} from '../module_descriptor.js';
import {PhotosProxy} from './photos_module_proxy.js';

/**
 * The Photos module, which serves Memories for the current user.
 * @polymer
 * @extends {PolymerElement}
 */
class PhotosModuleElement extends mixinBehaviors
([I18nBehavior], PolymerElement) {
  static get is() {
    return 'ntp-photos-module';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {Array<!photos.mojom.Memory>} */
      memories: Array,
    };
  }
}

customElements.define(PhotosModuleElement.is, PhotosModuleElement);

/**
 * @return {!Promise<?PhotosModuleElement>}
 */
async function createPhotosElement() {
  const {memories} = await PhotosProxy.getInstance().handler.getMemories();
  if (memories.length === 0) {
    return null;
  }
  const element = new PhotosModuleElement();
  element.memories = memories;
  return element;
}

/** @type {!ModuleDescriptor} */
export const photosDescriptor = new ModuleDescriptor(
    /*id=*/ 'photos',
    /*name=*/ loadTimeData.getString('modulesPhotosSentence'),
    createPhotosElement);
