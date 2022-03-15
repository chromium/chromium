// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './strings.m.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './site_permissions_edit_permissions_dialog.html.js';
import {SiteSettingsDelegate} from './site_settings_mixin.js';

export interface SitePermissionsEditPermissionsDialogElement {
  $: {
    dialog: CrDialogElement,
    submit: CrButtonElement,
  };
}

const SitePermissionsEditPermissionsDialogElementBase =
    I18nMixin(PolymerElement);

export class SitePermissionsEditPermissionsDialogElement extends
    SitePermissionsEditPermissionsDialogElementBase {
  static get is() {
    return 'site-permissions-edit-permissions-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      delegate: Object,

      /**
       * The current siteSet for `site`, as stored in the backend. Specifies
       * whether `site` is a user specified permitted or restricted site.
       */
      originalSiteSet: String,

      /**
       * The url of the site whose permissions are currently being edited.
       */
      site: String,

      /**
       * The temporary siteSet for `site` as displayed in the dialog. Will be
       * saved to the backend when the dialog is submitted.
       */
      siteSet_: String,

      userSiteSetEnum_: {
        type: Object,
        value: chrome.developerPrivate.UserSiteSet,
      },
    };
  }

  delegate: SiteSettingsDelegate;
  originalSiteSet: chrome.developerPrivate.UserSiteSet;
  site: string;
  private siteSet_: chrome.developerPrivate.UserSiteSet;

  override connectedCallback() {
    super.connectedCallback();
    this.siteSet_ = this.originalSiteSet;
  }

  private onCancelClick_() {
    this.$.dialog.cancel();
  }

  private onSubmitClick_() {
    if (this.siteSet_ === this.originalSiteSet) {
      this.$.dialog.close();
      return;
    }

    this.delegate.addUserSpecifiedSites(this.siteSet_, [this.site]).then(() => {
      this.$.dialog.close();
    });
  }

  private computeDialogTitle_(): string {
    return this.i18n('sitePermissionsEditPermissionsDialogTitle', this.site);
  }

  private getPermittedSiteLabel_(): string {
    return this.i18n('editSitePermissionsAllowAllExtensions', this.site);
  }

  private getRestrictedSiteLabel_(): string {
    return this.i18n('editSitePermissionsRestrictExtensions', this.site);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-permissions-edit-permissions-dialog':
        SitePermissionsEditPermissionsDialogElement;
  }
}

customElements.define(
    SitePermissionsEditPermissionsDialogElement.is,
    SitePermissionsEditPermissionsDialogElement);
