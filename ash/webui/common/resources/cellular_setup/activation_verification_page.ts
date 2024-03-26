// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This page is displayed when the activation code is being verified, and
 * an ESim profile is being installed.
 */
import './base_page.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '//resources/cros_components/lottie_renderer/lottie-renderer.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './activation_verification_page.html.js';

const ActivationVerificationPageElementBase = I18nMixin(PolymerElement);

export class ActivationVerificationPageElement extends
    ActivationVerificationPageElementBase {
  static get is() {
    return 'activation-verification-page' as const;
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(
    ActivationVerificationPageElement.is, ActivationVerificationPageElement);
