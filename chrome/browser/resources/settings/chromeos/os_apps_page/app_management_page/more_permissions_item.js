// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_style.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';

import {AppManagementUserAction} from '//resources/cr_components/app_management/constants.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {recordAppManagementUserAction} from 'chrome://resources/cr_components/app_management/util.js';

import {BrowserProxy} from './browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'app-management-more-permissions-item',

  properties: {
    /** @type {!App} */
    app: Object,
  },

  listeners: {
    click: 'onClick_',
  },

  onClick_() {
    BrowserProxy.getInstance().handler.openNativeSettings(this.app.id);
    recordAppManagementUserAction(
        this.app.type, AppManagementUserAction.NativeSettingsOpened);
  },
});
