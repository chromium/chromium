// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../../history_clusters/page_favicon.js';

import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Tab} from '../../../history_types.mojom-webui.js';
import {I18nMixin, loadTimeData} from '../../../i18n_setup.js';
import {InfoDialogElement} from '../../info_dialog';
import {ModuleDescriptor} from '../../module_descriptor.js';
import {MenuItem, ModuleHeaderElementV2} from '../module_header';

import {getTemplate} from './module.html.js';
import {TabResumptionProxyImpl} from './tab_resumption_proxy.js';

export const MAX_TABS = 10;

export interface TabResumptionModuleElement {
  $: {
    infoDialogRender: CrLazyRenderElement<InfoDialogElement>,
    moduleHeaderElementV2: ModuleHeaderElementV2,
  };
}

export class TabResumptionModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-tab-resumption';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** The cluster displayed by this element. */
      tabs: {
        type: Object,
      },
    };
  }

tabs:
  Tab[];

  private getMenuItemGroups_(): MenuItem[][] {
    return [
      [
        {
          action: 'disable',
          icon: 'modules:block',
          text: this.i18nRecursive(
              '', 'modulesDisableButtonTextV2', 'modulesThisTypeOfCardText'),
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

  private onDisableButtonClick_() {
    const disableEvent = new CustomEvent('disable-module', {
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'modulesDisableToastMessage',
            loadTimeData.getString('modulesThisTypeOfCardText')),
      },
    });
    this.dispatchEvent(disableEvent);
  }

  private onInfoButtonClick_() {
    this.$.infoDialogRender.get().showModal();
  }

  private onMenuButtonClick_(e: Event) {
    this.$.moduleHeaderElementV2.showAt(e);
  }
}

customElements.define(
    TabResumptionModuleElement.is, TabResumptionModuleElement);

async function createElement(): Promise<TabResumptionModuleElement|null> {
  const {tabs} = await TabResumptionProxyImpl.getInstance().handler.getTabs();
  if (!tabs || tabs.length === 0) {
    return null;
  }

  const element = new TabResumptionModuleElement();
  element.tabs = tabs.slice(0, MAX_TABS);

  return element;
}

export const tabResumptionDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'tab_resumption', createElement);
