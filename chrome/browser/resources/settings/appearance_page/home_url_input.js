// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `home-url-input` is a single-line text field intending to be used with
 * prefs.homepage
 */
Polymer({
  is: 'home-url-input',

  behaviors: [CrPolicyPrefBehavior, PrefControlBehavior],

  properties: {
    /**
     * The preference object to control.
     * @type {!chrome.settingsPrivate.PrefObject|undefined}
     * @override
     */
    pref: {observer: 'prefChanged_'},

    /* Set to true to disable editing the input. */
    disabled: {type: Boolean, value: false, reflectToAttribute: true},

    canTab: Boolean,

    invalid: {type: Boolean, value: false},

    /* The current value of the input, reflected to/from |pref|. */
    value: {
      type: String,
      value: '',
      notify: true,
    },
  },

  /** @private {?settings.AppearanceBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.AppearanceBrowserProxyImpl.getInstance();
    this.noExtensionIndicator = true;  // Prevent double indicator.
  },

  /**
   * Focuses the 'input' element.
   */
  focus: function() {
    this.$.input.focus();
  },

  /**
   * Polymer changed observer for |pref|.
   * @private
   */
  prefChanged_: function() {
    if (!this.pref)
      return;

    // Ignore updates while the input is focused so that user input is not
    // overwritten.
    if (this.$.input.focused)
      return;

    this.setInputValueFromPref_();
  },

  /** @private */
  setInputValueFromPref_: function() {
    assert(this.pref.type == chrome.settingsPrivate.PrefType.URL);
    this.value = /** @type {string} */ (this.pref.value);
  },

  /**
   * Gets a tab index for this control if it can be tabbed to.
   * @param {boolean} canTab
   * @return {number}
   * @private
   */
  getTabindex_: function(canTab) {
    return canTab ? 0 : -1;
  },

  /**
   * Change event handler for cr-input. Updates the pref value.
   * settings-input uses the change event because it is fired by the Enter key.
   * @private
   */
  onChange_: function() {
    if (this.invalid) {
      this.resetValue_();
      return;
    }

    assert(this.pref.type == chrome.settingsPrivate.PrefType.URL);
    this.set('pref.value', this.value);
  },

  /** @private */
  resetValue_: function() {
    this.invalid = false;
    this.setInputValueFromPref_();
    this.$.input.blur();
  },

  /**
   * Keydown handler to specify enter-key and escape-key interactions.
   * @param {!Event} event
   * @private
   */
  onKeydown_: function(event) {
    // If pressed enter when input is invalid, do not trigger on-change.
    if (event.key == 'Enter' && this.invalid)
      event.preventDefault();
    else if (event.key == 'Escape')
      this.resetValue_();

    this.stopKeyEventPropagation_(event);
  },

  /**
   * This function prevents unwanted change of selection of the containing
   * cr-radio-group, when the user traverses the input with arrow keys.
   * @param {!Event} e
   * @private
   */
  stopKeyEventPropagation_: function(e) {
    e.stopPropagation();
  },

  /**
   * @param {boolean} disabled
   * @return {boolean} Whether the element should be disabled.
   * @private
   */
  isDisabled_: function(disabled) {
    return disabled || this.isPrefEnforced();
  },

  /** @private */
  validate_: function() {
    if (this.value == '') {
      this.invalid = false;
      return;
    }

    this.browserProxy_.validateStartupPage(this.value).then(isValid => {
      this.invalid = !isValid;
    });
  },
});
