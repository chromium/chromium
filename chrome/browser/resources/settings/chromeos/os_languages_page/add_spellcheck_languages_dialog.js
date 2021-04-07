// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-add-spellcheck-language-dialog' is a dialog for
 * adding spell check languages.
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import './shared_style.m.js';
import '../../settings_shared_css.js';

import {CrScrollableBehavior} from '//resources/cr_elements/cr_scrollable_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsBehavior} from '../../prefs/prefs_behavior.js';
import {recordClick, recordNavigation, recordPageBlur, recordPageFocus, recordSearch, recordSettingChange, setUserActionRecorderForTesting} from '../metrics_recorder.m.js';

Polymer({
  is: 'os-settings-add-spellcheck-languages-dialog',

  _template: html`{__html_template__}`,

  behaviors: [
    CrScrollableBehavior,
    PrefsBehavior,
  ],

  properties: {
    /* Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @type {!LanguagesModel|undefined} */
    languages: Object,

    /** @type {!LanguageHelper} */
    languageHelper: Object,

    /** @private {!Set<string>} */
    languageCodesToAdd_: {
      type: Object,
      value() {
        return new Set();
      },
    },

    /** @private */
    disableActionButton_: {
      type: Boolean,
      value: true,
      computed: 'shouldDisableActionButton_(languageCodesToAdd_.size)',
    },
  },

  /**
   * True if the user has chosen to add this spell check language (checked its
   * checkbox).
   * @param {string} code
   * @return {boolean}
   * @private
   */
  willAdd_(code) {
    return this.languageCodesToAdd_.has(code);
  },

  /**
   * Handler for an input method checkbox.
   * @param {!{model: !{item: SpellCheckLanguageState},
   *           target: !Element}} e
   * @private
   */
  onCheckboxChange_(e) {
    const languageCode = e.model.item.language.code;
    if (e.target.checked) {
      this.languageCodesToAdd_.add(languageCode);
    } else {
      this.languageCodesToAdd_.delete(languageCode);
    }
    // Polymer doesn't notify changes to set size.
    this.notifyPath('languageCodesToAdd_.size');
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldDisableActionButton_() {
    return !this.languageCodesToAdd_.size;
  },

  /** @private */
  onCancelButtonClick_() {
    this.$.dialog.close();
  },

  /**
   * Add spell check languages.
   * @private
   */
  onActionButtonClick_() {
    this.languageCodesToAdd_.forEach(code => {
      this.languageHelper.toggleSpellCheck(code, true);
    });
    recordSettingChange();
    this.$.dialog.close();
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeydown_(e) {
    if (e.key === 'Escape') {
      this.$.dialog.close();
    }
  },
});
