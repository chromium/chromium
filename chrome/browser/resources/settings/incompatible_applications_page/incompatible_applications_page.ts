// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-incompatible-applications-page' is the settings subpage containing
 * the list of incompatible applications.
 */

import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../settings_shared.css.js';
import './incompatible_application_item.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import type {IncompatibleApplication} from './incompatible_applications_browser_proxy.js';
import {IncompatibleApplicationsBrowserProxyImpl} from './incompatible_applications_browser_proxy.js';
import {getTemplate} from './incompatible_applications_page.html.js';

const SettingsIncompatibleApplicationsPageElementBase =
    WebUiListenerMixin(PolymerElement);

export class SettingsIncompatibleApplicationsPageElement extends
    SettingsIncompatibleApplicationsPageElementBase {
  static get is() {
    return 'settings-incompatible-applications-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Indicates if the current user has administrator rights.
       */
      hasAdminRights_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('hasAdminRights');
        },
      },

      /**
       * The list of all the incompatible applications.
       */
      applications_: Array,

      /**
       * Determines if the user has finished with this page.
       */
      isDone_: {
        type: Boolean,
        computed: 'computeIsDone_(applications_.*)',
      },

      /**
       * The text for the subtitle of the subpage.
       */
      subtitleText_: {
        type: String,
        value: '',
      },

      /**
       * The text for the subtitle of the subpage, when the user does not have
       * administrator rights.
       */
      subtitleNoAdminRightsText_: {
        type: String,
        value: '',
      },

      /**
       * The text for the title of the list of incompatible applications.
       */
      listTitleText_: {
        type: String,
        value: '',
      },
    };
  }

  private hasAdminRights_: boolean;
  private applications_: IncompatibleApplication[];
  private isDone_: boolean;
  private subtitleText_: string;
  private subtitleNoAdminRightsText_: string;
  private listTitleText_: string;

  override ready() {
    super.ready();

    this.addWebUiListener(
        'incompatible-application-removed',
        (applicationName: string) =>
            this.onIncompatibleApplicationRemoved_(applicationName));

    IncompatibleApplicationsBrowserProxyImpl.getInstance()
        .requestIncompatibleApplicationsList()
        .then(list => {
          this.applications_ = list;
          this.updatePluralStrings_();
        });
  }

  private computeIsDone_(): boolean {
    return this.applications_.length === 0;
  }

  /**
   * Removes a single incompatible application from the |applications_| list.
   */
  private onIncompatibleApplicationRemoved_(applicationName: string) {
    // Find the index of the element.
    const index = this.applications_.findIndex(function(application) {
      return application.name === applicationName;
    });

    assert(index !== -1);

    this.splice('applications_', index, 1);
  }

  /**
   * Updates the texts of the Incompatible Applications subpage that depends on
   * the length of |applications_|.
   */
  private updatePluralStrings_() {
    const browserProxy = IncompatibleApplicationsBrowserProxyImpl.getInstance();
    const numApplications = this.applications_.length;

    // The plural strings are not displayed when there is no applications.
    if (this.applications_.length === 0) {
      return;
    }

    Promise
        .all([
          browserProxy.getSubtitlePluralString(numApplications),
          browserProxy.getSubtitleNoAdminRightsPluralString(numApplications),
          browserProxy.getListTitlePluralString(numApplications),
        ])
        .then(strings => {
          this.subtitleText_ = strings[0];
          this.subtitleNoAdminRightsText_ = strings[1];
          this.listTitleText_ = strings[2];
        });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-incompatible-applications-page':
        SettingsIncompatibleApplicationsPageElement;
  }
}

customElements.define(
    SettingsIncompatibleApplicationsPageElement.is,
    SettingsIncompatibleApplicationsPageElement);
