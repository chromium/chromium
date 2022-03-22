// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/chromeos/smb_shares/add_smb_share_dialog.js';
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import '//resources/js/action_link.js';
import '//resources/cr_elements/action_link_css.m.js';
import '//resources/cr_components/localized_link/localized_link.js';
import '../../settings_shared_css.js';
import '../../settings_vars_css.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-smb-shares-page',

  behaviors: [
    RouteObserverBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    showAddSmbDialog_: Boolean,
  },

  /**
   * Overridden from RouteObserverBehavior.
   * @param {!Route} route
   * @protected
   */
  currentRouteChanged(route) {
    if (route === routes.SMB_SHARES) {
      this.showAddSmbDialog_ = Router.getInstance().getQueryParameters().get(
                                   'showAddShare') === 'true';
    }
  },

  /** @private */
  onAddShareTap_() {
    this.showAddSmbDialog_ = true;
  },

  /** @private */
  onAddSmbDialogClosed_() {
    this.showAddSmbDialog_ = false;
  },
});
