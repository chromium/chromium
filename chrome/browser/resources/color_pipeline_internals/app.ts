// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import '//resources/cr_elements/icons.html.js';
import './color_data.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {CrContainerShadowMixinLit} from 'chrome://resources/cr_elements/cr_container_shadow_mixin_lit.js';
import type {CrMenuSelector} from 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {ALL_SECTIONS} from './color_data.js';
import type {ThemeData, ThemeSection} from './color_data.js';

export interface ColorPipelineInternalsAppElement {
  $: {
    content: HTMLElement,
    menu: CrMenuSelector,
  };
}

const CSS_PREFIX: string = '--color-sys-';
const CC_PREFIX: string = 'kColorSys';

const ColorPipelineInternalsAppElementBase =
    CrContainerShadowMixinLit(CrLitElement);

export class ColorPipelineInternalsAppElement extends
    ColorPipelineInternalsAppElementBase {
  static get is() {
    return 'color-pipeline-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Substring filter that (when set) shows only entries containing
       * `filter`.
       */
      currentColor_: {type: String},
      filter_: {type: String},
      narrow_: {type: Boolean},
    };
  }

  protected accessor currentColor_: string = '';
  protected accessor filter_: string = '';
  protected accessor narrow_: boolean = false;
  protected readonly sections_: ThemeSection[] = ALL_SECTIONS;
  protected red_: string = '';
  protected green_: string = '';
  protected blue_: string = '';
  protected alpha_: string = '';
  protected cssName_: string = '';
  protected cppName_: string = '';

  override firstUpdated() {
    ColorChangeUpdater.forDocument().start();
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    this.filter_ = e.detail.toLowerCase();
  }

  protected entryFilter_(entry: ThemeData): boolean {
    return this.filter_ === '' ||
        entry.background.toLowerCase().includes(this.filter_) ||
        !!(entry.foreground?.toLowerCase().includes(this.filter_));
  }

  /**
   * Prevent clicks on sidebar items from navigating.
   */
  protected onLinkClick_(event: Event) {
    event.preventDefault();
  }

  protected onSelectorActivate_(event: CustomEvent<{selected: string}>) {
    const url = event.detail.selected;
    this.$.menu.selected = url;
    const idx = url.lastIndexOf('#');
    const el = this.$.content.querySelector(url.substring(idx));
    el!.scrollIntoView(true);
  }

  protected onNarrowChanged_(e: CustomEvent<{value: boolean}>) {
    this.narrow_ = e.detail.value;
  }

  protected getEntryName_(entry: ThemeData): string {
    // For display purposes only, strip the prefix.
    let name = entry.background;
    if (name.startsWith(CSS_PREFIX)) {
      name = name.substring(CSS_PREFIX.length);
    }
    return name;
  }

  protected getEntryStyle_(entry: ThemeData): string {
    const foreground = entry.foreground ?? '--color-sys-on-surface';
    return `background-color: var(${entry.background});` +
        ` color: var(${foreground});` +
        ` border: 1px solid var(${foreground});`;
  }

  protected updateColorInfo_(e: MouseEvent) {
    const el = e.target as HTMLElement;
    this.currentColor_ = el.querySelector('p')!.innerText;

    this.cssName_ = CSS_PREFIX + this.currentColor_;
    this.cppName_ = CC_PREFIX +
        this.currentColor_.substring(0, 1).toUpperCase() +
        this.currentColor_.substring(1).replace(
            /-./g, x => x.substring(1).toUpperCase());

    const colorText = getComputedStyle(el).backgroundColor;
    const toHex = (v: string) => {
      const result = parseInt(v).toString(16).toUpperCase();
      return (result.length === 1 ? '0' : '') + result;
    };
    let m = colorText.match(/^rgb\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)$/i);
    if (m) {
      this.red_ = `#${toHex(m[1]!)} / ${m[1]}`;
      this.green_ = `#${toHex(m[2]!)} / ${m[2]}`;
      this.blue_ = `#${toHex(m[3]!)} / ${m[3]}`;
      this.alpha_ = '#FF / 255';
    } else {
      m = colorText.match(
          /^rgba\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)$/i);
      if (m) {
        this.red_ = `#${toHex(m[1]!)} / ${m[1]}`;
        this.green_ = `#${toHex(m[2]!)} / ${m[2]}`;
        this.blue_ = `#${toHex(m[3]!)} / ${m[3]}`;
        this.alpha_ = `#${toHex(m[4]!)} / ${m[4]}`;
        return;
      } else {
        this.red_ = colorText;
        this.green_ = '';
        this.blue_ = '';
        this.alpha_ = '';
      }
    }
  }

  protected clearColorInfo_() {
    this.currentColor_ = '';
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'color-internals-app': ColorPipelineInternalsAppElement;
  }
}

customElements.define(
    ColorPipelineInternalsAppElement.is, ColorPipelineInternalsAppElement);
