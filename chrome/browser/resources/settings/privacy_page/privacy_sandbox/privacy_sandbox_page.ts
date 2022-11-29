// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../../route.js';
import {Router} from '../../router.js';

import {getTemplate} from './privacy_sandbox_page.html.js';

export interface SettingsPrivacySandboxPageElement {
  $: {
    privacySandboxAdMeasurementLinkRow: HTMLElement,
    privacySandboxFledgeLinkRow: HTMLElement,
    privacySandboxTopicsLinkRow: HTMLElement,
  };
}

export class SettingsPrivacySandboxPageElement extends PolymerElement {
  static get is() {
    return 'settings-privacy-sandbox-page';
  }

  static get template() {
    return getTemplate();
  }

  private computePrivacySandboxTopicsSublabel_(): string {
    // TODO(b/254412639): Change sublabel based on the respective toggle being
    // enabled.
    return 'Enabled Nulla eros tortor, placerat blandit dictum a, interdum id metus';
  }

  private computePrivacySandboxFledgeSublabel_(): string {
    // TODO(b/254410792): Change sublabel based on the respective toggle being
    // enabled.
    return 'Enabled Duis scelerisque a mi eget ultricies';
  }

  private computePrivacySandboxAdMeasurementSublabel_(): string {
    // TODO(b/254412652): Change sublabel based on the respective toggle being
    // enabled.
    return 'Enabled Vivamus id lacus et lacus porttitor vulputate. Sed semper egestas orci vel maximus.';
  }

  private onPrivacySandboxTopicsClick_() {
    Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX_TOPICS);
  }

  private onPrivacySandboxFledgeClick_() {
    Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX_FLEDGE);
  }

  private onPrivacySandboxAdMeasurementClick_() {
    Router.getInstance().navigateTo(routes.PRIVACY_SANDBOX_AD_MEASUREMENT);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-sandbox-page': SettingsPrivacySandboxPageElement;
  }
}

customElements.define(
    SettingsPrivacySandboxPageElement.is, SettingsPrivacySandboxPageElement);
