// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';
import '../os_settings_page/os_settings_animated_pages.js';
import './facegaze_actions_card.js';
import './facegaze_cursor_card.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import type {DomRepeat} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import type {Route} from '../router.js';
import {routes} from '../router.js';

import {getTemplate} from './facegaze_subpage.html.js';
import type {FaceGazeSubpageBrowserProxy} from './facegaze_subpage_browser_proxy.js';
import {FaceGazeSubpageBrowserProxyImpl} from './facegaze_subpage_browser_proxy.js';


const SettingsFaceGazeSubpageElementBase = DeepLinkingMixin(RouteObserverMixin(
    WebUiListenerMixin(PrefsMixin(I18nMixin(PolymerElement)))));

export interface SettingsFaceGazeSubpageElement {
  $: {
    faceGazeToggle: SettingsToggleButtonElement,
    recognitionConfidenceRepeat: DomRepeat,
  };
}

export class SettingsFaceGazeSubpageElement extends
    SettingsFaceGazeSubpageElementBase {
  static get is() {
    return 'settings-facegaze-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      toggleLabel_: {
        type: String,
        computed:
            'getToggleLabel_(prefs.settings.a11y.face_gaze.enabled.value)',
      },
    };
  }

  private toggleLabel_: string;
  private faceGazeSubpageBrowserProxy_: FaceGazeSubpageBrowserProxy;

  constructor() {
    super();
    this.faceGazeSubpageBrowserProxy_ =
        FaceGazeSubpageBrowserProxyImpl.getInstance();
    this.addWebUiListener(
        'settings.handleDisableDialogResult',
        (accepted: boolean) => this.onDisableDialogResult_(accepted));
  }

  override ready(): void {
    super.ready();
    this.$.faceGazeToggle.checked =
        this.prefs.settings.a11y.face_gaze.enabled.value;
  }

  // DeepLinkingMixin override
  override supportedSettingIds = new Set<Setting>([
    Setting.kFaceGaze,
  ]);

  private getToggleLabel_(): string {
    return this.getPref('settings.a11y.face_gaze.enabled').value ?
        this.i18n('deviceOn') :
        this.i18n('deviceOff');
  }

  private onFaceGazeToggleClicked_(): void {
    this.faceGazeSubpageBrowserProxy_.requestEnableFaceGaze(
        this.$.faceGazeToggle.checked);
  }

  private onDisableDialogResult_(accepted: boolean): void {
    this.$.faceGazeToggle.checked = !accepted;
  }

  static get observers() {
    return [
      'onFaceGazeEnabledChanged_(prefs.settings.a11y.face_gaze.enabled.value)',
    ];
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.MANAGE_FACEGAZE_SETTINGS) {
      return;
    }

    this.attemptDeepLink();
  }

  private onFaceGazeEnabledChanged_() {
    // The pref will have been changed by the common extension to the correct
    // value at this point, so reset to the pref value.
    this.$.faceGazeToggle.resetToPrefValue();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsFaceGazeSubpageElement.is]: SettingsFaceGazeSubpageElement;
  }
}

customElements.define(
    SettingsFaceGazeSubpageElement.is, SettingsFaceGazeSubpageElement);
