// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_view.js';
import './text_accelerator.js';
import '../strings.m.js';
import '../css/shortcut_customization_shared.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './accelerator_row.html.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {AcceleratorInfo, AcceleratorSource, LayoutStyle, ShortcutProviderInterface, TextAcceleratorPart} from './shortcut_types.js';
import {isCustomizationDisabled, isTextAcceleratorInfo} from './shortcut_utils.js';

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
export class AcceleratorRowElement extends PolymerElement {
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
      this.removeEventListener('click', () => this.showDialog());
    }
  }

  override ready(): void {
    super.ready();
    const numberOfAccelerators = this.layoutStyle == LayoutStyle.kDefault ?
        this.acceleratorInfos.length :
        1;
    this.updateStyles({'--accelerator-row-num-accels': numberOfAccelerators});
  }

  protected onSourceChanged(): void {
    this.shortcutInterfaceProvider.isMutable(this.source)
        .then(({isMutable}) => {
          this.isLocked = !isMutable;
          if (!this.isLocked) {
            this.addEventListener('click', () => this.showDialog());
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

  protected getTextAcceleratorParts(info: AcceleratorInfo[]):
      TextAcceleratorPart[] {
    // For text based layout accelerators, we always expect this to be an array
    // with a single element.
    assert(info.length === 1);
    const textAcceleratorInfo = info[0];
    assert(isTextAcceleratorInfo(textAcceleratorInfo));
    return textAcceleratorInfo.layoutProperties.textAccelerator.parts;
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