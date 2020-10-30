// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-input-method-options-page' is the settings sub-page
 * to allow users to change options for each input method.
 */
Polymer({
  is: 'settings-input-method-options-page',

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
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
     * @private {!Array<!{title: string, options:!Array<!Object<string, *>>}>}
     */
    optionSections_: {
      type: Array,
      value: [],
    },
  },

  /**
   * The root path of input method options in Prefs.
   * @const {string}
   * @private
   */
  PREFS_PATH: 'settings.language.input_method_specific_settings',

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
    if (route !== settings.routes.OS_LANGUAGES_INPUT_METHOD_OPTIONS) {
      this.id_ = '';
      this.parentNode.pageTitle = '';
      this.optionSections_ = [];
      return;
    }

    const queryParams = settings.Router.getInstance().getQueryParameters();
    this.id_ = queryParams.get('id') || '';
    this.parentNode.pageTitle =
        this.languageHelper.getInputMethodDisplayName(this.id_);
    this.engineId_ =
        settings.input_method_util.getFirstPartyInputMethodEngineId(this.id_);
    this.populateOptionSections_();
  },

  /**
   * Get menu items for an option, and enrich the items with selected status and
   * i18n label.
   * @param {settings.input_method_util.OptionType} name
   * @param {*} value
   */
  getMenuItems(name, value) {
    return settings.input_method_util.getOptionMenuItems(name).map(menuItem => {
      menuItem['selected'] = menuItem['value'] === value;
      menuItem['label'] = this.i18n(menuItem['name']);
      return menuItem;
    });
  },

  /**
   * Generate the sections of options according to the engine ID and Prefs.
   * @private
   */
  populateOptionSections_() {
    const options = settings.input_method_util.generateOptions(this.engineId_);

    const prefValue = this.getPref(this.PREFS_PATH).value;
    const prefix = this.getPrefsPrefix_();
    const currentSettings = prefix in prefValue ? prefValue[prefix] : {};

    const makeOption = (name) => {
      const uiType = settings.input_method_util.getOptionUiType(name);
      const value = name in currentSettings ?
          currentSettings[name] :
          settings.input_method_util.OPTION_DEFAULT[name];
      return {
        name: name,
        uiType: uiType,
        value: value,
        label: this.i18n(settings.input_method_util.getOptionLabelName(name)),
        menuItems: this.getMenuItems(name, value),
        url: settings.input_method_util.getOptionUrl(name),
      };
    };

    // If there is no option name in a section, this section, including the
    // section title, should not be displayed.
    this.optionSections_ =
        options.filter(section => section.optionNames.length > 0)
            .map(section => {
              return {
                title: this.getSectionTitleI18n_(section.title),
                options: section.optionNames.map(makeOption),
              };
            });
  },

  /**
   * @return {string} Prefs prefix for the current engine ID, which is usually
   *     just the engine ID itself, but pinyin and zhuyin are special for legacy
   *     compatibility reason.
   * @private
   */
  getPrefsPrefix_() {
    if (this.engineId_ ===
        settings.input_method_util.InputToolCode.PINYIN_CHINESE_SIMPLIFIED) {
      return 'pinyin';
    } else if (
        this.engineId_ ===
        settings.input_method_util.InputToolCode.ZHUYIN_CHINESE_TRADITIONAL) {
      return 'zhuyin';
    }
    return this.engineId_;
  },

  /**
   * Handler for toggle button and dropdown change. Update the value of the
   * changing option in Cros prefs.
   * @param {!{model: !{option: Object}}} e
   * @private
   */
  onToggleButtonOrDropdownChange_(e) {
    // Get the existing settings dictionary, in order to update it later.
    // |PrefsBehavior.setPrefValue| will update Cros Prefs only if the reference
    // of variable has changed, so we need to copy the current content into a
    // new variable.
    const updatedSettings = {};
    Object.assign(updatedSettings, this.getPref(this.PREFS_PATH)['value']);
    const prefix = this.getPrefsPrefix_();
    if (!(prefix in updatedSettings)) {
      updatedSettings[prefix] = {};
    }

    // The value of dropdown is not updated immediately when the event is fired.
    // Wait for the polymer state to update to make sure we write the latest
    // to Cros Prefs.
    Polymer.RenderStatus.afterNextRender(this, () => {
      let newValue = e.model.option.value;
      // The value of dropdown in html is always string, but some of the prefs
      // values are used as integer or enum by IME, so we need to store numbers
      // for them to function correctly.
      if (settings.input_method_util.isNumberValue(e.model.option.name)) {
        newValue = parseInt(newValue, 10);
      }
      updatedSettings[prefix][e.model.option.name] = newValue;
      this.setPrefValue(this.PREFS_PATH, updatedSettings);
    });
  },

  /**
   * Opens external link in Chrome.
   * @param {!{model: !{option: !{url: string}}}} e
   * @private
   */
  onExternalLinkClick_(e) {
    window.open(e.model.option.url);
  },

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
      default:
        assertNotReached();
    }
  },

  /**
   * @param {!settings.input_method_util.UiType} item
   * @return {boolean} true if |item| is a toggle button.
   * @private
   */
  isToggleButton_(item) {
    return item === settings.input_method_util.UiType.TOGGLE_BUTTON;
  },

  /**
   * @param {!settings.input_method_util.UiType} item
   * @return {boolean} true if |item| is a dropdown.
   * @private
   */
  isDropdown_(item) {
    return item === settings.input_method_util.UiType.DROPDOWN;
  },

  /**
   * @param {!settings.input_method_util.UiType} item
   * @return {boolean} true if |item| is an external link.
   * @private
   */
  isLink_(item) {
    return item === settings.input_method_util.UiType.LINK;
  },
});
