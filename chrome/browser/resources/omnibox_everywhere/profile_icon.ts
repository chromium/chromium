// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';

import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PageCallbackRouter, PageHandlerInterface as SearchboxPageHandlerInterface} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './profile_icon.css.js';
import {getHtml} from './profile_icon.html.js';

export interface OmniboxEverywhereProfileIconElement {
  $: {
    profileMenu: CrActionMenuElement,
  };
}

const OmniboxEverywhereProfileIconElementBase = I18nMixinLit(CrLitElement);

export class OmniboxEverywhereProfileIconElement extends
    OmniboxEverywhereProfileIconElementBase {
  static get is() {
    return 'omnibox-everywhere-profile-icon';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isEnterpriseProfile_: {
        type: Boolean,
        reflect: true,
        attribute: 'is-enterprise-profile',
      },
      profileAvatarUrl_: {type: String},
      profileName_: {type: String},
      profileEmail_: {type: String},
      profilePickerEnabled_: {type: Boolean},
    };
  }

  protected accessor isEnterpriseProfile_: boolean =
      loadTimeData.getBoolean('isEnterpriseProfile');
  protected accessor profileAvatarUrl_: string =
      loadTimeData.getString('profileAvatarUrl');
  protected accessor profileName_: string =
      loadTimeData.getString('profileName');
  protected accessor profileEmail_: string =
      loadTimeData.getString('profileEmail');
  protected accessor profilePickerEnabled_: boolean =
      loadTimeData.getBoolean('omniboxEverywhereProfilePickerEnabled');

  private searchboxHandler_: SearchboxPageHandlerInterface =
      SearchboxBrowserProxy.getInstance().handler;
  private callbackRouter_: PageCallbackRouter =
      SearchboxBrowserProxy.getInstance().callbackRouter;
  private updateProfileInfoListenerId_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.updateProfileInfoListenerId_ =
        this.callbackRouter_.updateProfileInfo.addListener(
            (avatarUrl: Url, name: string, email: string) => {
              this.profileAvatarUrl_ = avatarUrl;
              this.profileName_ = name;
              this.profileEmail_ = email;
            });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.updateProfileInfoListenerId_ !== null) {
      this.callbackRouter_.removeListener(this.updateProfileInfoListenerId_);
      this.updateProfileInfoListenerId_ = null;
    }
  }

  protected getProfileTooltip_(): string {
    if (this.profileName_ && this.profileEmail_) {
      return `${this.profileName_}\n${this.profileEmail_}`;
    }
    return this.profileName_ || this.profileEmail_ ||
        this.i18n('profileButtonLabel');
  }

  protected onProfileIconClick_() {
    if (!this.profilePickerEnabled_) {
      return;
    }
    const anchor =
        this.shadowRoot?.querySelector<HTMLElement>('#profileContainer');
    if (anchor && this.$.profileMenu) {
      this.$.profileMenu.showAt(anchor, {
        anchorAlignmentX: AnchorAlignment.BEFORE_END,
        anchorAlignmentY: AnchorAlignment.AFTER_END,
      });
    }
  }

  protected onSwitchProfileClick_() {
    if (this.$.profileMenu) {
      this.$.profileMenu.close();
    }
    this.searchboxHandler_.openProfilePicker();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-everywhere-profile-icon': OmniboxEverywhereProfileIconElement;
  }
}

customElements.define(
    OmniboxEverywhereProfileIconElement.is,
    OmniboxEverywhereProfileIconElement);
