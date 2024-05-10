// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './value_display.js';
import './mojo_timestamp.js';
import './mojo_timedelta.js';

import type {Time, TimeDelta} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import type {Value} from 'chrome://resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';

import {getTemplate} from './content_setting_pattern_source.html.js';
import type {ContentSettingPatternSource} from './content_settings.mojom-webui.js';
import {ContentSetting} from './content_settings.mojom-webui.js';
import {ProviderType, SessionModel} from './content_settings_enums.mojom-webui.js';
import type {PageHandlerInterface} from './privacy_sandbox_internals.mojom-webui.js';
import type {LogicalFn} from './value_display.js';
import {defaultLogicalFn} from './value_display.js';

function contentSettingLogicalValue(v: Value) {
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

function providerTypeLogicalValue(v: Value) {
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

function sessionModelLogicalValue(v: Value) {
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
  static observedAttributes = ['collapsed'];

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

  async configure(
      pageHandler: PageHandlerInterface, cs: ContentSettingPatternSource) {
    try {
      this.setField(
          'primary-pattern',
          (await pageHandler.contentSettingsPatternToString(cs.primaryPattern))
              .s);
    } catch (e) {
      console.error('Error parsing primary pattern ', e);
    }
    try {
      this.setField(
          'secondary-pattern',
          (await pageHandler.contentSettingsPatternToString(
               cs.secondaryPattern))
              .s);
    } catch (e) {
      console.error('Error parsing secondary pattern ', e);
    }
    this.setFieldValue('value', cs.settingValue, contentSettingLogicalValue);
    const source: Value = {} as Value;
    if (cs.source != null) {
      source.intValue = cs.source;
    }
    this.setFieldValue('source', source, providerTypeLogicalValue);

    const incognito: Value = {} as Value;
    incognito.boolValue = cs.incognito;
    this.setFieldValue('incognito', incognito);
    this.setFieldTime('last-modified', cs.metadata.lastModified);
    this.setFieldTime('last-used', cs.metadata.lastUsed);
    this.setFieldTime('last-visited', cs.metadata.lastVisited);
    this.setFieldTime('expiration', cs.metadata.expiration);
    const sessionModel: Value = {} as Value;
    sessionModel.intValue = cs.metadata.sessionModel;
    this.setFieldValue('session-model', sessionModel, sessionModelLogicalValue);
    this.setFieldDuration('lifetime', cs.metadata.lifetime);

    this.shadowRoot!.querySelector<HTMLElement>(
                        '#expand-button')!.addEventListener('click', () => {
      if (this.getAttribute('collapsed') === 'true') {
        this.setAttribute('collapsed', 'false');
      } else {
        this.setAttribute('collapsed', 'true');
      }
    });
  }

  attributeChangedCallback(name: string, _oldValue: string, newValue: string) {
    if (name === 'collapsed') {
      const table = this.shadowRoot!.querySelector<HTMLElement>('#metadata')!;
      if (newValue === 'true') {
        table.classList.add('hidden');
      } else {
        table.classList.remove('hidden');
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'content-setting-pattern-source': ContentSettingPatternSourceElement;
  }
}

customElements.define(
    'content-setting-pattern-source', ContentSettingPatternSourceElement);
