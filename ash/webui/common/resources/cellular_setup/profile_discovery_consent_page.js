// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Page in eSIM Setup flow that requests user consent to scan for profiles.
 */

import '//resources/cr_elements/cr_shared_style.css.js';
import './base_page.js';

import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './profile_discovery_consent_page.html.js';

Polymer({
  _template: getTemplate(),
  is: 'profile-discovery-consent-page',

  properties: {},
});