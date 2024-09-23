// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {LocalDataLossWarningPageHandlerRemote} from '../../mojom-webui/screens_osauth.mojom-webui.js';
import {OobeScreensFactoryBrowserProxy} from '../../oobe_screens_factory_proxy.js';

import {getTemplate} from './local_data_loss_warning.html.js';


const LocalDataLossWarningBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

/**
 * Data that is passed to the screen during onBeforeShow.
 */
interface LocalDataLossWarningScreenData {
  isOwner: boolean;
  email: string;
  canGoBack: boolean;
}

export class LocalDataLossWarning extends LocalDataLossWarningBase {
  static get is() {
    return 'local-data-loss-warning-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      email: {
        type: String,
        value: '',
      },

      disabled: {
        type: Boolean,
      },

      isOwner: {
        type: Boolean,
      },

      canGoBack: {
        type: Boolean,
      },
    };
  }

  private email:string;
  private disabled : boolean;
  private isOwner : boolean;
  private canGoBack : boolean;
  private handler: LocalDataLossWarningPageHandlerRemote;


  constructor() {
    super();
    this.disabled = false;
    this.handler = new LocalDataLossWarningPageHandlerRemote();
    OobeScreensFactoryBrowserProxy.getInstance()
        .screenFactory.establishLocalDataLossWarningScreenPipe(
            this.handler.$.bindNewPipeAndPassReceiver());
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('LocalDataLossWarningScreen');
  }

  /** Initial UI State for screen */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.PASSWORD_CHANGED;
  }

  /**
   * Invoked just before being shown. Contains all the data for the screen.
   */
  override onBeforeShow(data: LocalDataLossWarningScreenData) : void {
    super.onBeforeShow(data);
    this.isOwner = data['isOwner'];
    this.email = data['email'];
    this.canGoBack = data['canGoBack'];
  }

  /**
   * Returns the subtitle message for the data loss warning screen.
   * @param locale The i18n locale.
   * @param email The email address that the user is trying to recover.
   * @return The translated subtitle message.
   */
  private getDataLossWarningSubtitleMessage(locale: string, email: string):
      TrustedHTML {
    return this.i18nAdvancedDynamic(
        locale, 'dataLossWarningSubtitle', {substitutions: [email]});
  }

  private onProceedClicked() : void {
    if (this.disabled) {
      return;
    }
    this.disabled = true;
    this.handler.onRecreateUser();
  }

  private onResetClicked(): void {
    if (this.disabled) {
      return;
    }
    this.disabled = true;
    this.handler.onPowerwash();
  }

  private onBackButtonClicked(): void {
    if (this.disabled) {
      return;
    }
    this.handler.onBack();
  }

  private onCancelClicked() : void {
    if (this.disabled) {
      return;
    }
    this.handler.onCancel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [LocalDataLossWarning.is]: LocalDataLossWarning;
  }
}


customElements.define(LocalDataLossWarning.is, LocalDataLossWarning);
