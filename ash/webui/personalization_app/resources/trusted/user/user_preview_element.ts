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

import {UserInfo} from '../personalization_app.mojom-webui.js';
import {Paths, PersonalizationRouter} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';

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
      clickable: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
      info_: Object,
      imageUrl_: {
        type: String,
        observer: 'onImageUrlChanged_',
        value: null,
      },
    };
  }

  public clickable: boolean;
  private info_: UserInfo|null;
  private imageUrl_: Url|null;

  override connectedCallback() {
    super.connectedCallback();
    UserImageObserver.initUserImageObserverIfNeeded();
    this.watch<UserPreview['info_']>('info_', state => state.user.info);
    this.watch<UserPreview['imageUrl_']>('imageUrl_', selectUserImageUrl);
    this.updateFromStore();
    initializeUserData(getUserProvider(), this.getStore());
  }

  private onClickUserEmail_() {
    window.open('chrome://os-settings/accountManager');
  }

  private onClickUserSubpageLink_() {
    PersonalizationRouter.instance().goToRoute(Paths.User);
  }

  private onImageUrlChanged_(_: Url|null, old: Url|null): void {
    if (old && old.url.startsWith('blob:')) {
      // Revoke old object urls to clear memory. This is safe to call multiple
      // times.
      URL.revokeObjectURL(old.url);
    }
  }
}

customElements.define(UserPreview.is, UserPreview);
