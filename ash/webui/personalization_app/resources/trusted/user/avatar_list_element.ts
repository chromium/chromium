// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The avatar-list component displays the list of avatar images
 * that the user can select from.
 */

import {assert} from 'chrome://resources/js/assert_ts.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {isNonEmptyArray, isSelectionEvent} from '../../common/utils.js';
import {DefaultUserImage, UserImage} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {decodeString16} from '../utils.js';

import {AvatarCamera, AvatarCameraMode} from './avatar_camera_element.js';
import {getTemplate} from './avatar_list_element.html.js';
import {fetchDefaultUserImages} from './user_controller.js';
import {getUserProvider} from './user_interface_provider.js';
import {selectLastExternalUserImageUrl} from './user_selectors.js';

export interface AvatarList {
  $: {avatarCamera: AvatarCamera};
}

type Option = {
  id: string,
  class: string,
  imgSrc: string,
  icon: string,
  title: string,
  defaultImageIndex?: number|null,
};

export class AvatarList extends WithPersonalizationStore {
  static get is() {
    return 'avatar-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      defaultUserImages_: Array,

      profileImage_: Object,

      image_: Object,

      lastExternalUserImageUrl_: Object,

      /** The presence of a device camera. */
      isCameraPresent_: {
        type: Boolean,
        value: false,
        observer: 'onIsCameraPresentChanged_',
      },

      /** Whether the camera is off, photo mode, or video mode. */
      cameraMode_: {
        type: String,
        value: null,
      },

      /**
       * List of options to be displayed to the user.
       */
      options_: {
        type: Array,
        computed:
            'computeOptions_(isCameraPresent_, profileImage_, lastExternalUserImageUrl_, defaultUserImages_)',
        observer: 'onOptionsChanged_',
      },
    };
  }

  private defaultUserImages_: Array<DefaultUserImage>|null;
  private profileImage_: Url|null;
  private isCameraPresent_: boolean;
  private cameraMode_: AvatarCameraMode|null;
  private image_: UserImage|null;
  private lastExternalUserImageUrl_: Url|null;
  private options_: Option[];

  override connectedCallback() {
    super.connectedCallback();
    this.watch<AvatarList['defaultUserImages_']>(
        'defaultUserImages_', state => state.user.defaultUserImages);
    this.watch<AvatarList['profileImage_']>(
        'profileImage_', state => state.user.profileImage);
    this.watch<AvatarList['isCameraPresent_']>(
        'isCameraPresent_', state => state.user.isCameraPresent);
    this.watch<AvatarList['image_']>('image_', state => state.user.image);
    this.watch<AvatarList['lastExternalUserImageUrl_']>(
        'lastExternalUserImageUrl_', selectLastExternalUserImageUrl);
    this.updateFromStore();
    fetchDefaultUserImages(getUserProvider(), this.getStore());
  }

  /** Invoked to compute |options_|. */
  private computeOptions_(
      isCameraPresent: AvatarList['isCameraPresent_'],
      profileImage: AvatarList['profileImage_'],
      lastExternalUserImageUrl: AvatarList['lastExternalUserImageUrl_'],
      defaultUserImages: AvatarList['defaultUserImages_']) {
    const options = [] as Option[];
    if (isCameraPresent) {
      // Add camera and video options.
      options.push({
        id: 'openCamera',
        class: 'avatar-button-container',
        imgSrc: '',
        icon: 'personalization:camera',
        title: this.i18n('takeWebcamPhoto'),
      });
      options.push({
        id: 'openVideo',
        class: 'avatar-button-container',
        imgSrc: '',
        icon: 'personalization:loop',
        title: this.i18n('takeWebcamVideo'),
      });
    }
    // Add open folder option.
    options.push({
      id: 'openFolder',
      class: 'avatar-button-container',
      imgSrc: '',
      icon: 'personalization:folder',
      title: this.i18n('chooseAFile'),
    });
    if (profileImage) {
      options.push({
        id: 'profileImage',
        class: 'image-container',
        imgSrc: profileImage.url,
        icon: 'personalization:checkmark',
        title: this.i18n('googleProfilePhoto'),
      });
    }
    if (lastExternalUserImageUrl) {
      options.push({
        id: 'lastExternalImage',
        class: 'image-container',
        imgSrc: lastExternalUserImageUrl.url,
        icon: 'personalization:checkmark',
        title: this.i18n('lastExternalImageTitle'),
      });
    }
    if (isNonEmptyArray(defaultUserImages)) {
      defaultUserImages.forEach(defaultImage => {
        options.push({
          id: `defaultUserImage-${defaultImage.index}`,
          class: 'image-container',
          imgSrc: defaultImage.url.url,
          icon: 'personalization:checkmark',
          title: decodeString16(defaultImage.title),
          defaultImageIndex: defaultImage.index,
        });
      });
    }
    return options;
  }

  /** Invoked on changes to |options_|. */
  private onOptionsChanged_(options: AvatarList['options_']) {
    this.updateList(
        /*propertyPath=*/ 'options_',
        /*identityGetter=*/ (option: Option) => option.id,
        /*newList=*/ options, /*identityBasedUpdate=*/ true);
  }

  private onOptionSelected_(e: Event) {
    if (!isSelectionEvent(e)) {
      return;
    }
    const divElement = e.currentTarget as HTMLDivElement;
    const id = divElement.id;
    switch (id) {
      case 'openCamera':
        this.openCamera_(e);
        break;
      case 'openVideo':
        this.openVideo_(e);
        break;
      case 'openFolder':
        this.onSelectImageFromDisk_(e);
        break;
      case 'profileImage':
        this.onSelectProfileImage_(e);
        break;
      case 'lastExternalImage':
        this.onSelectLastExternalUserImage_(e);
        break;
      default:
        this.onSelectDefaultImage_(e);
        break;
    }
  }

  private getImageClassForOption_(option: Option) {
    if (option.imgSrc) {
      return '';
    }
    return 'hidden';
  }

  private onSelectDefaultImage_(event: Event) {
    if (!isSelectionEvent(event)) {
      return;
    }

    const id = (event.currentTarget as HTMLElement).dataset['id'];
    if (!id) {
      return;
    }

    const index = parseInt(id, 10);
    getUserProvider().selectDefaultImage(index);
  }

  private onSelectProfileImage_(event: Event) {
    if (!isSelectionEvent(event)) {
      return;
    }

    getUserProvider().selectProfileImage();
  }

  private onSelectLastExternalUserImage_(event: Event) {
    if (!isSelectionEvent(event)) {
      return;
    }
    getUserProvider().selectLastExternalUserImage();
  }

  private openCamera_(event: Event) {
    if (!isSelectionEvent(event)) {
      return;
    }
    assert(this.isCameraPresent_, 'Camera needed to record an image');
    this.cameraMode_ = AvatarCameraMode.CAMERA;
  }

  private openVideo_(event: Event) {
    if (!isSelectionEvent(event)) {
      return;
    }
    assert(this.isCameraPresent_, 'Camera needed to record a video');
    this.cameraMode_ = AvatarCameraMode.VIDEO;
  }

  private onIsCameraPresentChanged_(value: boolean) {
    // Potentially hide camera UI if the camera has become unavailable.
    if (!value) {
      this.cameraMode_ = null;
    }
  }

  private onSelectImageFromDisk_(event: Event) {
    if (!isSelectionEvent(event)) {
      return;
    }

    getUserProvider().selectImageFromDisk();
  }

  private onCameraClosed_() {
    this.cameraMode_ = null;
  }

  private getAriaIndex_(i: number): number {
    return i + 1;
  }

  private getAriaSelected_(option: Option, image: UserImage|null): string {
    if (!option) {
      return 'false';
    }
    switch (option.id) {
      case 'openCamera':
      case 'openVideo':
      case 'openFolder':
        return 'false';
      case 'profileImage':
        return (!!image && !!image.profileImage).toString();
      case 'lastExternalImage':
        return (!!image && !!image.externalImage).toString();
      default:
        // Handle default user image.
        return (!!image && !!image.defaultImage &&
                image.defaultImage.index === option.defaultImageIndex)
            .toString();
    }
  }

  private camelToKebab_(className: string): string {
    return className.replace(/[A-Z]/g, m => '-' + m.toLowerCase());
  }

  private getOptionInnerContainerClass_(option: Option, image: UserImage|null):
      string {
    const defaultClass = option ? option.class : 'image-container';
    return this.getAriaSelected_(option, image) === 'true' ?
        `${defaultClass} selected-${this.camelToKebab_(option.id)}` :
        defaultClass;
  }
}

customElements.define(AvatarList.is, AvatarList);
