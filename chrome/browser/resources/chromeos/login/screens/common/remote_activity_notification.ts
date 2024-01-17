// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for remote activity notification screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import { mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';

import {getTemplate} from './remote_activity_notification.html.js';

const RemoteActivityNotificationBase =
  mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    PolymerElement) as {
      new (): PolymerElement & OobeI18nBehaviorInterface &
          LoginScreenBehaviorInterface & OobeDialogHostBehaviorInterface,
  };

class RemoteActivityNotification extends RemoteActivityNotificationBase {
  static get is() {
    return 'remote-activity-notification-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {};
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('RemoteActivityNotificationScreen');
  }

  /** Initial UI State for screen */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OOBE_UI_STATE {
    return OOBE_UI_STATE.BLOCKING;
  }

  onContinueUsingDevice(): void {
    this.userActed('continue-using-device');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [RemoteActivityNotification.is]: RemoteActivityNotification;
  }
}

customElements.define(
    RemoteActivityNotification.is, RemoteActivityNotification);
