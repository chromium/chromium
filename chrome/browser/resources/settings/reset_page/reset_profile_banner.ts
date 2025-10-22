// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-reset-profile-banner' is the banner shown for prompting the user to
 * clear profile settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../settings_shared.css.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {Router} from '../router.js';

import type {ResetBrowserProxy} from './reset_browser_proxy.js';
import {ResetBrowserProxyImpl} from './reset_browser_proxy.js';
import {getTemplate} from './reset_profile_banner.html.js';

export interface SettingsResetProfileBannerElement {
  $: {
    dialog: CrDialogElement,
    ok: HTMLElement,
    reset: HTMLElement,
  };
}

const SettingsResetProfileBannerElementBase = I18nMixin(PolymerElement);

export class SettingsResetProfileBannerElement extends
    SettingsResetProfileBannerElementBase {
  static get is() {
    return 'settings-reset-profile-banner';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      showResetProfileBannerV2: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showResetProfileBannerV2'),
      },
      tamperedPrefs: {
        type: Array,
        value: () => [],
      },
      showTamperedPrefsList: {
        type: Boolean,
        value: false,
      },
    };
  }

  declare showResetProfileBannerV2: boolean;
  declare tamperedPrefs: string[];
  declare showTamperedPrefsList: boolean;

  private browserProxy_: ResetBrowserProxy =
      ResetBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    if (this.showResetProfileBannerV2) {
      this.browserProxy_.getTamperedPreferencePaths().then(prefs => {
        if (prefs.length > 0) {
          this.tamperedPrefs = prefs;
          this.showTamperedPrefsList = true;
          this.$.dialog.showModal();
          this.browserProxy_.onShowResetProfileDialog();
        }
      });
    } else {
      this.$.dialog.showModal();
      this.browserProxy_.onShowResetProfileDialog();
    }
  }

  private onOkClick_() {
    this.$.dialog.close();
    this.browserProxy_.onHideResetProfileBanner();
  }

  private onCancel_() {
    this.browserProxy_.onHideResetProfileBanner();
  }

  private onResetClick_() {
    this.$.dialog.close();
    Router.getInstance().navigateTo(routes.RESET_DIALOG);
  }

  private onConfirmClick_() {
    this.$.dialog.close();
    this.browserProxy_.onHideResetProfileBanner();
  }

  private onLearnMoreClick_() {
    window.open(this.i18n('resetProfileBannerLearnMoreUrl'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-reset-profile-banner': SettingsResetProfileBannerElement;
  }
}

customElements.define(
    SettingsResetProfileBannerElement.is, SettingsResetProfileBannerElement);
