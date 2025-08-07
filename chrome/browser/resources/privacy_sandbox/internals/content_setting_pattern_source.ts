// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './value_display.js';
import './mojo_timestamp.js';
import './mojo_timedelta.js';
import './text_copy_button.js';

import type {Time, TimeDelta} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import type {Value} from 'chrome://resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';

import {getTemplate} from './content_setting_pattern_source.html.js';
import type {ContentSettingPatternSource as MojoContentSettingPatternSource} from './content_settings.mojom-webui.js';
import {ContentSetting} from './content_settings.mojom-webui.js';
import {ProviderType, SessionModel} from './content_settings_enums.mojom-webui.js';
import type {PageHandlerInterface} from './privacy_sandbox_internals.mojom-webui.js';
import type {LogicalFn} from './value_display.js';
import {defaultLogicalFn} from './value_display.js';

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

export class ContentSettingPatternSourceElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  getFieldElement(key: string): HTMLElement|null {
    const elem = this.shadowRoot!.querySelector<HTMLElement>(`.id-${key}`);
    if (!elem) {
      console.warn(`No element found for id-${key}`);
    }
    return elem;
  }

  setField(key: string, value: string) {
    const elemToFill = this.getFieldElement(key);
    if (elemToFill) {
      elemToFill.textContent = value;
      elemToFill.title = value;

      const patternHeader = this.getFieldElement(key + '-header');
      if (patternHeader) {
        const copyButton = document.createElement('text-copy-button');
        copyButton.setAttribute('text-to-copy', value);
        patternHeader.appendChild(copyButton);
      }
    }
  }

  setFieldValue(
      key: string, value: Value, logicalFn: LogicalFn = defaultLogicalFn) {
    const elem = this.getFieldElement(key);
    if (elem) {
      const valueElement = document.createElement('value-display');
      elem.appendChild(valueElement);
      valueElement.configure(value, logicalFn);
    }
  }

  setFieldTime(key: string, time: Time) {
    const elem = this.getFieldElement(key);
    if (elem) {
      const tsElement = document.createElement('mojo-timestamp');
      tsElement.setAttribute('ts', '' + time.internalValue);
      elem.appendChild(tsElement);
    }
  }

  setFieldDuration(key: string, time: TimeDelta) {
    const elem = this.getFieldElement(key);
    if (elem) {
      const tsElement = document.createElement('mojo-timedelta');
      tsElement.setAttribute('duration', '' + time.microseconds);
      elem.appendChild(tsElement);
    }
  }

  private getSearchableContent(
      cs: MojoContentSettingPatternSource, primaryPattern: string,
      secondaryPattern: string): string {
    const formatTimeToISO = (t: Time) => t.internalValue > 0 ?
        new Date(Number(t.internalValue / 1000n)).toISOString() :
        undefined;

    const searchableContent = [
      primaryPattern,
      secondaryPattern,
      cs.settingValue.intValue !== undefined ?
          ContentSetting[cs.settingValue.intValue] :
          undefined,
      JSON.stringify(cs.settingValue),
      cs.source !== undefined ? ProviderType[cs.source] : undefined,
      cs.incognito.toString(),
      formatTimeToISO(cs.metadata.lastModified),
      formatTimeToISO(cs.metadata.lastUsed),
      formatTimeToISO(cs.metadata.lastVisited),
      formatTimeToISO(cs.metadata.expiration),
      cs.metadata.sessionModel !== undefined ?
          SessionModel[cs.metadata.sessionModel] :
          undefined,
      cs.metadata.lifetime.microseconds.toString(),
    ];

    return searchableContent.filter(Boolean).join(' ');
  }

  async configure(
      pageHandler: PageHandlerInterface,
      cs: MojoContentSettingPatternSource): Promise<string> {
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

    // Populate the UI fields.
    this.setField('primary-pattern', primaryPatternString);
    this.setField('secondary-pattern', secondaryPatternString);
    this.setFieldValue('value', cs.settingValue, contentSettingLogicalValue);
    this.setFieldValue(
        'source', {intValue: cs.source}, providerTypeLogicalValue);
    this.setFieldValue('incognito', {boolValue: cs.incognito});
    this.setFieldTime('last-modified', cs.metadata.lastModified);
    this.setFieldTime('last-used', cs.metadata.lastUsed);
    this.setFieldTime('last-visited', cs.metadata.lastVisited);
    this.setFieldTime('expiration', cs.metadata.expiration);
    this.setFieldValue(
        'session-model', {intValue: cs.metadata.sessionModel},
        sessionModelLogicalValue);
    this.setFieldDuration('lifetime', cs.metadata.lifetime);

    return this.getSearchableContent(
        cs, primaryPatternString, secondaryPatternString);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'content-setting-pattern-source': ContentSettingPatternSourceElement;
  }
}

customElements.define(
    'content-setting-pattern-source', ContentSettingPatternSourceElement);
