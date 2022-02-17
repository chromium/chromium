// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The avatar-list component displays the list of avatar images
 * that the user can select from.
 */

import {assert} from 'chrome://resources/js/assert.m.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isSelectionEvent} from '../../common/utils.js';
import {DefaultUserImage} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {AvatarCamera} from './avatar_camera_element.js';
import {fetchDefaultUserImages} from './user_controller.js';
import {getUserProvider} from './user_interface_provider.js';

export interface AvatarList {
  $: {avatarCamera: AvatarCamera}
}

export class AvatarList extends WithPersonalizationStore {
  static get is() {
    return 'avatar-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      defaultUserImages_: Array,

      profileImage_: Object,

      /** The presence of a device camera. */
      isCameraPresent_: {
        type: Boolean,
        value: false,
        observer: 'onIsCameraPresentChanged_',
      },

      /** Whether to show the camera UI to the user. */
      shouldShowCameraUi_: {
        type: Boolean,
        value: false,
      }
    };
  }

  private defaultUserImages_: Array<DefaultUserImage>|null;
  private profileImage_: Url|null;
  private isCameraPresent_: boolean;
  private shouldShowCameraUi_: boolean;

  connectedCallback() {
    super.connectedCallback();
    this.watch<AvatarList['defaultUserImages_']>(
        'defaultUserImages_', state => state.user.defaultUserImages);
    this.watch<AvatarList['profileImage_']>(
        'profileImage_', state => state.user.profileImage);
    this.watch<AvatarList['isCameraPresent_']>(
        'isCameraPresent_', state => state.user.isCameraPresent);
    this.updateFromStore();
    fetchDefaultUserImages(getUserProvider(), this.getStore());
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

  private openCamera_() {
    assert(this.isCameraPresent_, 'Camera needed to record an image');
    this.shouldShowCameraUi_ = true;
  }

  private onIsCameraPresentChanged_(value: boolean) {
    // Potentially hide camera UI if the camera has become unavailable.
    this.shouldShowCameraUi_ = this.shouldShowCameraUi_ && value;
  }

  private onSelectImageFromDisk_(event: Event) {
    if (!isSelectionEvent(event)) {
      return;
    }

    getUserProvider().selectImageFromDisk();
  }

  private onCameraClosed_() {
    this.shouldShowCameraUi_ = false;
  }
}

customElements.define(AvatarList.is, AvatarList);
