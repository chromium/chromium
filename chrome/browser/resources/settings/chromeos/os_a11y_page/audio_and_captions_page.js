// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-audio-and-captions-page' is the accessibility settings subpage for
 * audio and captions accessibility settings.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../../controls/settings_slider.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteOriginBehavior, RouteOriginBehaviorImpl, RouteOriginBehaviorInterface} from '../route_origin_behavior.js';

import {AudioAndCaptionsPageBrowserProxy, AudioAndCaptionsPageBrowserProxyImpl} from './audio_and_captions_page_browser_proxy.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteOriginBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsAudioAndCaptionsPageElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      I18nBehavior,
      RouteOriginBehavior,
      WebUIListenerBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsAudioAndCaptionsPageElement extends
    SettingsAudioAndCaptionsPageElementBase {
  static get is() {
    return 'settings-audio-and-captions-page';
  }

  static get template() {
    return html`{__html_template__}`;
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

      /**
       * Whether the user is in kiosk mode.
       * @protected
       */
      isKioskModeActive_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isKioskModeActive');
        },
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kMonoAudio,
          Setting.kStartupSound,
        ]),
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** RouteOriginBehavior override */
    this.route_ = routes.A11Y_AUDIO_AND_CAPTIONS;

    /** @private {!AudioAndCaptionsPageBrowserProxy} */
    this.audioAndCaptionsBrowserProxy_ =
        AudioAndCaptionsPageBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();

    this.addWebUIListener(
        'initial-data-ready',
        (startupSoundEnabled) =>
            this.onAudioAndCaptionsPageReady_(startupSoundEnabled));
    this.audioAndCaptionsBrowserProxy_.audioAndCaptionsPageReady();

    this.addFocusConfig(
        routes.MANAGE_CAPTION_SETTINGS, '#captionsSubpageButton');
  }

  /**
   * Note: Overrides RouteOriginBehavior implementation
   * @param {!Route} newRoute
   * @param {!Route=} prevRoute
   * @protected
   */
  currentRouteChanged(newRoute, prevRoute) {
    RouteOriginBehaviorImpl.currentRouteChanged.call(this, newRoute, prevRoute);

    // Does not apply to this page.
    if (newRoute !== routes.A11Y_AUDIO_AND_CAPTIONS) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * @param {!CustomEvent<boolean>} e
   * @private
   */
  toggleStartupSoundEnabled_(e) {
    this.audioAndCaptionsBrowserProxy_.setStartupSoundEnabled(e.detail);
  }

  /** @private */
  onCaptionsClick_() {
    Router.getInstance().navigateTo(routes.MANAGE_CAPTION_SETTINGS);
  }

  /** @private */
  onMouseTap_() {
    Router.getInstance().navigateTo(
        routes.POINTERS,
        /* dynamicParams */ null, /* removeSearch */ true);
  }

  /**
   * Handles updating the visibility of the shelf navigation buttons setting
   * and updating whether startupSoundEnabled is checked.
   * @param {boolean} startupSoundEnabled Whether startup sound is enabled.
   * @private
   */
  onAudioAndCaptionsPageReady_(startupSoundEnabled) {
    this.$.startupSoundEnabled.checked = startupSoundEnabled;
  }
}

customElements.define(
    SettingsAudioAndCaptionsPageElement.is,
    SettingsAudioAndCaptionsPageElement);
