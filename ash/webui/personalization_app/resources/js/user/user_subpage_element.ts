// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The user-subpage component displays information about the
 * current user and allows changing device avatar image.
 */

import {PersonalizationRouterElement} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {UserImageObserver} from './user_image_observer.js';
import {getTemplate} from './user_subpage_element.html.js';

export class UserSubpageElement extends WithPersonalizationStore {
  static get is() {
    return 'user-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      path: String,
      isUserImageEnterpriseManaged_: {
        type: Boolean,
        value: null,
        observer: 'onUserImageIsEnterpriseManagedChanged_',
      },
    };
  }

  path: string;
  private isUserImageEnterpriseManaged_: boolean|null;

  override connectedCallback() {
    super.connectedCallback();
    UserImageObserver.initUserImageObserverIfNeeded();
    this.watch<UserSubpageElement['isUserImageEnterpriseManaged_']>(
        'isUserImageEnterpriseManaged_',
        state => state.user.imageIsEnterpriseManaged);
    this.updateFromStore();
  }

  private onUserImageIsEnterpriseManagedChanged_(isUserImageEnterpriseManaged:
                                                     boolean|null) {
    if (isUserImageEnterpriseManaged) {
      // This page should not be accessible if the image is enterprise managed.
      PersonalizationRouterElement.reloadAtRoot();
    }
  }

  private isNotEnterpriseManaged_(isEnterpriseManaged: boolean|null): boolean {
    // Specifically exclude null.
    return isEnterpriseManaged === false;
  }
}

customElements.define(UserSubpageElement.is, UserSubpageElement);
