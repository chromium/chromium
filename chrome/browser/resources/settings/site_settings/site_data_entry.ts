// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-data-entry' handles showing the local storage summary for a site.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import {FocusRowMixin} from 'chrome://resources/js/cr/ui/focus_row_mixin.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';

import {LocalDataBrowserProxyImpl, LocalDataItem} from './local_data_browser_proxy.js';
import {getTemplate} from './site_data_entry.html.js';

const SiteDataEntryElementBase = FocusRowMixin(I18nMixin(PolymerElement));

class SiteDataEntryElement extends SiteDataEntryElementBase {
  static get is() {
    return 'site-data-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      model: Object,
    };
  }

  model: LocalDataItem;

  /**
   * Deletes all site data for this site.
   */
  private onRemove_(e: Event) {
    e.stopPropagation();
    MetricsBrowserProxyImpl.getInstance().recordSettingsPageHistogram(
        PrivacyElementInteractions.SITE_DATA_REMOVE_SITE);
    LocalDataBrowserProxyImpl.getInstance().removeSite(this.model.site);
  }
}

customElements.define(SiteDataEntryElement.is, SiteDataEntryElement);
