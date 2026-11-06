// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '/shared/icon_from_table.js';

import {assert} from '//resources/js/assert.js';
import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import type {ExtensionActionId} from '/shared/extensions_bar_data_model.mojom-webui.js';
import type {IconHandle} from '/shared/icon_handle.mojom-webui.js';
import {HighlightTracker} from '/shared/toolbar_button.js';

import {getCss} from './extension.css.js';
import {getHtml} from './extension.html.js';
import type {ExtensionsBarElement} from './extensions_bar.js';

export class ExtensionElement extends CrLitElement {
  static get is() {
    return 'webui-browser-extension';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      iconHandle: {type: Object},
      visible: {type: Boolean, reflect: true},
      text: {type: String},
    };
  }

  accessor iconHandle: IconHandle = {handleId: 0n};
  accessor visible: boolean = false;
  accessor text: string = '';

  // ID of the action, sent to the browser on interaction.
  actionId: ExtensionActionId = {extensionsMenuButton: {}};

  private bar?: ExtensionsBarElement;
  // Key identifying the action (see getExtensionActionKey()).
  private actionKey: string = '';
  protected highlightTracker_: HighlightTracker = new HighlightTracker();

  override connectedCallback() {
    super.connectedCallback();
    TrackedElementManager.getInstance().startTracking(
        this, 'kToolbarActionViewElementId', {
          secondaryId: 'ext:' + this.actionKey,
          onHighlightChanged: this.highlightTracker_.onHighlightChanged,
        });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    TrackedElementManager.getInstance().stopTracking(this);
  }

  init(actionKey: string, bar: ExtensionsBarElement) {
    this.actionKey = actionKey;
    this.bar = bar;
  }

  protected onClick(e: PointerEvent) {
    if (this.highlightTracker_.shouldSkipClick(e)) {
      return;
    }
    assert(this.bar);
    this.bar.onClick(this.actionId);
  }

  protected onContextmenu_(event: PointerEvent) {
    event.preventDefault();
    let sourceType: MenuSourceType = MenuSourceType.kNone;
    switch (event.pointerType) {
      case 'mouse':
        sourceType = MenuSourceType.kMouse;
        break;
      case 'pen':
        sourceType = MenuSourceType.kStylus;
        break;
      case 'touch':
        sourceType = MenuSourceType.kTouch;
        break;
      default:
        break;
    }
    assert(this.bar);
    this.bar.onContextMenu(sourceType, this.actionId);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-extension': ExtensionElement;
  }
}

customElements.define(ExtensionElement.is, ExtensionElement);
