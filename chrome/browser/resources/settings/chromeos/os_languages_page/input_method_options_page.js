// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-input-method-options-page' is the settings sub-page
 * to allow users to change options for each input method.
 */
import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '../../settings_shared.css.js';
import './os_japanese_clear_ime_data_dialog.js';
import './os_japanese_manage_user_dictionary_page.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {assert, assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {afterNextRender, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Route, Router} from '../router.js';
import {routes} from '../os_route.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {getTemplate} from './input_method_options_page.html.js';
import {generateOptions, getDefaultValue, getFirstPartyInputMethodEngineId, getOptionLabelName, getOptionMenuItems, getOptionSubtitleName, getOptionUiType, getOptionUrl, getSubmenuButtonType, getUntranslatedOptionLabelName, hasOptionsPageInSettings, isOptionLabelTranslated, OPTION_MAP, OptionType, PHYSICAL_KEYBOARD_AUTOCORRECT_ENABLED_BY_DEFAULT, shouldStoreAsNumber, SubmenuButton, UiType} from './input_method_util.js';
import {LanguageHelper} from './languages_types.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {PrefsBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const SettingsInputMethodOptionsPageElementBase = mixinBehaviors(
    [I18nBehavior, PrefsBehavior, RouteObserverBehavior], PolymerElement);

