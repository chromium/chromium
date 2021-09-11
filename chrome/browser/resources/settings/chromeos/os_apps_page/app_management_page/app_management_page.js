// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Load api_listener after other assets have initialized.

import './api_listener.js';
import './main_view.js';
import '../../../settings_shared_css.js';

import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {updateSelectedAppId} from './actions.js';
import {BrowserProxy} from './browser_proxy.js';
import {AppManagementStore} from './store.js';
import {AppManagementStoreClient} from './store_client.js';


Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-app-management-page',

  properties: {
    /**
     * @type {string}
     */
    searchTerm: String,
  },
});
