// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The avatar-list component displays the list of avatar images
 * that the user can select from.
 */

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isSelectionEvent} from '../../common/utils.js';
import {DefaultUserImage} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {fetchDefaultUserImages} from './user_controller.js';
import {getUserProvider} from './user_interface_provider.js';

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
    };
  }

  defaultUserImages_: Array<DefaultUserImage>|null;

  connectedCallback() {
    super.connectedCallback();
    this.watch<AvatarList['defaultUserImages_']>(
        'defaultUserImages_', state => state.user.defaultUserImages);
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
}

customElements.define(AvatarList.is, AvatarList);
