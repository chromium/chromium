// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Session} from '../../../history_types.mojom-webui.js';
import {I18nMixin} from '../../../i18n_setup.js';
import {InfoDialogElement} from '../../info_dialog';
import {ModuleDescriptor} from '../../module_descriptor.js';

import {getTemplate} from './module.html.js';
import {TabResumptionProxyImpl} from './tab_resumption_proxy.js';


export interface HistoryClustersModuleElement {
  $: {
    infoDialogRender: CrLazyRenderElement<InfoDialogElement>,
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
      sessions: {
        type: Object,
      },
    };
  }
sessions:
  Session[];
}

customElements.define(
    TabResumptionModuleElement.is, TabResumptionModuleElement);

async function createElement(): Promise<TabResumptionModuleElement|null> {
  const {sessions} =
      await TabResumptionProxyImpl.getInstance().handler.getTabs();
  if (!sessions || sessions.length === 0) {
    return null;
  }

  const element = new TabResumptionModuleElement();
  element.sessions = sessions;

  return element;
}

export const tabResumptionDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'tab_resumption', createElement);
