// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-chromevox-subpage' is the accessibility settings subpage for
 * ChromeVox settings.
 */

import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';
import './ax_annotations_section.js';
import './bluetooth_braille_display_ui.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertExhaustive} from '../assert_extras.js';
import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {DropdownMenuOptionList, SettingsDropdownMenuElement} from '../controls/settings_dropdown_menu.js';
import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './chromevox_subpage.html.js';
import {ChromeVoxSubpageBrowserProxy, ChromeVoxSubpageBrowserProxyImpl} from './chromevox_subpage_browser_proxy.js';

export {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';

const SYSTEM_VOICE = 'chromeos_system_voice';
const CHROMEVOX_EXTENSION_ID = 'mndnfokpggljbaajbnioimlmbfngpief';
const GOOGLE_TTS_EXTENSION_ID = 'gjjabgpgjpampikjhjpfhneeoapjbjaf';
const ESPEAK_TTS_EXTENSION_ID = 'dakbfdmgjiabojdgbiljlhgjbokobjpg';
const EVENT_STREAM_FILTERS_PREF_KEY =
    'settings.a11y.chromevox.event_stream_filters';
const BRAILLE_TABLE_PREF_KEY = 'settings.a11y.chromevox.braille_table';
const BRAILLE_TABLE_6_PREF_KEY = 'settings.a11y.chromevox.braille_table_6';
const BRAILLE_TABLE_8_PREF_KEY = 'settings.a11y.chromevox.braille_table_8';
const BRAILLE_TABLE_TYPE_PREF_KEY =
    'settings.a11y.chromevox.braille_table_type';
const VIRTUAL_BRAILLE_ROWS_PREF_KEY =
    'settings.a11y.chromevox.virtual_braille_rows';
const VIRTUAL_BRAILLE_COLUMNS_PREF_KEY =
    'settings.a11y.chromevox.virtual_braille_columns';
const MIN_BRAILLE_ROWS = 1;
const MAX_BRAILLE_ROWS = 99;
const MIN_BRAILLE_COLUMNS = 1;
const MAX_BRAILLE_COLUMNS = 99;

enum BrailleTableType {
  BRAILLE_TABLE_6 = 'brailleTable6',
  BRAILLE_TABLE_8 = 'brailleTable8',
}

type EventStreamFiltersPrefValue = Record<string, boolean>;

/**
 * Represents a voice as sent from the TTS Handler class.
 * |name| is the user-facing voice name.
 * |remote| is whether the TTS voice is online (versus on-device).
 * |extensionId| is the Chrome Extension ID for the TTS voice.
 */
interface TtsHandlerVoice {
  name: string;
  remote: boolean;
  extensionId: string;
}

/**
 * Represents a braille table from liblouis.
 */
interface BrailleTable {
  locale: string;
  dots: string;
  id: string;
  grade?: string;
  variant?: string;
  fileNames: string;
  enDisplayName?: string;
  alwaysUseEnDisplayName: boolean;
}

export interface SettingsChromeVoxSubpageElement {
  $: {
    capitalStrategyDropdown: SettingsDropdownMenuElement,
  };
}

const SettingsChromeVoxSubpageElementBase = DeepLinkingMixin(RouteOriginMixin(
    PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

export class SettingsChromeVoxSubpageElement extends
    SettingsChromeVoxSubpageElementBase {
  static get is() {
    return 'settings-chromevox-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Dropdown menu choices for capital strategy options.
       */
      capitalStrategyOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: 'announceCapitals',
              name: loadTimeData.getString('chromeVoxAnnounceCapitals'),
            },
            {
              value: 'increasePitch',
              name: loadTimeData.getString('chromeVoxIncreasePitch'),
            },
          ];
        },
      },

      /**
       * Dropdown menu choices for number reading style options.
       */
      numberReadingStyleOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: 'asWords',
              name: loadTimeData.getString('chromeVoxAsWords'),
            },
            {
              value: 'asDigits',
              name: loadTimeData.getString('chromeVoxAsDigits'),
            },
          ];
        },
      },

      /**
       * Dropdown menu choices for punctuation echo options.
       */
      punctuationEchoOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: 0,
              name: loadTimeData.getString('chromeVoxNone'),
            },
            {
              value: 1,
              name: loadTimeData.getString('chromeVoxSome'),
            },
            {
              value: 2,
              name: loadTimeData.getString('chromeVoxAll'),
            },
          ];
        },
      },

      /**
       * Dropdown menu choices for audio strategy options.
       */
      audioStrategyOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: 'audioNormal',
              name: loadTimeData.getString('chromeVoxAudioNormal'),
            },
            {
              value: 'audioDuck',
              name: loadTimeData.getString('chromeVoxAudioDuck'),
            },
            {
              value: 'audioSuspend',
              name: loadTimeData.getString('chromeVoxAudioSuspend'),
            },
          ];
        },
      },

      /**
       * Dropdown menu choices for braille table type options.
       */
      brailleTableTypeOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: 'brailleTable6',
              name: loadTimeData.getString('chromeVoxBrailleTable6Dot'),
            },
            {
              value: 'brailleTable8',
              name: loadTimeData.getString('chromeVoxBrailleTable8Dot'),
            },
          ];
        },
      },

      /**
       * Dropdown menu choices for braille table options.
       */
      brailleTableOptions_: {
        type: Array,
        value: [],
      },

      /**
       * Dropdown menu choices for virtual braille display style options.
       */
      virtualBrailleDisplayStyleOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: false,
              name: loadTimeData.getString(
                  'chromeVoxVirtualBrailleDisplayStyleInterleave'),
            },
            {
              value: true,
              name: loadTimeData.getString(
                  'chromeVoxVirtualBrailleDisplayStyleSideBySide'),
            },
          ];
        },
      },

      /**
       * Dropdown menu choices for voice options.
       */
      voiceOptions_: {
        type: Array,
        value: [],
      },

      /**
       * Whether developer options is expanded.
       */
      developerOptionsExpanded_: {
        type: Boolean,
        value: false,
      },

      /**
       * Event stream filters list. Should match
       * SettingsManager.EVENT_STREAM_FILTERS from settings_manager.js.
       */
      eventStreamFilters_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            'activedescendantchanged',
            'alert',
            // TODO(crbug.com/1464633) Fully remove
            // ARIA_ATTRIBUTE_CHANGED_DEPRECATED starting in 122, because
            // although it was removed in 118, it is still present in earlier
            // versions of LaCros.
            'ariaAttributeChangedDeprecated',
            'autocorrectionOccured',
            'blur',
            'checkedStateChanged',
            'childrenChanged',
            'clicked',
            'documentSelectionChanged',
            'documentTitleChanged',
            'expandedChanged',
            'focus',
            'focusContext',
            'hide',
            'hitTestResult',
            'hover',
            'imageFrameUpdated',
            'invalidStatusChanged',
            'layoutComplete',
            'liveRegionChanged',
            'liveRegionCreated',
            'loadComplete',
            'locationChanged',
            'mediaStartedPlaying',
            'mediaStoppedPlaying',
            'menuEnd',
            'menuItemSelected',
            'menuListValueChangedDeprecated',
            'menuPopupEnd',
            'menuPopupStart',
            'menuStart',
            'mouseCanceled',
            'mouseDragged',
            'mouseMoved',
            'mousePressed',
            'mouseReleased',
            'rowCollapsed',
            'rowCountChanged',
            'rowExpanded',
            'scrollPositionChanged',
            'scrolledToAnchor',
            'selectedChildrenChanged',
            'selection',
            'selectionAdd',
            'selectionRemove',
            'show',
            'stateChanged',
            'textChanged',
            'textSelectionChanged',
            'treeChanged',
            'valueInTextFieldChanged',
          ];
        },
      },

      mainNodeAnnotationsFeatureEnabled_: {
        type: String,
        value: loadTimeData.getBoolean('mainNodeAnnotationsEnabled'),
        readOnly: true,
      },
    };
  }

  static get observers() {
    return [
      'populateBrailleTableList_(' +
          `prefs.${BRAILLE_TABLE_TYPE_PREF_KEY}.value,` +
          `prefs.${BRAILLE_TABLE_PREF_KEY}.value, brailleTables_)`,
      'onBrailleTableTypeChanged_(' +
          `prefs.${BRAILLE_TABLE_TYPE_PREF_KEY}.value)`,
      `onBrailleTableChanged_(prefs.${BRAILLE_TABLE_PREF_KEY}.value)`,
    ];
  }

  private capitalStrategyOptions_: DropdownMenuOptionList;
  private numberReadingStyleOptions_: DropdownMenuOptionList;
  private punctuationEchoOptions_: DropdownMenuOptionList;
  private audioStrategyOptions_: DropdownMenuOptionList;
  private brailleTableTypeOptions_: DropdownMenuOptionList;
  private brailleTableOptions_: DropdownMenuOptionList;
  private voiceOptions_: DropdownMenuOptionList;
  private virtualBrailleDisplayStyleOptions_: DropdownMenuOptionList;
  private chromeVoxBrowserProxy_: ChromeVoxSubpageBrowserProxy;
  private brailleTables_: BrailleTable[];

  // TODO(270619855): Add tests to verify these controls change their prefs.
  constructor() {
    super();

    this.chromeVoxBrowserProxy_ =
        ChromeVoxSubpageBrowserProxyImpl.getInstance();

    /** RouteOriginMixin override */
    this.route = routes.A11Y_CHROMEVOX;
  }

  override ready(): void {
    super.ready();

    this.addWebUiListener(
        'all-voice-data-updated',
        (voices: TtsHandlerVoice[]) => this.populateVoiceList_(voices));
    this.chromeVoxBrowserProxy_.getAllTtsVoiceData();
    this.chromeVoxBrowserProxy_.refreshTtsVoices();
    this.fetchBrailleTables_();
  }

  /**
   * Note: Overrides RouteOriginMixin implementation.
   */
  override currentRouteChanged(newRoute: Route, prevRoute?: Route): void {
    super.currentRouteChanged(newRoute, prevRoute);

    // Does not apply to this page.
    if (newRoute !== this.route) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * When usePitchChanges is toggled, we should update the preference value and
   * dropdown for capitalStrategy. (The capitalStrategy pref depends on the
   * value of usePitchChanges.)
   * TODO(b/270619855): Add test to verify correct dropdown state when toggling.
   */
  private onUsePitchChangesToggled_(event: Event): void {
    const usePitchChanges =
        (event.target as SettingsToggleButtonElement).checked;

    if (!usePitchChanges) {
      // Backup and disable capitalStrategy setting and set to announceCapitals.
      this.$.capitalStrategyDropdown.disabled = true;
      const capitalStrategy =
          this.getPref<string>('settings.a11y.chromevox.capital_strategy')
              .value;
      this.setPrefValue(
          'settings.a11y.chromevox.capital_strategy_backup', capitalStrategy);
      this.setPrefValue(
          'settings.a11y.chromevox.capital_strategy', 'announceCapitals');
      return;
    }

    // Restore original capitalStrategy setting.
    this.$.capitalStrategyDropdown.disabled = false;
    const capitalStrategyBackup =
        this.getPref<string>('settings.a11y.chromevox.capital_strategy_backup')
            .value;
    this.setPrefValue(
        'settings.a11y.chromevox.capital_strategy', capitalStrategyBackup);
  }

  /**
   * Populates the list of voices for the UI to use in display.
   */
  private populateVoiceList_(voices: TtsHandlerVoice[]): void {
    // TODO(b/271422242): voiceName can actually be omitted in the TTS engine.
    // We should generate a name in that case.
    voices.forEach(voice => voice.name = voice.name || '');
    voices.sort((a, b) => {
      function score(voice: TtsHandlerVoice): number {
        // Prefer Google tts voices over all others.
        if (voice.extensionId === GOOGLE_TTS_EXTENSION_ID) {
          return 4;
        }

        // Next, prefer Espeak tts voices.
        if (voice.extensionId === ESPEAK_TTS_EXTENSION_ID) {
          return 2;
        }

        // Finally, prefer local over remote voices.
        if (!voice.remote) {
          return 1;
        }
        return 0;
      }
      return score(b) - score(a);
    });

    this.voiceOptions_ = [
      {
        value: SYSTEM_VOICE,
        name: this.i18n('chromeVoxSystemVoice'),
      },
      ...voices.map(({name}) => ({value: name, name})),
    ];
  }

  /**
   * Retrieves a list of all available braille tables.
   * TODO(b/268196299): Add tests to verify braille tables correctly populated.
   */
  private fetchBrailleTables_(): void {
    const needsDisambiguation = new Map<string, BrailleTable[]>();
    function preprocess(tables: BrailleTable[]): BrailleTable[] {
      tables.forEach(table => {
        // Save all tables which have a mirroring duplicate for locale + grade.
        const key = table.locale + table.grade!;
        if (!needsDisambiguation.has(key)) {
          needsDisambiguation.set(key, []);
        }

        const entry = needsDisambiguation.get(key);
        entry!.push(table);
      });

      for (const entry of needsDisambiguation.values()) {
        if (entry.length > 1) {
          entry.forEach(table => table.alwaysUseEnDisplayName = true);
        }
      }

      return tables;
    }

    const xhr = new XMLHttpRequest();
    xhr.open('GET', 'static/liblouis/tables.json', true);
    xhr.onreadystatechange = () => {
      if (xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {
        const tables: BrailleTable[] = JSON.parse(xhr.responseText);
        this.set('brailleTables_', preprocess(tables));
      }
    };
    xhr.send();
  }

  private async getBrailleTableDisplayName_(table: BrailleTable):
      Promise<string|undefined> {
    const [applicationLocale, localeName] = await Promise.all([
      this.chromeVoxBrowserProxy_.getApplicationLocale(),
      this.chromeVoxBrowserProxy_.getDisplayNameForLocale(table.locale),
    ]);

    const enDisplayName = table.enDisplayName;
    if (!localeName && !enDisplayName) {
      return;
    }

    let baseName;
    if (enDisplayName &&
        (table.alwaysUseEnDisplayName ||
         applicationLocale.toLowerCase().startsWith('en') || !localeName)) {
      baseName = enDisplayName;
    } else {
      baseName = localeName;
    }

    if (!table.grade && !table.variant) {
      return baseName;
    }
    if (table.grade && !table.variant) {
      return this.i18n(
          'chromeVoxBrailleTableNameWithGrade', baseName, table.grade!);
    }
    if (!table.grade && table.variant) {
      return this.i18n(
          'chromeVoxBrailleTableNameWithVariant', baseName, table.variant!);
    }

    return this.i18n(
        'chromeVoxBrailleTableNameWithVariantAndGrade', baseName,
        table.variant!, table.grade!);
  }

  /**
   * Computes the list of braille tables for the UI to display.
   */
  private async populateBrailleTableList_(): Promise<void> {
    if (!this.brailleTables_) {
      return;
    }

    const dots = this.getPref<string>(BRAILLE_TABLE_TYPE_PREF_KEY).value.at(-1);

    // Gather the display names and sort them according to locale.
    const items: Array<{id: string, name: string}> = [];
    for (const table of this.brailleTables_) {
      if (table.dots !== dots) {
        continue;
      }
      const displayName = await this.getBrailleTableDisplayName_(table);

      // Ignore tables that don't have a display name.
      if (displayName) {
        items.push({id: table.id, name: displayName});
      }
    }
    items.sort((a, b) => a.id.localeCompare(b.id));
    this.brailleTableOptions_ = items.map(({id, name}) => ({value: id, name}));
  }

  private onTtsSettingsClick_(): void {
    Router.getInstance().navigateTo(
        routes.MANAGE_TTS_SETTINGS,
        /* dynamicParams= */ undefined, /* removeSearch= */ true);
  }

  private onBrailleRowsInput_(e: KeyboardEvent): void {
    const inputBox = e.target as CrInputElement;
    if (inputBox.value === '') {
      return;
    }
    const numericalValue = parseInt(inputBox.value, 10);
    if (numericalValue < MIN_BRAILLE_ROWS ||
        numericalValue > MAX_BRAILLE_ROWS) {
      inputBox.value =
          String(this.getPref<number>(VIRTUAL_BRAILLE_ROWS_PREF_KEY).value);
    } else {
      this.setPrefValue(VIRTUAL_BRAILLE_ROWS_PREF_KEY, numericalValue);
    }
  }

  private onBrailleRowsFocusout_(e: KeyboardEvent): void {
    const inputBox = e.target as CrInputElement;
    if (inputBox.value === '') {
      inputBox.value =
          String(this.getPref<number>(VIRTUAL_BRAILLE_ROWS_PREF_KEY).value);
    }
  }

  private onBrailleColumnsInput_(e: KeyboardEvent): void {
    const inputBox = e.target as CrInputElement;
    if (inputBox.value === '') {
      return;
    }
    const numericalValue = parseInt(inputBox.value, 10);
    if (numericalValue < MIN_BRAILLE_COLUMNS ||
        numericalValue > MAX_BRAILLE_COLUMNS) {
      inputBox.value =
          String(this.getPref<number>(VIRTUAL_BRAILLE_COLUMNS_PREF_KEY).value);
    } else {
      this.setPrefValue(VIRTUAL_BRAILLE_COLUMNS_PREF_KEY, numericalValue);
    }
  }

  private onBrailleColumnsFocusout_(e: KeyboardEvent): void {
    const inputBox = e.target as CrInputElement;
    if (inputBox.value === '') {
      inputBox.value =
          String(this.getPref<number>(VIRTUAL_BRAILLE_COLUMNS_PREF_KEY).value);
    }
  }

  private onEventLogClick_(): void {
    window.open(
        'chrome-extension://' + CHROMEVOX_EXTENSION_ID +
        '/chromevox/log_page/log.html');
  }

  private getEventStreamFilterPref_(eventStreamFilter: string):
      chrome.settingsPrivate.PrefObject<boolean> {
    return {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: Boolean(this.prefs) &&
          this.getPref<EventStreamFiltersPrefValue>(
                  EVENT_STREAM_FILTERS_PREF_KEY)
              .value[eventStreamFilter],
    };
  }

  /**
   * When an event stream filter checkbox is checked, update the dictionary pref
   * of event stream filter states.
   */
  private onEventStreamFilterPrefChanged_(e: Event): void {
    // Get all eventStreamFilters, set new filter state.
    const filter = e.target as SettingsToggleButtonElement;
    const eventStreamFilters = {
      ...this
          .getPref<EventStreamFiltersPrefValue>(EVENT_STREAM_FILTERS_PREF_KEY)
          .value,
      [filter.id]: filter.checked,
    };
    this.setPrefValue(EVENT_STREAM_FILTERS_PREF_KEY, eventStreamFilters);
  }

  /**
   * Update braille table prefs when braille table type changed.
   */
  private onBrailleTableTypeChanged_(): void {
    const brailleTableType: BrailleTableType =
        this.getPref<BrailleTableType>(BRAILLE_TABLE_TYPE_PREF_KEY).value;
    let brailleTable;
    switch (brailleTableType) {
      case BrailleTableType.BRAILLE_TABLE_6:
        brailleTable = this.getPref(BRAILLE_TABLE_6_PREF_KEY).value;
        break;
      case BrailleTableType.BRAILLE_TABLE_8:
        brailleTable = this.getPref(BRAILLE_TABLE_8_PREF_KEY).value;
        break;
      default:
        assertExhaustive(brailleTableType);
    }
    this.setPrefValue(BRAILLE_TABLE_PREF_KEY, brailleTable);
  }

  /**
   * Update braille table type prefs when braille table changed.
   */
  private onBrailleTableChanged_(): void {
    const brailleTable = this.getPref<string>(BRAILLE_TABLE_PREF_KEY).value;
    const brailleTableType: BrailleTableType =
        this.getPref<BrailleTableType>(BRAILLE_TABLE_TYPE_PREF_KEY).value;
    switch (brailleTableType) {
      case BrailleTableType.BRAILLE_TABLE_6:
        this.setPrefValue(BRAILLE_TABLE_6_PREF_KEY, brailleTable);
        break;
      case BrailleTableType.BRAILLE_TABLE_8:
        this.setPrefValue(BRAILLE_TABLE_8_PREF_KEY, brailleTable);
        break;
      default:
        assertExhaustive(brailleTableType);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsChromeVoxSubpageElement.is]: SettingsChromeVoxSubpageElement;
  }
}

customElements.define(
    SettingsChromeVoxSubpageElement.is, SettingsChromeVoxSubpageElement);
