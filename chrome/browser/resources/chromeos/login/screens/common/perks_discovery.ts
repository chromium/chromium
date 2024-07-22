// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Polymer element for perks discovery screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './perks_discovery.html.js';

export const PerksDiscoveryElementBase = OobeDialogHostMixin(
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement))));

interface PerkData {
  perkId: string;
  title: string;
  subtitle: string;
  iconUrl: string;
  illustrationUrl: string;
  illustrationWidth: string;
  illustrationHeight: string;
  primaryButtonLabel: string;
  secondaryButtonLabel: string;
}

enum PerksDiscoveryStep {
  LOADING = 'loading',
  OVERVIEW = 'overview',
}

export class PerksDiscoveryElement extends PerksDiscoveryElementBase {
  static get is() {
    return 'perks-discovery-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * List of perks to display.
       */
      perksList: {
        type: Array,
        value: [],
        notify: true,
      },
    };
  }

  private perksList: PerkData[];

  override get UI_STEPS() {
    return PerksDiscoveryStep;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return PerksDiscoveryStep.LOADING;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('PerksDiscoveryScreenScreen');
  }

  override get EXTERNAL_API(): string[] {
    return [
      'setPerksData',
    ];
  }

  setPerksData(perksData: PerkData[]): void {
    assert(perksData !== null);
    this.perksList = perksData;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PerksDiscoveryElement.is]: PerksDiscoveryElement;
  }
}

customElements.define(PerksDiscoveryElement.is, PerksDiscoveryElement);
