// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_view.js';
import './text_accelerator.js';
import '../strings.m.js';
import '../css/shortcut_customization_shared.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './accelerator_row.html.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {AcceleratorInfo, AcceleratorSource, AcceleratorState, LayoutStyle, ShortcutProviderInterface, TextAcceleratorInfo, TextAcceleratorPart} from './shortcut_types.js';
import {isCustomizationDisabled} from './shortcut_utils.js';
import {TextAcceleratorElement} from './text_accelerator.js';

export type ShowEditDialogEvent = CustomEvent<{
  description: string,
  accelerators: AcceleratorInfo[],
  action: number,
  source: AcceleratorSource,
}>;

declare global {
  interface HTMLElementEventMap {
    'show-edit-dialog': ShowEditDialogEvent;
  }
}

/**
 * @fileoverview
 * 'accelerator-row' is a wrapper component for one shortcut. It features a
 * description of the shortcut along with a list of accelerators.
 * TODO(jimmyxgong): Implement opening a dialog when clicked.
 */
const AcceleratorRowElementBase = I18nMixin(PolymerElement);
export class AcceleratorRowElement extends AcceleratorRowElementBase {
  static get is(): string {
    return 'accelerator-row';
  }

  static get properties(): PolymerElementProperties {
    return {
      description: {
        type: String,
        value: '',
      },

      acceleratorInfos: {
        type: Array,
        value: () => [],
      },

      layoutStyle: {
        type: Object,
      },

      isLocked: {
        type: Boolean,
        value: false,
      },

      action: {
        type: Number,
        value: 0,
        reflectToAttribute: true,
      },

      source: {
        type: Number,
        value: 0,
        observer: AcceleratorRowElement.prototype.onSourceChanged,
      },
    };
  }

  description: string;
  acceleratorInfos: AcceleratorInfo[];
  layoutStyle: LayoutStyle;
  action: number;
  source: AcceleratorSource;
  private isLocked: boolean;
  private shortcutInterfaceProvider: ShortcutProviderInterface =
      getShortcutProvider();

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    if (!this.isLocked) {
      this.removeEventListener('edit-icon-clicked', () => this.showDialog());
    }
  }

  protected onSourceChanged(): void {
    this.shortcutInterfaceProvider.isMutable(this.source)
        .then(({isMutable}) => {
          this.isLocked = !isMutable;
          if (!this.isLocked) {
            this.addEventListener('edit-icon-clicked', () => this.showDialog());
          }
        });
  }

  private isDefaultLayout(): boolean {
    return this.layoutStyle === LayoutStyle.kDefault;
  }

  private isTextLayout(): boolean {
    return this.layoutStyle === LayoutStyle.kText;
  }

  private showDialog(): void {
    if (isCustomizationDisabled() || this.isTextLayout()) {
      return;
    }

    this.dispatchEvent(new CustomEvent(
        'show-edit-dialog',
        {
          bubbles: true,
          composed: true,
          detail: {
            description: this.description,
            accelerators: this.acceleratorInfos,
            action: this.action,
            source: this.source,
          },
        },
        ));
  }

  protected getTextAcceleratorParts(infos: TextAcceleratorInfo[]):
      TextAcceleratorPart[] {
    return TextAcceleratorElement.getTextAcceleratorParts(infos);
  }

  protected getFilteredAccelerators(accelerators: AcceleratorInfo[]):
      AcceleratorInfo[] {
    return accelerators.filter(
        accel => accel.state !== AcceleratorState.kDisabledByUser);
  }

  protected isEmptyList(infos: AcceleratorInfo[]): boolean {
    return this.getFilteredAccelerators(infos).length === 0;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'accelerator-row': AcceleratorRowElement;
  }
}

customElements.define(AcceleratorRowElement.is, AcceleratorRowElement);