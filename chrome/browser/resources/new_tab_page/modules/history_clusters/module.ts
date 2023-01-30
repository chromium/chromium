// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nMixin} from '../../i18n_setup.js';
import {ModuleDescriptor} from '../module_descriptor.js';

import {getTemplate} from './module.html.js';

export class HistoryClustersModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-history-clusters';
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(
    HistoryClustersModuleElement.is, HistoryClustersModuleElement);

async function createElement(): Promise<HTMLElement|null> {
  const element = new HistoryClustersModuleElement();
  return element;
}

export const historyClustersDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'history-clusters', createElement);
