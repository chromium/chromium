// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-keyboard' is the settings subpage with keyboard settings.
 */
cr.exportPath('settings');

/**
 * Modifier key IDs corresponding to the ModifierKey enumerators in
 * /ui/base/ime/chromeos/ime_keyboard.h.
 * @enum {number}
 */
settings.ModifierKey = {
  SEARCH_KEY: 0,
  CONTROL_KEY: 1,
  ALT_KEY: 2,
  VOID_KEY: 3,  // Represents a disabled key.
  CAPS_LOCK_KEY: 4,
  ESCAPE_KEY: 5,
  BACKSPACE_KEY: 6,
  ASSISTANT_KEY: 7,
};

Polymer({
  is: 'settings-keyboard',

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private Whether to show Caps Lock options. */
    showCapsLock_: Boolean,

    /** @private Whether this device has an internal keyboard. */
    hasInternalKeyboard_: Boolean,

    /** @private Whether this device has an Assistant key on keyboard. */
    hasAssistantKey_: Boolean,

    /**
     * Whether to show a remapping option for external keyboard's Meta key
     * (Search/Windows keys). This is true only when there's an external
     * keyboard connected that is a non-Apple keyboard.
     * @private
     */
    showExternalMetaKey_: Boolean,

    /**
     * Whether to show a remapping option for the Command key. This is true when
     * one of the connected keyboards is an Apple keyboard.
     * @private
     */
    showAppleCommandKey_: Boolean,

    /** @private {!DropdownMenuOptionList} Menu items for key mapping. */
    keyMapTargets_: Object,

    /**
     * Auto-repeat delays (in ms) for the corresponding slider values, from
     * long to short. The values were chosen to provide a large range while
     * giving several options near the defaults.
     * @private {!Array<number>}
     */
    autoRepeatDelays_: {
      type: Array,
      value: [2000, 1500, 1000, 500, 300, 200, 150],
      readOnly: true,
    },

    /**
     * Auto-repeat intervals (in ms) for the corresponding slider values, from
     * long to short. The slider itself is labeled "rate", the inverse of
     * interval, and goes from slow (long interval) to fast (short interval).
     * @private {!Array<number>}
     */
    autoRepeatIntervals_: {
      type: Array,
      value: [2000, 1000, 500, 300, 200, 100, 50, 30, 20],
      readOnly: true,
    },
  },

  /** @override */
  ready: function() {
    cr.addWebUIListener('show-keys-changed', this.onShowKeysChange_.bind(this));
    settings.DevicePageBrowserProxyImpl.getInstance().initializeKeyboard();
    this.setUpKeyMapTargets_();
  },

  /**
   * Initializes the dropdown menu options for remapping keys.
   * @private
   */
  setUpKeyMapTargets_: function() {
    // Ordering is according to UX, but values match settings.ModifierKey.
    this.keyMapTargets_ = [
      {
        value: settings.ModifierKey.SEARCH_KEY,
        name: loadTimeData.getString('keyboardKeySearch'),
      },
      {
        value: settings.ModifierKey.CONTROL_KEY,
        name: loadTimeData.getString('keyboardKeyCtrl')
      },
      {
        value: settings.ModifierKey.ALT_KEY,
        name: loadTimeData.getString('keyboardKeyAlt')
      },
      {
        value: settings.ModifierKey.CAPS_LOCK_KEY,
        name: loadTimeData.getString('keyboardKeyCapsLock')
      },
      {
        value: settings.ModifierKey.ESCAPE_KEY,
        name: loadTimeData.getString('keyboardKeyEscape')
      },
      {
        value: settings.ModifierKey.BACKSPACE_KEY,
        name: loadTimeData.getString('keyboardKeyBackspace')
      },
      {
        value: settings.ModifierKey.ASSISTANT_KEY,
        name: loadTimeData.getString('keyboardKeyAssistant')
      },
      {
        value: settings.ModifierKey.VOID_KEY,
        name: loadTimeData.getString('keyboardKeyDisabled')
      }
    ];
  },

  /**
   * Handler for updating which keys to show.
   * @param {Object} keyboardParams
   * @private
   */
  onShowKeysChange_: function(keyboardParams) {
    this.hasInternalKeyboard_ = keyboardParams['hasInternalKeyboard'];
    this.hasAssistantKey_ = keyboardParams['hasAssistantKey'];
    this.showCapsLock_ = keyboardParams['showCapsLock'];
    this.showExternalMetaKey_ = keyboardParams['showExternalMetaKey'];
    this.showAppleCommandKey_ = keyboardParams['showAppleCommandKey'];
  },

  onShowKeyboardShortcutViewerTap_: function() {
    settings.DevicePageBrowserProxyImpl.getInstance()
        .showKeyboardShortcutViewer();
  },

  onShowLanguageInputTap_: function() {
    settings.navigateTo(
        settings.routes.LANGUAGES_DETAILS,
        /* dynamicParams */ null, /* removeSearch */ true);
  },

  getExternalMetaKeyLabel_: function(hasInternalKeyboard) {
    return loadTimeData.getString(
        hasInternalKeyboard ? 'keyboardKeyExternalMeta' : 'keyboardKeyMeta');
  },

  getExternalCommandKeyLabel_: function(hasInternalKeyboard) {
    return loadTimeData.getString(
        hasInternalKeyboard ? 'keyboardKeyExternalCommand' :
                              'keyboardKeyCommand');
  },
});
