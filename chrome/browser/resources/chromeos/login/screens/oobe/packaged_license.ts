// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Packaged License screen.
 */

import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {PackagedLicensePageHandlerRemote} from '../../mojom-webui/screens_oobe.mojom-webui.js';
import {OobeScreensFactoryBrowserProxy} from '../../oobe_screens_factory_proxy.js';

import {getTemplate} from './packaged_license.html.js';


export const PackagedLicenseScreenBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));


export class PackagedLicenseScreen extends PackagedLicenseScreenBase {
  static get is() {
    return 'packaged-license-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {};
  }

  private handler: PackagedLicensePageHandlerRemote;

  constructor() {
    super();
    this.handler = new PackagedLicensePageHandlerRemote();
    OobeScreensFactoryBrowserProxy.getInstance()
        .screenFactory.establishPackagedLicenseScreenPipe(
            this.handler.$.bindNewPipeAndPassReceiver());
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('PackagedLicenseScreen');
  }

  /**
   * Returns the control which should receive initial focus.
   */
  override get defaultControl(): HTMLElement|null {
    return this.shadowRoot!.querySelector('#packagedLicenseDialog');
  }

  /**
   * On-tap event handler for Don't Enroll button.
   */
  private onDontEnrollButtonPressed(): void {
    this.handler.onDontEnrollClicked();
  }

  /**
   * On-tap event handler for Enroll button.
   */
  private onEnrollButtonPressed(): void {
    this.handler.onEnrollClicked();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PackagedLicenseScreen.is]: PackagedLicenseScreen;
  }
}

customElements.define(PackagedLicenseScreen.is, PackagedLicenseScreen);
