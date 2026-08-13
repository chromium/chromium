// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './value_display.js';
import './mojo_timestamp.js';
import './mojo_timedelta.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {render} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Value} from 'chrome://resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';

import sheet from './content_setting_pattern_source.css' with {type : 'css'};
import {getHtml} from './content_setting_pattern_source.html.js';
import type {ContentSettingPatternSource as MojoContentSettingPatternSource} from './content_settings.mojom-webui.js';
import {ContentSetting} from './content_settings.mojom-webui.js';
import {ProviderType, SessionModel} from './content_settings_enums.mojom-webui.js';
import type {PageHandlerInterface} from './content_settings_internals.mojom-webui.js';
import type {LogicalFn, ValueDisplayElement} from './value_display.js';

function contentSettingLogicalValue(v: Value): HTMLElement|undefined {
  if (v.intValue === undefined) {
    return undefined;
  }
  const s = ContentSetting[v.intValue];
  if (s === undefined) {
    return undefined;
  }
  const el = document.createElement('span');
  el.textContent = s;
  return el;
}

function providerTypeLogicalValue(v: Value): HTMLElement|undefined {
  if (v.intValue === undefined) {
    return undefined;
  }
  const s = ProviderType[v.intValue];
  if (s === undefined) {
    return undefined;
  }
  const el = document.createElement('span');
  el.textContent = s;
  return el;
}

function sessionModelLogicalValue(v: Value): HTMLElement|undefined {
  if (v.intValue === undefined) {
    return undefined;
  }
  const s = SessionModel[v.intValue];
  if (s === undefined) {
    return undefined;
  }
  const el = document.createElement('span');
  el.textContent = s;
  return el;
}

function createValueDisplay(
    value: Value, logicalFn?: LogicalFn): ValueDisplayElement {
  const el = document.createElement('value-display');
  el.configure(value, logicalFn);
  return el;
}

export class ContentSettingPatternSourceElement extends CustomElement {
  constructor() {
    super();
    this.shadowRoot!.adoptedStyleSheets = [sheet];
  }

  async configure(
      pageHandler: PageHandlerInterface,
      cs: MojoContentSettingPatternSource): Promise<void> {
    let primaryPatternString = '';
    let secondaryPatternString = '';
    try {
      primaryPatternString =
          (await pageHandler.contentSettingsPatternToString(cs.primaryPattern))
              .s;
    } catch (e) {
      console.error('Error parsing primary pattern ', e);
    }
    try {
      secondaryPatternString =
          (await pageHandler.contentSettingsPatternToString(
               cs.secondaryPattern))
              .s;
    } catch (e) {
      console.error('Error parsing secondary pattern ', e);
    }

    render(
        getHtml({
          primaryPattern: primaryPatternString,
          secondaryPattern: secondaryPatternString,
          sourceElement: createValueDisplay(
              {intValue: cs.source}, providerTypeLogicalValue),
          valueElement:
              createValueDisplay(cs.settingValue, contentSettingLogicalValue),
          incognitoElement: createValueDisplay({boolValue: cs.incognito}),
          lastModified: cs.metadata.lastModified,
          lastUsed: cs.metadata.lastUsed,
          lastVisited: cs.metadata.lastVisited,
          expiration: cs.metadata.expiration,
          sessionModelElement: createValueDisplay(
              {intValue: cs.metadata.sessionModel}, sessionModelLogicalValue),
          lifetime: cs.metadata.lifetime,
        }),
        this.shadowRoot!);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'content-setting-pattern-source': ContentSettingPatternSourceElement;
  }
}

customElements.define(
    'content-setting-pattern-source', ContentSettingPatternSourceElement);
