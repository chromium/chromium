// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The user-preview component displays information about the
 * current user.
 */

import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';

import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {UserImage, UserInfo} from '../personalization_app.mojom-webui.js';
import {Paths, PersonalizationRouter} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {decodeString16} from '../utils.js';

import {initializeUserData} from './user_controller.js';
import {UserImageObserver} from './user_image_observer.js';
import {getUserProvider} from './user_interface_provider.js';
import {getTemplate} from './user_preview_element.html.js';
import {selectUserImageUrl} from './user_selectors.js';

export class UserPreview extends WithPersonalizationStore {
  static get is() {
    return 'user-preview';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      path: String,
      info_: Object,
      image_: Object,
      imageUrl_: {
        type: String,
        observer: 'onImageUrlChanged_',
        value: null,
      },
      imageIsEnterpriseManaged_: {
        type: Boolean,
        value: null,
      },
    };
  }

  private path: string;
  private info_: UserInfo|null;
  private image_: UserImage|null;
  private imageUrl_: Url|null;
  private imageIsEnterpriseManaged_: boolean|null;

  override connectedCallback() {
    super.connectedCallback();
    UserImageObserver.initUserImageObserverIfNeeded();
    this.watch<UserPreview['info_']>('info_', state => state.user.info);
    this.watch<UserPreview['image_']>('image_', state => state.user.image);
    this.watch<UserPreview['imageUrl_']>('imageUrl_', selectUserImageUrl);
    this.watch<UserPreview['imageIsEnterpriseManaged_']>(
        'imageIsEnterpriseManaged_',
        state => state.user.imageIsEnterpriseManaged);
    this.updateFromStore();
    initializeUserData(getUserProvider(), this.getStore());
  }

  private onClickUserEmail_() {
    window.open('chrome://os-settings/accountManager');
  }

  private onClickUserSubpageLink_() {
    PersonalizationRouter.instance().goToRoute(Paths.USER);
  }

  private onImageUrlChanged_(_: Url|null, old: Url|null): void {
    if (old && old.url.startsWith('blob:')) {
      // Revoke old object urls to clear memory. This is safe to call multiple
      // times.
      URL.revokeObjectURL(old.url);
    }
  }

  private shouldShowMainPageView_(path: string, isEnterpriseManaged: boolean):
      boolean {
    return path === Paths.ROOT && !isEnterpriseManaged;
  }

  private shouldShowSubpageView_(path: string, isEnterpriseManaged: boolean):
      boolean {
    return path === Paths.USER && !isEnterpriseManaged;
  }

  private getImageContainerAriaLabel_(
      path: string, isEnterpriseManaged: boolean): string|boolean {
    if (this.shouldShowMainPageView_(path, isEnterpriseManaged) ||
        isEnterpriseManaged) {
      return this.i18n('ariaLabelChangeAvatar');
    }
    if (this.shouldShowSubpageView_(path, isEnterpriseManaged)) {
      return this.i18n('ariaLabelCurrentAvatar');
    }
    // No aria-label attribute will be set.
    return false;
  }

  private getImageAlt_(image: UserImage|null): string {
    if (!image || image.invalidImage) {
      return '';
    }
    if (image.defaultImage) {
      return decodeString16(image.defaultImage.title);
    }
    if (image.externalImage) {
      return this.i18n('lastExternalImageTitle');
    }
    if (image.profileImage) {
      return this.i18n('googleProfilePhoto');
    }

    console.warn('Unknown image type received');
    return '';
  }
}

customElements.define(UserPreview.is, UserPreview);
