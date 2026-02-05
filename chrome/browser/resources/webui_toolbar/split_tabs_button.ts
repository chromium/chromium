// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '/strings.m.js';
import './split_tabs_button_icons.html.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';

import {ContextMenuType, SplitTabActiveLocation, ToolbarButtonType} from './browser_controls_api_data_model.mojom-webui.js';
import {type BrowserProxy, BrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './split_tabs_button.css.js';
import {getHtml} from './split_tabs_button.html.js';
import {getContextMenuPosition} from './toolbar_button.js';

export class SplitTabsButtonElement extends CrLitElement {
  static get is() {
    return 'split-tabs-button-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isPinned: {type: Boolean},
      isSplit: {type: Boolean, reflect: true},
      location_: {state: true, type: Number},
    };
  }

  protected accessor isPinned: boolean = false;
  protected accessor isSplit: boolean = false;
  private accessor location_: number = SplitTabActiveLocation.kStart;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private listenerIds_: number[] = [];

  override connectedCallback() {
    super.connectedCallback();

    this.listenerIds_.push(
        this.browserProxy_.callbackRouter.onTabSplitStatusChanged.addListener(
            this.onTabSplitStatusChanged_.bind(this)));

    this.listenerIds_.push(
        this.browserProxy_.callbackRouter.onButtonPinStateChanged.addListener(
            (buttonType: ToolbarButtonType, isPinned: boolean) => {
              if (buttonType !== ToolbarButtonType.kSplitTabs) {
                return;
              }
              this.isPinned = isPinned;
            }));

    this.browserProxy_.handler.getTabSplitState().then(
        ({isSplit, location}) => {
          this.onTabSplitStatusChanged_(isSplit, location);
        });
    this.browserProxy_.handler.getButtonPinState(ToolbarButtonType.kSplitTabs)
        .then(({isPinned}) => {
          this.isPinned = isPinned;
        });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    // The callback router is a singleton that persists across component
    // lifecycles. Remove listeners to prevent memory leaks and duplicate
    // event handling if the component is re-connected.
    this.listenerIds_.forEach(
        id => this.browserProxy_.callbackRouter.removeListener(id));
    this.listenerIds_ = [];
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('isPinned') ||
        changedPrivateProperties.has('isSplit')) {
      this.hidden = !this.isPinned && !this.isSplit;
    }
  }

  protected getIcon(): string {
    let iconName = 'split-scene';
    if (this.isSplit) {
      switch (this.location_) {
        case SplitTabActiveLocation.kStart:
          iconName = 'split-scene-left';
          break;
        case SplitTabActiveLocation.kEnd:
          iconName = 'split-scene-right';
          break;
        case SplitTabActiveLocation.kTop:
          iconName = 'split-scene-up';
          break;
        case SplitTabActiveLocation.kBottom:
          iconName = 'split-scene-down';
          break;
        default:
          break;
      }
    }
    return `split-tabs-button:${iconName}`;
  }

  protected getLabel(): string {
    const labelId = this.isSplit ? 'splitTabsButtonAccNameEnabled' :
                                   'splitTabsButtonAccNamePinned';
    return loadTimeData.getString(labelId);
  }

  protected onClick() {
    if (this.isSplit) {
      // If already split, show the action menu.
      this.browserProxy_.handler.showContextMenu(
          ContextMenuType.kSplitTabsAction, this.menuPosition(),
          MenuSourceType.kMouse);
    } else {
      // If not split, enters split view.
      this.browserProxy_.handler.splitActiveTab();
    }
  }

  protected onContextMenu(e: MouseEvent) {
    e.preventDefault();
    this.browserProxy_.handler.showContextMenu(
        ContextMenuType.kSplitTabsContext, this.menuPosition(),
        MenuSourceType.kMouse);
  }

  protected menuPosition() {
    return getContextMenuPosition(this);
  }

  private onTabSplitStatusChanged_(isSplit: boolean, location: number) {
    this.isSplit = isSplit;
    this.location_ = location;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'split-tabs-button-app': SplitTabsButtonElement;
  }
}

customElements.define(SplitTabsButtonElement.is, SplitTabsButtonElement);
