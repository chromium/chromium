// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-guide-ad-topics-fragment' is the fragment in a privacy guide
 * card that contains the ad topics setting and its description.
 */

import '../../controls/settings_toggle_button.js';
import '../../icons.html.js';
import './privacy_guide_description_item.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {PrivacySandboxBrowserProxyImpl} from '../../privacy_sandbox/privacy_sandbox_browser_proxy.js';

import {getTemplate} from './privacy_guide_ad_topics_fragment.html.js';

const PrivacyGuideAdTopicsFragmentElementBase = PrefsMixin(PolymerElement);

export class PrivacyGuideAdTopicsFragmentElement extends
    PrivacyGuideAdTopicsFragmentElementBase {
  static get is() {
    return 'privacy-guide-ad-topics-fragment';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }

  override focus() {
    // The fragment element is focused when it becomes visible. Move the focus
    // to the fragment header, so that the newly shown content of the fragment
    // is downwards from the focus position. This allows users of screen readers
    // to continue navigating the screen reader position downwards through the
    // newly visible content.
    this.shadowRoot!.querySelector<HTMLElement>('[focus-element]')!.focus();
  }

  private onToggleChange_(e: Event) {
    const target = e.target as SettingsToggleButtonElement;
    PrivacySandboxBrowserProxyImpl.getInstance().topicsToggleChanged(
        target.checked);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-guide-ad-topics-fragment': PrivacyGuideAdTopicsFragmentElement;
  }
}

customElements.define(
    PrivacyGuideAdTopicsFragmentElement.is,
    PrivacyGuideAdTopicsFragmentElement);
