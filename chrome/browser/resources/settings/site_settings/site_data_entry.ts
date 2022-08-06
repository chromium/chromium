// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-data-entry' handles showing the local storage summary for a site.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import {FocusRowBehavior} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/js/i18n_mixin.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';

import {LocalDataBrowserProxyImpl, LocalDataItem} from './local_data_browser_proxy.js';
import {getTemplate} from './site_data_entry.html.js';

const SiteDataEntryElementBase =
    mixinBehaviors([FocusRowBehavior], I18nMixin(PolymerElement)) as
    {new (): PolymerElement & I18nMixinInterface};

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
