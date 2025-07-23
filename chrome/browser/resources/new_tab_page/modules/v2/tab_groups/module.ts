// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import '/strings.m.js';
import '../module_header.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {I18nMixinLit} from '../../../i18n_setup.js';
import type {TabGroup} from '../../../tab_groups.mojom-webui.js';
import {ModuleDescriptor} from '../../module_descriptor.js';
import type {MenuItem} from '../module_header.js';

import {getCss} from './module.css.js';
import {getHtml} from './module.html.js';
import {TabGroupsProxyImpl} from './tab_groups_proxy.js';

export const MAX_TAB_GROUPS = 4;

const ModuleElementBase = I18nMixinLit(CrLitElement);

/**
 * The Tab Groups module, which helps users resume journey and discover tab
 * groups.
 */
export class ModuleElement extends ModuleElementBase {
  static get is() {
    return 'ntp-tab-groups';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      tabGroups: {type: Object},
    };
  }

  accessor tabGroups: TabGroup[] = [];

  protected getMenuItemGroups_(): MenuItem[][] {
    return [
      [
        {
          action: 'dismiss',
          icon: 'modules:thumb_down',
          text: this.i18nRecursive(
              '', 'modulesDismissButtonText', 'modulesTabGroupsTitle'),
        },
        {
          action: 'disable',
          icon: 'modules:block',
          text: this.i18nRecursive(
              '', 'modulesDisableButtonTextV2', 'modulesTabGroupsTitle'),
        },
        {
          action: 'info',
          icon: 'modules:info',
          text: this.i18n('moduleInfoButtonTitle'),
        },
      ],
      [
        {
          action: 'customize-module',
          icon: 'modules:tune',
          text: this.i18n('modulesCustomizeButtonText'),
        },
      ],
    ];
  }

  protected getTabGroups_(): TabGroup[] {
    return this.tabGroups.slice(0, MAX_TAB_GROUPS);
  }
}

customElements.define(ModuleElement.is, ModuleElement);

async function createElement(): Promise<ModuleElement|null> {
  const {tabGroups} =
      await TabGroupsProxyImpl.getInstance().handler.getTabGroups();
  if (!tabGroups || tabGroups.length === 0) {
    // TODO(crbug.com/431278744): Show zero-state card.
    return null;
  }

  const element = new ModuleElement();
  element.tabGroups = tabGroups;

  return element;
}

export const tabGroupsDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'tab_groups', createElement);
