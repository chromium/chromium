// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'edit-hostname-dialog' is a component allowing the
 * user to edit the device hostname.
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/cr_elements/cr_input/cr_input.m.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import '//resources/polymer/v3_0/iron-selector/iron-selector.js';
import '../../settings_shared_css.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AboutPageBrowserProxy, AboutPageBrowserProxyImpl, AboutPageUpdateInfo, BrowserChannel, browserChannelToI18nId, ChannelInfo, isTargetChannelMoreStable, RegulatoryInfo, TPMFirmwareUpdateStatusChangedEvent, UpdateStatus, UpdateStatusChangedEvent, VersionInfo} from './about_page_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'edit-hostname-dialog',

  /** @private */
  onCancelTap_() {
    this.$.dialog.close();
  },

  /** @private */
  onDoneTap_() {
    this.$.dialog.close();
  },
});
