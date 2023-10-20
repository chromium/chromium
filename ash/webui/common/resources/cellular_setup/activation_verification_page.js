// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This page is displayed when the activation code is being verified, and
 * an ESim profile is being installed.
 */
import './base_page.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import 'chrome://resources/cros_components/lottie_renderer/lottie-renderer.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './activation_verification_page.html.js';

Polymer({
  _template: getTemplate(),
  is: 'activation-verification-page',

  behaviors: [I18nBehavior],
});
