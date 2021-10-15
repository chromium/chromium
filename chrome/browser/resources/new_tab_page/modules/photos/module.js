// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nBehavior, loadTimeData} from '../../i18n_setup.js';
import {recordOccurence} from '../../metrics_utils.js';
import {InfoDialogElement} from '../info_dialog.js';
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

      /** @type {boolean} */
      showOptInScreen: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /**
       * @type {boolean}
       * @private
       */
      showExploreMore_:
          {type: Boolean, computed: 'computeShowExploreMore_(memories)'},

      /**
       * @type {string}
       * @private
       */
      headerChipText_:
          {type: Boolean, computed: 'computeHeaderChipText_(showOptInScreen)'},
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.addEventListener('detect-impression', e => {
      chrome.metricsPrivate.recordBoolean(
          'NewTabPage.Photos.ModuleShown', this.showOptInScreen);
    });
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShowExploreMore_() {
    return this.memories.length === 1;
  }

  /**
   * @return {string}
   * @private
   */
  computeHeaderChipText_() {
    return this.showOptInScreen ? loadTimeData.getString('modulesPhotosNew') :
                                  '';
  }

  /** @private */
  onInfoButtonClick_() {
    /** @type {InfoDialogElement} */ (this.$.infoDialogRender.get())
        .showModal();
  }

  /** @private */
  onDismissButtonClick_() {
    PhotosProxy.getHandler().dismissModule();
    this.dispatchEvent(new CustomEvent('dismiss-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getString('modulesPhotosMemoriesHiddenToday'),
        restoreCallback: () => PhotosProxy.getHandler().restoreModule(),
      },
    }));
  }

  /** @private */
  onDisableButtonClick_() {
    this.dispatchEvent(new CustomEvent('disable-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesPhotosMemoriesDisabled')),
      },
    }));
  }

  /** @private */
  onImageLoadError_() {
    chrome.metricsPrivate.recordBoolean('NewTabPage.Photos.ImageLoad', false);
  }

  /** @private */
  onImageLoadSuccess_() {
    chrome.metricsPrivate.recordBoolean('NewTabPage.Photos.ImageLoad', true);
  }

  /** @private */
  onOptInClick_() {
    chrome.metricsPrivate.recordBoolean('NewTabPage.Photos.UserOptIn', true);
    PhotosProxy.getHandler().onUserOptIn(true);
    this.showOptInScreen = false;
  }

  /** @private */
  onOptOutClick_() {
    chrome.metricsPrivate.recordBoolean('NewTabPage.Photos.UserOptIn', false);
    PhotosProxy.getHandler().onUserOptIn(false);
    // Disable the module when user opt out.
    this.onDisableButtonClick_();
  }

  /** @private */
  onMemoryClick_() {
    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
    PhotosProxy.getHandler().onMemoryOpen();
  }

  /**
   * @param {string} url
   * @param {number} numMemories
   * @return {string}
   * @private
   */
  resizeImageUrl_(url, numMemories) {
    // We request image dimensions related to the layout.
    let imgSize = '=w168-h164-p-k-rw-no';
    if (numMemories < 3) {
      imgSize = '=w255-h164-p-k-rw-no';
    }
    return url.replace('?', imgSize + '?');
  }
}

customElements.define(PhotosModuleElement.is, PhotosModuleElement);

/**
 * @return {!Promise<?PhotosModuleElement>}
 */
async function createPhotosElement() {
  const {memories} = await PhotosProxy.getHandler().getMemories();
  const {showOptInScreen} =
      await PhotosProxy.getHandler().shouldShowOptInScreen();
  if (memories.length === 0) {
    return null;
  }
  const element = new PhotosModuleElement();
  element.showOptInScreen = showOptInScreen;
  // We show only the first 3 at most.
  element.memories = memories.slice(0, 3);
  return element;
}

/** @type {!ModuleDescriptor} */
export const photosDescriptor = new ModuleDescriptor(
    /*id=*/ 'photos',
    /*name=*/ loadTimeData.getString('modulesPhotosSentence'),
    createPhotosElement);
