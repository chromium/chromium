// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../info_dialog.js';
import '../../module_header.js';
import './file_suggestion.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {I18nMixinLit, loadTimeData} from '../../../i18n_setup.js';
import {ModuleDescriptor} from '../../module_descriptor.js';
import type {MenuItem, ModuleHeaderElement} from '../module_header.js';

import {getHtml} from './microsoft_files_module.html.js';

export interface MicrosoftFilesModuleElement {
  $: {
    moduleHeaderElementV2: ModuleHeaderElement,
  };
}

const MicrosoftFilesModuleElementBase = I18nMixinLit(CrLitElement);

/**
 * The SharePoint/OneDrive module, which serves as an inside look to recent
 * activity within a user's Microsoft SharePoint and OneDrive.
 */
export class MicrosoftFilesModuleElement extends
    MicrosoftFilesModuleElementBase {
  static get is() {
    return 'ntp-microsoft-files-module';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showInfoDialog_: {type: Boolean},
    };
  }

  protected showInfoDialog_: boolean = false;

  protected getMenuItemGroups_(): MenuItem[][] {
    return [
      [
        {
          action: 'dismiss',
          icon: 'modules:visibility_off',
          // TODO(crbug.com/372724129): Rename `modulesDriveDismissButtonText`
          // to accommodate both sharepoint and drive modules, or replace it
          // with a sharepoint-specific string.
          text: this.i18n('modulesDriveDismissButtonText'),
        },
        {
          action: 'disable',
          icon: 'modules:block',
          // TODO(crbug.com/372724129): Rename `modulesDriveDisableButtonTextV2`
          // to accommodate both sharepoint and drive modules, or replace it
          // with a sharepoint-specific string.
          text: this.i18n('modulesDriveDisableButtonTextV2'),
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

  protected onDisableButtonClick_() {
    const disableEvent = new CustomEvent('disable-module', {
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesSharepointName')),
      },
    });
    this.dispatchEvent(disableEvent);
  }

  protected onDismissButtonClick_() {
    // TODO(crbug.com/372729916): Handle dismiss button click.
  }

  protected onInfoButtonClick_() {
    this.showInfoDialog_ = true;
  }

  protected onInfoDialogClose_() {
    this.showInfoDialog_ = false;
  }
}

customElements.define(
    MicrosoftFilesModuleElement.is, MicrosoftFilesModuleElement);

async function createMicrosoftFilesElement():
    Promise<MicrosoftFilesModuleElement> {
  // TODO(crbug.com/329492316): Retrieve files from the backend, and return null
  // if there are no files.
  return new MicrosoftFilesModuleElement();
}

export const microsoftFilesModuleDescriptor: ModuleDescriptor =
    new ModuleDescriptor(
        /*id*/ 'microsoft_files', createMicrosoftFilesElement);
