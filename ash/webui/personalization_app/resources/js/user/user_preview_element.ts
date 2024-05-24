// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The user-preview component displays information about the
 * current user.
 */

import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';

import {isNonEmptyArray} from 'chrome://resources/ash/common/sea_pen/sea_pen_utils.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';

import {UserImage, UserInfo} from '../../personalization_app.mojom-webui.js';
import {Paths, PersonalizationRouterElement} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {isNonEmptyString} from '../utils.js';

import {initializeUserData} from './user_controller.js';
import {UserImageObserver} from './user_image_observer.js';
import {getUserProvider} from './user_interface_provider.js';
import {getTemplate} from './user_preview_element.html.js';
import {selectUserImageUrl} from './user_selectors.js';
import {getAvatarUrl} from './utils.js';

class AvatarChangedEvent extends CustomEvent<{text: string}> {
  constructor() {
    super(
        'iron-announce',
        {
          bubbles: true,
          composed: true,
          detail: {text: loadTimeData.getString('ariaAnnounceAvatarChanged')},
        },
    );
  }
}

export class UserPreviewElement extends WithPersonalizationStore {
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

  override ready() {
    super.ready();
    IronA11yAnnouncer.requestAvailability();
  }

  override connectedCallback() {
    super.connectedCallback();
    UserImageObserver.initUserImageObserverIfNeeded();
    this.watch<UserPreviewElement['info_']>('info_', state => state.user.info);
    this.watch<UserPreviewElement['image_']>(
        'image_', state => state.user.image);
    this.watch<UserPreviewElement['imageUrl_']>(
        'imageUrl_', selectUserImageUrl);
    this.watch<UserPreviewElement['imageIsEnterpriseManaged_']>(
        'imageIsEnterpriseManaged_',
        state => state.user.imageIsEnterpriseManaged);
    this.updateFromStore();
    initializeUserData(getUserProvider(), this.getStore());
  }

  private onClickUserSubpageLink_() {
    PersonalizationRouterElement.instance().goToRoute(Paths.USER);
  }

  private onImageUrlChanged_(value: Url|null, old: Url|null): void {
    if (value && old) {
      this.dispatchEvent(new AvatarChangedEvent());
    }
  }

  private onImgError_(e: Event) {
    const divElement = e.currentTarget as HTMLDivElement;
    divElement.setAttribute('hidden', 'true');
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
      return mojoString16ToString(image.defaultImage.title);
    }
    if (image.externalImage) {
      return this.i18n('lastExternalImageTitle');
    }
    if (image.profileImage) {
      return this.i18n('googleProfilePhoto');
    }

    console.error('Unknown image type received');
    return '';
  }

  /**
   * Creates style string with static background image url for default user
   * images . Static image loads faster and will provide a smooth experience
   * when the animated image complete loading.
   */
  private getImgBackgroundStyle_(url: string|null): string {
    // Only add background image for default user images.
    if (!this.image_ || this.image_.invalidImage || !this.image_.defaultImage ||
        !url) {
      return '';
    }
    assert(
        !url.startsWith('chrome://image/'), 'The url should not be sanitized');
    return `background-image: url('${
        getAvatarUrl(url, /*staticEncode=*/ true)}')`;
  }

  private getAvatarUrl_(url: string): string {
    return getAvatarUrl(url);
  }

  private shouldShowDeprecatedImageSourceInfo_(image: UserImage|null): boolean {
    return !!image && !!image.defaultImage && !!image.defaultImage.sourceInfo &&
        isNonEmptyArray(image.defaultImage.sourceInfo.author.data) &&
        isNonEmptyString(image.defaultImage.sourceInfo.website.url);
  }

  private getDeprecatedAuthor_(image: UserImage): string {
    assert(
        image && image.defaultImage && image.defaultImage.sourceInfo,
        'only called for deprecated default images with sourceInfo');
    return mojoString16ToString(image.defaultImage.sourceInfo.author);
  }

  private getDeprecatedWebsite_(image: UserImage): string {
    assert(
        image && image.defaultImage && image.defaultImage.sourceInfo,
        'only called for deprecated default images with sourceInfo');
    return image.defaultImage.sourceInfo.website.url;
  }
}

customElements.define(UserPreviewElement.is, UserPreviewElement);
