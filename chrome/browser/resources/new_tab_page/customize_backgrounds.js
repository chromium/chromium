// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import './mini_page.js';
import './iframe.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nBehavior, loadTimeData} from './i18n_setup.js';
import {NewTabPageProxy} from './new_tab_page_proxy.js';

/**
 * Element that lets the user configure the background.
 * @polymer
 * @extends {PolymerElement}
 */
class CustomizeBackgroundsElement extends mixinBehaviors
([I18nBehavior], PolymerElement) {
  static get is() {
    return 'ntp-customize-backgrounds';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      customBackgroundDisabledByPolicy_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('customBackgroundDisabledByPolicy'),
      },

      /** @private */
      showBackgroundSelection_: {
        type: Boolean,
        computed: 'computeShowBackgroundSelection_(selectedCollection)',
      },

      /** @private {newTabPage.mojom.BackgroundCollection} */
      selectedCollection: {
        notify: true,
        observer: 'onSelectedCollectionChange_',
        type: Object,
        value: null,
      },

      /** @type {!newTabPage.mojom.Theme} */
      theme: Object,

      /** @private {!Array<!newTabPage.mojom.BackgroundCollection>} */
      collections_: Array,

      /** @private {!Array<!newTabPage.mojom.CollectionImage>} */
      images_: Array,
    };
  }

  constructor() {
    super();
    if (this.customBackgroundDisabledByPolicy_) {
      return;
    }
    /** @private {newTabPage.mojom.PageHandlerRemote} */
    this.pageHandler_ = NewTabPageProxy.getInstance().handler;
    this.pageHandler_.getBackgroundCollections().then(({collections}) => {
      this.collections_ = collections;
    });
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShowBackgroundSelection_() {
    return !this.customBackgroundDisabledByPolicy_ && !this.selectedCollection;
  }

  /**
   * @return {string}
   * @private
   */
  getCustomBackgroundClass_() {
    return this.theme && this.theme.backgroundImage &&
            this.theme.backgroundImage.url.url.startsWith(
                'chrome-untrusted://new-tab-page/background.jpg') ?
        'selected' :
        '';
  }

  /**
   * @return {string}
   * @private
   */
  getNoBackgroundClass_() {
    return this.theme &&
            (this.theme.backgroundImage && !this.theme.isCustomBackground ||
             !this.theme.backgroundImage &&
                 !this.theme.dailyRefreshCollectionId) ?
        'selected' :
        '';
  }

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getImageSelectedClass_(index) {
    const {url} = this.images_[index].imageUrl;
    return this.theme && this.theme.backgroundImage &&
            this.theme.backgroundImage.url.url === url &&
            !this.theme.dailyRefreshCollectionId ?
        'selected' :
        '';
  }

  /**
   * @param {!Event} e
   * @private
   */
  onCollectionClick_(e) {
    this.selectedCollection = this.$.collectionsRepeat.itemForElement(e.target);
    this.pageHandler_.onCustomizeDialogAction(
        newTabPage.mojom.CustomizeDialogAction.kBackgroundsCollectionOpened);
  }

  /** @private */
  async onUploadFromDeviceClick_() {
    this.pageHandler_.onCustomizeDialogAction(
        newTabPage.mojom.CustomizeDialogAction
            .kBackgroundsUploadFromDeviceClicked);
    const {success} = await this.pageHandler_.chooseLocalCustomBackground();
    if (success) {
      // The theme update is asynchronous. Close the dialog and allow ntp-app
      // to update the background.
      this.dispatchEvent(new Event('close', {bubbles: true, composed: true}));
    }
  }

  /** @private */
  onDefaultClick_() {
    if (!this.theme.isCustomBackground) {
      this.pageHandler_.onCustomizeDialogAction(
          newTabPage.mojom.CustomizeDialogAction
              .kBackgroundsNoBackgroundSelected);
    }
    this.pageHandler_.setNoBackgroundImage();
  }

  /**
   * @param {!Event} e
   * @private
   */
  onImageClick_(e) {
    const image = this.$.imagesRepeat.itemForElement(e.target);
    if (this.theme.isCustomBackground && this.theme.backgroundImage !== image) {
      this.pageHandler_.onCustomizeDialogAction(
          newTabPage.mojom.CustomizeDialogAction.kBackgroundsImageSelected);
    }
    const {attribution1, attribution2, attributionUrl, imageUrl} = image;
    this.pageHandler_.setBackgroundImage(
        attribution1, attribution2, attributionUrl, imageUrl);
  }

  /** @private */
  async onSelectedCollectionChange_() {
    this.images_ = [];
    if (!this.selectedCollection) {
      return;
    }
    const collectionId = this.selectedCollection.id;
    const {images} = await this.pageHandler_.getBackgroundImages(collectionId);
    // We check the IDs match since the user may have already moved to a
    // different collection before the results come back.
    if (!this.selectedCollection ||
        this.selectedCollection.id !== collectionId) {
      return;
    }
    this.images_ = images;
  }

  revertBackgroundChanges() {
    this.pageHandler_.revertBackgroundChanges();
  }

  confirmBackgroundChanges() {
    this.pageHandler_.confirmBackgroundChanges();
  }
}

customElements.define(
    CustomizeBackgroundsElement.is, CustomizeBackgroundsElement);