/** @polymer */
class SettingsInputMethodOptionsPageElement extends
    SettingsInputMethodOptionsPageElementBase {
  static get is() {
    return 'settings-input-method-options-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {!LanguageHelper} */
      languageHelper: Object,

      /** Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * Input method ID.
       * @private
       */
      id_: String,

      /**
       * Input method engine ID.
       * @private
       */
      engineId_: String,

      /**
       * The content to be displayed in the page, auto generated every time when
       * the user enters the page.
       * @private {!Array<{title: string, options:!Array<!Object<string, *>>}>}
       */
      optionSections_: {
        type: Array,
        value: [],
      },

      /** @private */
      showClearPersonalizedData_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();

    /**
     * The root path of input method options in Prefs.
     * @const {string}
     * @private
     */
    this.PREFS_PATH = 'settings.language.input_method_specific_settings';
  }

  /**
   * RouteObserverBehavior
   * @param {!Route} route
   * @param {!Route=} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
    if (route !== routes.OS_LANGUAGES_INPUT_METHOD_OPTIONS) {
      this.id_ = '';
      this.parentNode.pageTitle = '';
      this.optionSections_ = [];
      return;
    }

    const queryParams = Router.getInstance().getQueryParameters();
    this.id_ = queryParams.get('id') || '';
    this.parentNode.pageTitle =
        this.languageHelper.getInputMethodDisplayName(this.id_);
    assert(
        this.parentNode.pageTitle !== '',
        `Input method ID '${this.id_}' is invalid`);
    this.engineId_ = getFirstPartyInputMethodEngineId(this.id_);
    this.populateOptionSections_();
  }

  onSubmenuButtonClick_(e) {
    if (e.target.getAttribute('submenu-button-type') ===
        SubmenuButton.JAPANESE_CLEAR_PERSONALIZATION_DATA) {
      this.showClearPersonalizedData_ = true;
      return;
    }
    console.error(`SubmenuButton with invalid type clicked : ${
        e.target.getAttribute('submenu-button-type')}`);
  }

  onClearPersonalizedDataClose_() {
    this.showClearPersonalizedData_ = false;
  }

  /**
   * For some engineId, we want to store the data in a different storage
   * engineId. i.e. we want to use the nacl_mozc_jp settings data for
   * the nacl_mozc_us settings.
   */
  getStorageEngineId_() {
    return this.engineId_ !== 'nacl_mozc_us' ? this.engineId_ : 'nacl_mozc_jp';
  }

  /**
   * Get menu items for an option, and enrich the items with selected status and
   * i18n label.
   * @param {OptionType} name
   * @param {*} value
   */
  getMenuItems(name, value) {
    return getOptionMenuItems(name).map(menuItem => {
      menuItem['selected'] = menuItem['value'] === value;
      menuItem['label'] =
          menuItem['name'] ? this.i18n(menuItem['name']) : menuItem['value'];
      return menuItem;
    });
  }

  /**
   * Generate the sections of options according to the engine ID and Prefs.
   * @private
   */
  populateOptionSections_() {
    const options = generateOptions(
        this.engineId_, loadTimeData.getBoolean('allowPredictiveWriting'),
        loadTimeData.getBoolean('allowDiacriticsOnPhysicalKeyboardLongpress'),
        loadTimeData.getBoolean('systemJapanesePhysicalTyping'));
    const inputMethodSpecificSettings = this.getPref(this.PREFS_PATH).value;
    // The settings for Japanese for both engine nacl_mozc_us and nacl_mozc_jp
    // types will be stored in nacl_mozc_us. See:
    // https://crsrc.org/c/chrome/browser/ash/input_method/input_method_settings.cc;drc=5b784205e8043fb7d1c11e3d80521e80704947ca;l=25
    const engineId = this.getStorageEngineId_();
    const currentSettings = engineId in inputMethodSpecificSettings ?
        inputMethodSpecificSettings[engineId] :
        {};
    const defaultOverrides = this.getDefaultValueOverrides_(engineId);

    const makeOption = (option) => {
      const name = option.name;
      const uiType = getOptionUiType(name);

      let value = name in currentSettings ?
          currentSettings[name] :
          getDefaultValue(name, defaultOverrides);
      if (loadTimeData.getBoolean('allowAutocorrectToggle') &&
          name in OPTION_MAP) {
        value = OPTION_MAP[name].mapValueForDisplay(value);
      }
      if (!this.isSettingValueValid_(name, value)) {
        value = getDefaultValue(name, defaultOverrides);
        this.updatePref_(name, value);
      }

      const label = isOptionLabelTranslated(name) ?
          this.i18n(getOptionLabelName(name)) :
          getUntranslatedOptionLabelName(name);

      const subtitleStringName = getOptionSubtitleName(name);
      const subtitle = subtitleStringName && this.i18n(subtitleStringName);

      return {
        name: name,
        uiType: uiType,
        value: value,
        label: label,
        subtitle: subtitle,
        menuItems: this.getMenuItems(name, value),
        url: getOptionUrl(name),
        dependentOptions: option.dependentOptions ?
            option.dependentOptions.map(t => makeOption({name: t})) :
            [],
        submenuButtonType: this.isSubmenuButton_(uiType) ?
            getSubmenuButtonType(name) :
            undefined,
      };
    };

    // If there is no option name in a section, this section, including the
    // section title, should not be displayed.
    this.optionSections_ =
        options.filter(section => section.optionNames.length > 0)
            .map(section => {
              return {
                title: this.getSectionTitleI18n_(section.title),
                options: section.optionNames.map(makeOption, false),
              };
            });
  }

  /**
   * Returns an object specifying the default values to be used for a subset
   * of options.
   *
   * @param engineId string The engine id we want default values for.
   * @return {Object<OptionType, *>} Default value overrides.
   */
  getDefaultValueOverrides_(engineId) {
    if (!loadTimeData.getBoolean('autocorrectEnableByDefault')) {
      return {};
    }
    const enabledByDefaultKey =
        PHYSICAL_KEYBOARD_AUTOCORRECT_ENABLED_BY_DEFAULT;
    const prefBlob = this.getPref(this.PREFS_PATH).value;
    const isAutocorrectDefaultEnabled =
        prefBlob?.[engineId]?.[enabledByDefaultKey];
    return !isAutocorrectDefaultEnabled ? {} : {
      [OptionType.PHYSICAL_KEYBOARD_AUTO_CORRECTION_LEVEL]: 1,
    };
  }

  /**
   *
   * @param {*} value
   * @private
   */
  dependentOptionsDisabled_(value) {
    // TODO(b/189909728): Sometimes the value comes as a string, other times as
    // an integer, other times as a boolean, so handle all cases. Try to
    // understand and fix this.
    return value === '0' || value === 0 || value === false;
  }

  /**
   * Handler for toggle button and dropdown change. Update the value of the
   * changing option in Cros prefs.
   * @param {{model: {option: Object}}} e
   * @private
   */
  onToggleButtonOrDropdownChange_(e) {
    // e.model isn't correctly set for dependent options, due to nested
    // dom-repeat, so figure out what option was actually set.
    const option = e.model.dependant ? e.model.dependant : e.model.option;
    // The value of dropdown is not updated immediately when the event is fired.
    // Wait for the polymer state to update to make sure we write the latest
    // to Cros Prefs.
    afterNextRender(this, () => {
      this.updatePref_(option.name, option.value);
    });
  }

  isSettingValueValid_(name, value) {
    const uiType = getOptionUiType(name);
    if (uiType !== UiType.DROPDOWN) {
      return true;
    }
    const menuItems = getOptionMenuItems(name);
    return menuItems.find((item) => item.value === value);
  }

  /**
   * Update an input method pref.
   * @param {!OptionType} optionName
   * @param {*} newValue
   * @private
   */
  updatePref_(optionName, newValue) {
    // Get the existing settings dictionary, in order to update it later.
    // |PrefsBehavior.setPrefValue| will update Cros Prefs only if the reference
    // of variable has changed, so we need to copy the current content into a
    // new variable.
    const updatedSettings = {};
    Object.assign(updatedSettings, this.getPref(this.PREFS_PATH)['value']);

    const engineId = this.getStorageEngineId_();
    if (updatedSettings[engineId] === undefined) {
      updatedSettings[engineId] = {};
    }
    if (shouldStoreAsNumber(optionName)) {
      if (loadTimeData.getBoolean('allowAutocorrectToggle')) {
        newValue = OPTION_MAP[optionName].mapValueForWrite(newValue);
      } else {
        newValue = parseInt(newValue, 10);
      }
    }
    updatedSettings[engineId][optionName] = newValue;

    this.setPrefValue(this.PREFS_PATH, updatedSettings);
  }

  /**
   * Opens external link in Chrome.
   * @param {{model: {option: {url: !Route}}}} e
   * @private
   */
  navigateToOtherPageInSettings_(e) {
    Router.getInstance().navigateTo(e.model.option.url);
  }

  /**
   * @param {string} section the name of the section.
   * @return {string} the i18n string for the section title.
   * @private
   */
  getSectionTitleI18n_(section) {
    switch (section) {
      case 'basic':
        return this.i18n('inputMethodOptionsBasicSectionTitle');
      case 'advanced':
        return this.i18n('inputMethodOptionsAdvancedSectionTitle');
      case 'physicalKeyboard':
        return this.i18n('inputMethodOptionsPhysicalKeyboardSectionTitle');
      case 'virtualKeyboard':
        return this.i18n('inputMethodOptionsVirtualKeyboardSectionTitle');
      case 'suggestions':
        return this.i18n('inputMethodOptionsSuggestionsSectionTitle');
      // Japanese section
      case 'inputAssistance':
        return this.i18n('inputMethodOptionsInputAssistanceSectionTitle');
      // Japanese section
      case 'userDictionaries':
        return this.i18n('inputMethodOptionsUserDictionariesSectionTitle');
      // Japanese section
      case 'privacy':
        return this.i18n('inputMethodOptionsPrivacySectionTitle');

      default:
        assertNotReached();
    }
  }

  /**
   * @param {!UiType} item
   * @return {boolean} true if |item| is a toggle button.
   * @private
   */
  isToggleButton_(item) {
    return item === UiType.TOGGLE_BUTTON;
  }


  /**
   * @param {!UiType} item
   * @return {boolean} true if |item| is a toggle button.
   * @private
   */
  isSubmenuButton_(item) {
    return item === UiType.SUBMENU_BUTTON;
  }

  /**
   * @param {!UiType} item
   * @return {boolean} true if |item| is a dropdown.
   * @private
   */
  isDropdown_(item) {
    return item === UiType.DROPDOWN;
  }

  /**
   * @param {!UiType} item
   * @return {boolean} true if |item| is an external link.
   * @private
   */
  isLink_(item) {
    return item === UiType.LINK;
  }
}

customElements.define(
    SettingsInputMethodOptionsPageElement.is,
    SettingsInputMethodOptionsPageElement);
