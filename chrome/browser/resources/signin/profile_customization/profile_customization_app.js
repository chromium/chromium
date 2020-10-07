// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/customize_themes/customize_themes.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import './strings.m.js';
import './signin_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ProfileCustomizationBrowserProxy, ProfileCustomizationBrowserProxyImpl} from './profile_customization_browser_proxy.js';

Polymer({
  is: 'profile-customization-app',

  _template: html`{__html_template__}`,

  /** @private {?ProfileCustomizationBrowserProxy} */
  profileCustomizationBrowserProxy_: null,

  /** @override */
  ready() {
    this.profileCustomizationBrowserProxy_ =
        ProfileCustomizationBrowserProxyImpl.getInstance();
  },

  /** @private */
  onDoneCustomizationClicked_() {
    this.profileCustomizationBrowserProxy_.done();
  },
});
