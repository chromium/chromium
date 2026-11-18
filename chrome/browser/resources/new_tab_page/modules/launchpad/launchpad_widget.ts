// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {I18nMixinLit, loadTimeData} from '../../i18n_setup.js';
import {ModuleDescriptor} from '../module_descriptor.js';
import type {MenuItem, ModuleHeaderElement} from '../module_header.js';

import {getCss} from './launchpad_widget.css.js';
import {getHtml} from './launchpad_widget.html.js';

export interface LaunchpadWidgetElement {
  $: {
    greeting: HTMLElement,
    moduleHeader: ModuleHeaderElement,
  };
}

const LaunchpadWidgetElementBase = I18nMixinLit(CrLitElement);

/**
 * The Launchpad widget, which helps users stay on top of their tasks on the
 * New Tab Page.
 */
export class LaunchpadWidgetElement extends LaunchpadWidgetElementBase {
  static get is() {
    return 'ntp-launchpad-widget';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected getMenuItems_(): MenuItem[] {
    return [
      {
        action: 'disable',
        icon: loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
            'modules:block' :
            'modules:block-old',
        text: this.i18nRecursive(
            '', 'modulesDisableButtonTextV2', 'modulesLaunchpadTitle'),
      },
    ];
  }

  protected onDisableButtonClick_() {
    this.fire('disable-module', {
      message: this.i18n(
          'disableModuleToastMessage', this.i18n('modulesLaunchpadTitle')),
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-launchpad-widget': LaunchpadWidgetElement;
  }
}

customElements.define(LaunchpadWidgetElement.is, LaunchpadWidgetElement);

function createLaunchpadElement(): Promise<LaunchpadWidgetElement|null> {
  return Promise.resolve(document.createElement('ntp-launchpad-widget'));
}

export const launchpadDescriptor: ModuleDescriptor =
    new ModuleDescriptor(/*id=*/ 'launchpad', createLaunchpadElement);
