// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for lacros data backward migration screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/oobe_icons.html.js';
import '../../components/oobe_slide.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nMixin, OobeI18nMixinInterface} from '../../components/mixins/oobe_i18n_mixin.js';
import {LacrosDataBackwardMigrationPageCallbackRouter, LacrosDataBackwardMigrationPageHandlerRemote} from '../../mojom-webui/screens_login.mojom-webui.js';
import {OobeScreensFacotryBrowserProxy} from '../../oobe_screens_factory_proxy.js';

import {getTemplate} from './lacros_data_backward_migration.html.js';

enum LacrosDataBackwardMigrationStep {
  PROGRESS = 'progress',
  ERROR = 'error',
}

const LacrosDataBackwardMigrationScreenElementBase = mixinBehaviors(
    [
      OobeDialogHostBehavior,
      LoginScreenBehavior,
      MultiStepBehavior,
    ],
    OobeI18nMixin(PolymerElement)) as { new (): PolymerElement
      & OobeDialogHostBehaviorInterface
      & OobeI18nMixinInterface
      & LoginScreenBehaviorInterface
      & MultiStepBehaviorInterface,
  };

export class LacrosDataBackwardMigrationScreen extends
    LacrosDataBackwardMigrationScreenElementBase {
  static get is() {
    return 'lacros-data-backward-migration-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      progressValue: {
        type: Number,
        value: 0,
      },
    };
  }

  private progressValue: number;
  private callbackRouter: LacrosDataBackwardMigrationPageCallbackRouter;
  private handler: LacrosDataBackwardMigrationPageHandlerRemote;

  constructor() {
    super();
    this.callbackRouter = new LacrosDataBackwardMigrationPageCallbackRouter();
    this.handler = new LacrosDataBackwardMigrationPageHandlerRemote();
    OobeScreensFacotryBrowserProxy.getInstance()
        .screenFactory.createLacrosDataBackwardMigrationScreenHandler(
            this.callbackRouter.$.bindNewPipeAndPassRemote(),
            this.handler.$.bindNewPipeAndPassReceiver());
    this.callbackRouter.setProgressValue.addListener(
        this.setProgressValue.bind(this));
    this.callbackRouter.setFailureStatus.addListener(
        this.setFailureStatus.bind(this));
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): string {
    return LacrosDataBackwardMigrationStep.PROGRESS;
  }

  override get UI_STEPS() {
    return LacrosDataBackwardMigrationStep;
  }


  /**
   * Called when the migration failed.
  */
  setFailureStatus(): void {
    this.setUIStep('error');
  }

  /**
   * Called to update the progress of data migration.
   * @param progress Percentage of data copied so far.
   */
  setProgressValue(progress: number): void {
    this.progressValue = progress;
  }

  override ready() {
    super.ready();
    this.initializeLoginScreen('LacrosDataBackwardMigrationScreen');
  }

  private onCancelButtonClicked() {
    this.handler.onCancelButtonClicked();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [LacrosDataBackwardMigrationScreen.is]: LacrosDataBackwardMigrationScreen;
  }
}

customElements.define(
    LacrosDataBackwardMigrationScreen.is, LacrosDataBackwardMigrationScreen);
