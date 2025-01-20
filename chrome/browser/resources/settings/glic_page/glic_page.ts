// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import '../controls/settings_toggle_button.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrShortcutInputElement} from 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';

import {getTemplate} from './glic_page.html.js';

export enum SettingsGlicPageFeaturePrefName {
  LAUNCHER_ENABLED = 'glic.launcher_enabled',
  // TODO(crbug.com/379166610): Keyboard shortcut
}

// browser_element_identifiers constants
const OS_WIDGET_TOGGLE_ELEMENT_ID = 'kGlicOsToggleElementId';
const OS_WIDGET_KEYBOARD_SHORTCUT_ELEMENT_ID =
    'kGlicOsWidgetKeyboardShortcutElementId';

const SettingsGlicPageElementBase = HelpBubbleMixin(PrefsMixin(PolymerElement));

export interface SettingsGlicPageElement {
  $: {
    launcherToggle: SettingsToggleButtonElement,
    shortcutInput: CrShortcutInputElement,
    keyboardShortcutSetting: HTMLElement,
  };
}

export class SettingsGlicPageElement extends SettingsGlicPageElementBase {
  static get is() {
    return 'settings-glic-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }

  override ready() {
    super.ready();
    this.registerHelpBubble(
        OS_WIDGET_TOGGLE_ELEMENT_ID, this.$.launcherToggle.getBubbleAnchor());
  }

  override connectedCallback() {
    super.connectedCallback();
    this.registerHelpBubble(
        OS_WIDGET_KEYBOARD_SHORTCUT_ELEMENT_ID,
        this.$.shortcutInput.getBubbleAnchor());
  }

  private onShortcutUpdated_(_: CustomEvent<string>) {
    // TODO(crbug.com/378143781): Parse the event and save the shortcut to the
    // glic prefs.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-glic-page': SettingsGlicPageElement;
  }
}

customElements.define(SettingsGlicPageElement.is, SettingsGlicPageElement);
