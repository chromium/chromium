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
    profileContainer: HTMLButtonElement,
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
  private isOpeningMenu_: boolean = false;

  get isMenuOpen(): boolean {
    return this.isOpeningMenu_ || this.$.profileMenu.open;
  }

  // Closes the unbounded menu when Loomnibox hides in place (keeping the DOM
  // connected without firing disconnectedCallback).
  private onVisibilityChange_ = () => {
    if (document.hidden && this.isMenuOpen) {
      this.$.profileMenu.close();
    }
  };

  override connectedCallback() {
    super.connectedCallback();
    this.updateProfileInfoListenerId_ =
        this.callbackRouter_.updateProfileInfo.addListener(
            (avatarUrl: Url, name: string, email: string) => {
              this.profileAvatarUrl_ = avatarUrl;
              this.profileName_ = name;
              this.profileEmail_ = email;
            });
    document.addEventListener('visibilitychange', this.onVisibilityChange_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    // Closes the unbounded menu when <omnibox-everywhere-omnibox> unmounts
    // while the document remains visible (e.g. switching to Composebox).
    if (this.isMenuOpen) {
      this.$.profileMenu.close();
    }
    if (this.updateProfileInfoListenerId_ !== null) {
      this.callbackRouter_.removeListener(this.updateProfileInfoListenerId_);
      this.updateProfileInfoListenerId_ = null;
    }
    document.removeEventListener('visibilitychange', this.onVisibilityChange_);
  }

  protected getProfileTooltip_(): string {
    return [
      this.i18n('profileTooltipHeader'),
      this.profileName_,
      this.profileEmail_,
    ].filter(Boolean)
        .join('\n');
  }

  protected getProfileAriaLabel_(): string {
    return [
      this.i18n('profileTooltipHeader'),
      this.profileName_,
      this.profileEmail_,
    ].filter(Boolean)
        .join(' ');
  }

  protected async onProfileIconClick_() {
    if (!this.profilePickerEnabled_) {
      return;
    }
    if (this.isMenuOpen) {
      this.$.profileMenu.close();
      return;
    }
    await this.$.profileMenu.setUnbounded();
    if (!this.isConnected || this.isMenuOpen) {
      return;
    }
    this.isOpeningMenu_ = true;
    try {
      this.$.profileMenu.showAt(this.$.profileContainer, {
        anchorAlignmentX: AnchorAlignment.BEFORE_END,
        anchorAlignmentY: AnchorAlignment.AFTER_END,
      });
    } finally {
      this.isOpeningMenu_ = false;
    }
  }

  protected onSwitchProfileClick_() {
    this.searchboxHandler_.openProfilePicker();
    this.$.profileMenu.close();
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
