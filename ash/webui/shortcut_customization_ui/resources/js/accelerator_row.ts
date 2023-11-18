// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_view.js';
import './text_accelerator.js';
import '../strings.m.js';
import '../css/shortcut_customization_shared.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './accelerator_row.html.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {AcceleratorInfo, AcceleratorSource, LayoutStyle, ShortcutProviderInterface, StandardAcceleratorInfo, TextAcceleratorInfo, TextAcceleratorPart} from './shortcut_types.js';
import {getAriaLabelForStandardAccelerators, getAriaLabelForTextAccelerators, getTextAcceleratorParts, isCustomizationAllowed} from './shortcut_utils.js';

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
    if (!isCustomizationAllowed() || this.isTextLayout()) {
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
    return getTextAcceleratorParts(infos);
  }

  protected isEmptyList(infos: AcceleratorInfo[]): boolean {
    return infos.length === 0;
  }

  // Returns true if it is the first accelerator in the list.
  protected isFirstAccelerator(index: number): boolean {
    return index === 0;
  }

  private onEditIconClicked(): void {
    this.dispatchEvent(
        new CustomEvent('edit-icon-clicked', {bubbles: true, composed: true}));
  }

  protected getTabIndex(): number {
    // If customization is disabled, this element should not be tab-focusable.
    return !isCustomizationAllowed() ? -1 : 0;
  }

  protected onFocusOrMouseEnter(): void {
    strictQuery('#container', this.shadowRoot, HTMLTableRowElement).focus();
  }

  private getAriaLabel(): string {
    let acceleratorText;

    if (this.acceleratorInfos.length === 0) {
      // No shortcut assigned case:
      acceleratorText = this.i18n('noShortcutAssigned');
    } else if (this.isDefaultLayout()) {
      // Default accelerator:
      acceleratorText = getAriaLabelForStandardAccelerators(
          this.acceleratorInfos as StandardAcceleratorInfo[],
          this.i18n('acceleratorTextDivider'));
    } else {
      // Text accelerator:
      acceleratorText = getAriaLabelForTextAccelerators(
          this.acceleratorInfos as TextAcceleratorInfo[]);
    }

    return this.i18n(
        'acceleratorRowAriaLabel', this.description, acceleratorText);
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