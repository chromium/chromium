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
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {UserInfo} from '../personalization_app.mojom-webui.js';
import {Paths, PersonalizationRouter} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {initializeUserData} from '../user/user_controller.js';
import {UserImageObserver} from '../user/user_image_observer.js';
import {getUserProvider} from '../user/user_interface_provider.js';

export class UserPreview extends WithPersonalizationStore {
  static get is() {
    return 'user-preview';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      clickable: {
        type: Boolean,
        value: false,
      },
      image_: Object,
      info_: Object,
    };
  }

  clickable: boolean;
  private image_: Url|null;
  private info_: UserInfo|null;

  connectedCallback() {
    super.connectedCallback();
    UserImageObserver.initUserImageObserverIfNeeded();
    this.watch<UserPreview['image_']>('image_', state => state.user.image);
    this.watch<UserPreview['info_']>('info_', state => state.user.info);
    this.updateFromStore();
    initializeUserData(getUserProvider(), this.getStore());
  }

  private onClickUserEmail_() {
    window.open('chrome://os-settings/accountManager');
  }

  private onClickUserSubpageLink_() {
    PersonalizationRouter.instance().goToRoute(Paths.User);
  }
}

customElements.define(UserPreview.is, UserPreview);
