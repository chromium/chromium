// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import './mini_page.js';
import './iframe.js';

import {DomRepeat, DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './customize_backgrounds.html.js';
import {loadTimeData} from './i18n_setup.js';
import {BackgroundCollection, CollectionImage, CustomizeDialogAction, PageHandlerRemote, Theme} from './new_tab_page.mojom-webui.js';
import {NewTabPageProxy} from './new_tab_page_proxy.js';

export interface CustomizeBackgroundsElement {
  $: {
    collections: HTMLElement,
    images: HTMLElement,
    imagesRepeat: DomRepeat,
    noBackground: HTMLElement,
    uploadFromDevice: HTMLElement,
  };
}

/** Element that lets the user configure the background. */
export class CustomizeBackgroundsElement extends PolymerElement {
  static get is() {
    return 'ntp-customize-backgrounds';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      customBackgroundDisabledByPolicy_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('customBackgroundDisabledByPolicy'),
      },

      showBackgroundSelection_: {
        type: Boolean,
        computed: 'computeShowBackgroundSelection_(selectedCollection)',
      },

      selectedCollection: {
        notify: true,
        observer: 'onSelectedCollectionChange_',
        type: Object,
        value: null,
      },

      theme: Object,
      collections_: Array,
      images_: Array,
    };
  }

  theme: Theme;
  selectedCollection: BackgroundCollection|null;

  private customBackgroundDisabledByPolicy_: boolean;
  private showBackgroundSelection_: boolean;
  private collections_: BackgroundCollection[];
  private images_: CollectionImage[];

  private pageHandler_: PageHandlerRemote;

  constructor() {
    super();
    if (this.customBackgroundDisabledByPolicy_) {
      return;
    }
    this.pageHandler_ = NewTabPageProxy.getInstance().handler;
    this.pageHandler_.getBackgroundCollections().then(({collections}) => {
      this.collections_ = collections;
    });
  }

  private computeShowBackgroundSelection_(): boolean {
    return !this.customBackgroundDisabledByPolicy_ && !this.selectedCollection;
  }

  private getCustomBackgroundClass_(): string {
    return this.theme && this.theme.backgroundImage &&
            this.theme.backgroundImage.url.url.startsWith(
                'chrome-untrusted://new-tab-page/background.jpg') ?
        'selected' :
        '';
  }

  private getNoBackgroundClass_(): string {
    return this.theme &&
            (this.theme.backgroundImage && !this.theme.isCustomBackground ||
             !this.theme.backgroundImage && !this.theme.dailyRefreshEnabled) ?
        'selected' :
        '';
  }

  private getImageSelectedClass_(index: number): string {
    const {url} = this.images_[index].imageUrl;
    return this.theme && this.theme.backgroundImage &&
            this.theme.backgroundImage.url.url === url &&
            !this.theme.dailyRefreshEnabled ?
        'selected' :
        '';
  }

  private onCollectionClick_(e: DomRepeatEvent<BackgroundCollection>) {
    this.selectedCollection = e.model.item;
    this.pageHandler_.onCustomizeDialogAction(
        CustomizeDialogAction.kBackgroundsCollectionOpened);
  }

  private async onUploadFromDeviceClick_() {
    this.pageHandler_.onCustomizeDialogAction(
        CustomizeDialogAction.kBackgroundsUploadFromDeviceClicked);
    const {success} = await this.pageHandler_.chooseLocalCustomBackground();
    if (success) {
      // The theme update is asynchronous. Close the dialog and allow ntp-app
      // to update the background.
      this.dispatchEvent(new Event('close', {bubbles: true, composed: true}));
    }
  }

  private onDefaultClick_() {
    if (!this.theme.isCustomBackground) {
      this.pageHandler_.onCustomizeDialogAction(
          CustomizeDialogAction.kBackgroundsNoBackgroundSelected);
    }
    this.pageHandler_.setNoBackgroundImage();
  }

  private onImageClick_(e: DomRepeatEvent<CollectionImage>) {
    const image = e.model.item;
    if (this.theme.isCustomBackground &&
        this.theme.backgroundImage!.url.url !== image.imageUrl.url) {
      this.pageHandler_.onCustomizeDialogAction(
          CustomizeDialogAction.kBackgroundsImageSelected);
    }
    const {
      attribution1,
      attribution2,
      attributionUrl,
      imageUrl,
      previewImageUrl,
      collectionId,
    } = image;
    this.pageHandler_.setBackgroundImage(
        attribution1, attribution2, attributionUrl, imageUrl, previewImageUrl,
        collectionId);
  }

  private async onSelectedCollectionChange_() {
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
    if (!this.customBackgroundDisabledByPolicy_) {
      this.pageHandler_.revertBackgroundChanges();
    }
  }

  confirmBackgroundChanges() {
    if (!this.customBackgroundDisabledByPolicy_) {
      this.pageHandler_.confirmBackgroundChanges();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-customize-backgrounds': CustomizeBackgroundsElement;
  }
}

customElements.define(
    CustomizeBackgroundsElement.is, CustomizeBackgroundsElement);
