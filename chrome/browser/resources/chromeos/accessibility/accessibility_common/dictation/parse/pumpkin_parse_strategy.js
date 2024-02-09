// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a strategy for parsing text that utilizes the pumpkin
 * semantic parser.
 */

import {InputController} from '/common/action_fulfillment/input_controller.js';
import {DeletePrevSentMacro} from '/common/action_fulfillment/macros/delete_prev_sent_macro.js';
import {InputTextViewMacro} from '/common/action_fulfillment/macros/input_text_view_macro.js';
import {Macro} from '/common/action_fulfillment/macros/macro.js';
import {MacroName} from '/common/action_fulfillment/macros/macro_names.js';
import {NavNextSentMacro, NavPrevSentMacro} from '/common/action_fulfillment/macros/nav_sent_macro.js';
import {RepeatMacro} from '/common/action_fulfillment/macros/repeat_macro.js';
import * as RepeatableKeyPressMacro from '/common/action_fulfillment/macros/repeatable_key_press_macro.js';
import {SmartDeletePhraseMacro} from '/common/action_fulfillment/macros/smart_delete_phrase_macro.js';
import {SmartInsertBeforeMacro} from '/common/action_fulfillment/macros/smart_insert_before_macro.js';
import {SmartReplacePhraseMacro} from '/common/action_fulfillment/macros/smart_replace_phrase_macro.js';
import {SmartSelectBetweenMacro} from '/common/action_fulfillment/macros/smart_select_between_macro.js';

import {ToggleDictationMacro} from '../../../common/action_fulfillment/macros/toggle_dictation_macro.js';
import {LocaleInfo} from '../locale_info.js';
import {ListCommandsMacro} from '../macros/list_commands_macro.js';

import {ParseStrategy} from './parse_strategy.js';
import * as PumpkinConstants from './pumpkin/pumpkin_constants.js';

/** A parsing strategy that utilizes the Pumpkin semantic parser. */
export class PumpkinParseStrategy extends ParseStrategy {
  /** @param {!InputController} inputController */
  constructor(inputController) {
    super(inputController);
    /** @private {?PumpkinConstants.PumpkinData} */
    this.pumpkinData_ = null;
    /** @private {boolean} */
    this.pumpkinTaggerReady_ = false;
    /** @private {Function} */
    this.tagResolver_ = null;
    /** @private {?Worker} */
    this.worker_ = null;
    /** @private {?PumpkinConstants.PumpkinLocale} */
    this.locale_ = null;
    /** @private {boolean} */
    this.requestedPumpkinInstall_ = false;

    /** @private {?function(): void} */
    this.onPumpkinTaggerReadyChangedForTesting_ = null;

    this.init_();
  }

  /** @private */
  init_() {
    this.refreshLocale_();
    if (!this.locale_) {
      return;
    }

    this.requestedPumpkinInstall_ = true;
    chrome.accessibilityPrivate.installPumpkinForDictation(data => {
      // TODO(crbug.comg/1258190): Consider retrying installation at a later
      // time if it failed.
      this.onPumpkinInstalled_(data);
    });
  }

  /**
   * @param {PumpkinConstants.PumpkinData} data
   * @private
   */
  onPumpkinInstalled_(data) {
    if (!data) {
      console.warn('Pumpkin installed, but data is empty');
      return;
    }

    for (const [key, value] of Object.entries(data)) {
      if (!value || value.byteLength === 0) {
        throw new Error(`Pumpkin data incomplete, missing data for ${key}`);
      }
    }

    this.refreshLocale_();
    if (!this.locale_ || !this.isEnabled()) {
      return;
    }

    // Create SandboxedPumpkinTagger.
    this.setPumpkinTaggerReady_(false);
    this.pumpkinData_ = data;

    this.worker_ = new Worker(
        PumpkinConstants.SANDBOXED_PUMPKIN_TAGGER_JS_FILE, {type: 'module'});
    this.worker_.onmessage = (message) => this.onMessage_(message);
  }

  /**
   * Called when the SandboxedPumpkinTagger posts a message to the background
   * context.
   * @param {!Event} message
   * @private
   */
  onMessage_(message) {
    const command =
        /** @type {!PumpkinConstants.FromPumpkinTagger} */ (message.data);
    switch (command.type) {
      case PumpkinConstants.FromPumpkinTaggerCommand.READY:
        this.refreshLocale_();
        if (!this.locale_) {
          throw new Error(
              `Can't load SandboxedPumpkinTagger in an unsupported locale ${
                  LocaleInfo.locale}`);
        }

        this.sendToSandboxedPumpkinTagger_({
          type: PumpkinConstants.ToPumpkinTaggerCommand.LOAD,
          locale: this.locale_,
          pumpkinData: this.pumpkinData_,
        });
        this.pumpkinData_ = null;
        return;
      case PumpkinConstants.FromPumpkinTaggerCommand.FULLY_INITIALIZED:
        this.setPumpkinTaggerReady_(true);
        this.maybeRefresh_();
        return;
      case PumpkinConstants.FromPumpkinTaggerCommand.TAG_RESULTS:
        this.tagResolver_(command.results);
        return;
      case PumpkinConstants.FromPumpkinTaggerCommand.REFRESHED:
        this.setPumpkinTaggerReady_(true);
        this.maybeRefresh_();
        return;
    }

    throw new Error(
        `Unrecognized message received from SandboxedPumpkinTagger: ${
            command.type}`);
  }

  /**
   * @param {!PumpkinConstants.ToPumpkinTagger} command
   * @private
   */
  sendToSandboxedPumpkinTagger_(command) {
    if (!this.worker_) {
      throw new Error(
          `Worker not ready, cannot send command to SandboxedPumpkinTagger: ${
              command.type}`);
    }

    this.worker_.postMessage(command);
  }

  /**
   * In Android Voice Access, Pumpkin Hypotheses will be converted to UserIntent
   * protos before being passed to Macros.
   * @param {proto.speech.pumpkin.HypothesisResult.ObjectFormat} hypothesis
   * @return {?Macro} The macro matching the hypothesis if one can be found.
   * @private
   */
  macroFromPumpkinHypothesis_(hypothesis) {
    const numArgs = hypothesis.actionArgumentList.length;
    if (!numArgs) {
      return null;
    }
    let repeat = 1;
    let text = '';
    let tag = '';
    let beginPhrase = '';
    let endPhrase = '';
    for (let i = 0; i < numArgs; i++) {
      const argument = hypothesis.actionArgumentList[i];
      // See Variable Argument Placeholders in voiceaccess.patterns_template.
      if (argument.name === PumpkinConstants.HypothesisArgumentName.SEM_TAG) {
        // Map Pumpkin's STOP_LISTENING to generic TOGGLE_DICTATION macro.
        // When this is run by Dictation, it always stops.
        if (argument.value === 'STOP_LISTENING') {
          tag = MacroName.TOGGLE_DICTATION;
        } else {
          tag = MacroName[argument.value];
        }
      } else if (
          argument.name === PumpkinConstants.HypothesisArgumentName.NUM_ARG) {
        repeat = argument.value;
      } else if (
          argument.name ===
          PumpkinConstants.HypothesisArgumentName.OPEN_ENDED_TEXT) {
        text = argument.value;
      } else if (
          argument.name ===
          PumpkinConstants.HypothesisArgumentName.BEGIN_PHRASE) {
        beginPhrase = argument.value;
      } else if (
          argument.name ===
          PumpkinConstants.HypothesisArgumentName.END_PHRASE) {
        endPhrase = argument.value;
      }
    }

    switch (tag) {
      case MacroName.INPUT_TEXT_VIEW:
        return new InputTextViewMacro(text, this.getInputController());
      case MacroName.DELETE_PREV_CHAR:
        return new RepeatableKeyPressMacro.DeletePreviousCharacterMacro(
            this.getInputController(), repeat);
      case MacroName.NAV_PREV_CHAR:
        return new RepeatableKeyPressMacro.NavPreviousCharMacro(
            this.getInputController(), LocaleInfo.isRTLLocale(), repeat);
      case MacroName.NAV_NEXT_CHAR:
        return new RepeatableKeyPressMacro.NavNextCharMacro(
            this.getInputController(), LocaleInfo.isRTLLocale(), repeat);
      case MacroName.NAV_PREV_LINE:
        return new RepeatableKeyPressMacro.NavPreviousLineMacro(
            this.getInputController(), repeat);
      case MacroName.NAV_NEXT_LINE:
        return new RepeatableKeyPressMacro.NavNextLineMacro(
            this.getInputController(), repeat);
      case MacroName.COPY_SELECTED_TEXT:
        return new RepeatableKeyPressMacro.CopySelectedTextMacro(
            this.getInputController());
      case MacroName.PASTE_TEXT:
        return new RepeatableKeyPressMacro.PasteTextMacro();
      case MacroName.CUT_SELECTED_TEXT:
        return new RepeatableKeyPressMacro.CutSelectedTextMacro(
            this.getInputController());
      case MacroName.UNDO_TEXT_EDIT:
        return new RepeatableKeyPressMacro.UndoTextEditMacro();
      case MacroName.REDO_ACTION:
        return new RepeatableKeyPressMacro.RedoActionMacro();
      case MacroName.SELECT_ALL_TEXT:
        return new RepeatableKeyPressMacro.SelectAllTextMacro(
            this.getInputController());
      case MacroName.UNSELECT_TEXT:
        return new RepeatableKeyPressMacro.UnselectTextMacro(
            this.getInputController(),
            LocaleInfo.isRTLLocale(),
        );
      case MacroName.LIST_COMMANDS:
        return new ListCommandsMacro();
      case MacroName.TOGGLE_DICTATION:
        return new ToggleDictationMacro();
      case MacroName.DELETE_PREV_WORD:
        return new RepeatableKeyPressMacro.DeletePrevWordMacro(
            this.getInputController(), repeat);
      case MacroName.DELETE_PREV_SENT:
        return new DeletePrevSentMacro(this.getInputController());
      case MacroName.NAV_NEXT_WORD:
        return new RepeatableKeyPressMacro.NavNextWordMacro(
            this.getInputController(), LocaleInfo.isRTLLocale(), repeat);
      case MacroName.NAV_PREV_WORD:
        return new RepeatableKeyPressMacro.NavPrevWordMacro(
            this.getInputController(), LocaleInfo.isRTLLocale(), repeat);
      case MacroName.SMART_DELETE_PHRASE:
        return new SmartDeletePhraseMacro(this.getInputController(), text);
      case MacroName.SMART_REPLACE_PHRASE:
        return new SmartReplacePhraseMacro(
            this.getInputController(), beginPhrase, text);
      case MacroName.SMART_INSERT_BEFORE:
        return new SmartInsertBeforeMacro(
            this.getInputController(), text, endPhrase);
      case MacroName.SMART_SELECT_BTWN_INCL:
        return new SmartSelectBetweenMacro(
            this.getInputController(), beginPhrase, endPhrase);
      case MacroName.NAV_NEXT_SENT:
        return new NavNextSentMacro(this.getInputController());
      case MacroName.NAV_PREV_SENT:
        return new NavPrevSentMacro(this.getInputController());
      case MacroName.DELETE_ALL_TEXT:
        return new RepeatableKeyPressMacro.DeleteAllText(
            this.getInputController());
      case MacroName.NAV_START_TEXT:
        return new RepeatableKeyPressMacro.NavStartText(
            this.getInputController());
      case MacroName.NAV_END_TEXT:
        return new RepeatableKeyPressMacro.NavEndText(
            this.getInputController());
      case MacroName.SELECT_PREV_WORD:
        return new RepeatableKeyPressMacro.SelectPrevWord(
            this.getInputController(), repeat);
      case MacroName.SELECT_NEXT_WORD:
        return new RepeatableKeyPressMacro.SelectNextWord(
            this.getInputController(), repeat);
      case MacroName.SELECT_NEXT_CHAR:
        return new RepeatableKeyPressMacro.SelectNextChar(
            this.getInputController(), repeat);
      case MacroName.SELECT_PREV_CHAR:
        return new RepeatableKeyPressMacro.SelectPrevChar(
            this.getInputController(), repeat);
      case MacroName.REPEAT:
        return new RepeatMacro();
      default:
        // Every hypothesis is guaranteed to include a semantic tag due to the
        // way Voice Access set up its grammars. Not all tags are supported in
        // Dictation yet.
        console.log('Unsupported Pumpkin action: ', tag);
        return null;
    }
  }

  /** @private */
  refreshLocale_() {
    this.locale_ =
        PumpkinConstants.SUPPORTED_LOCALES[LocaleInfo.locale] || null;
  }

  /**
   * Refreshes SandboxedPumpkinTagger if the Dictation locale differs from
   * the pumpkin locale.
   * @private
   */
  maybeRefresh_() {
    const dictationLocale =
        PumpkinConstants.SUPPORTED_LOCALES[LocaleInfo.locale];
    if (dictationLocale !== this.locale_) {
      this.refresh();
    }
  }

  /** @override */
  refresh() {
    this.refreshLocale_();
    this.enabled = Boolean(this.locale_) && LocaleInfo.areCommandsSupported();
    if (!this.requestedPumpkinInstall_) {
      this.init_();
      return;
    }

    if (!this.isEnabled() || !this.locale_ || !this.pumpkinTaggerReady_) {
      return;
    }

    this.setPumpkinTaggerReady_(false);
    this.sendToSandboxedPumpkinTagger_({
      type: PumpkinConstants.ToPumpkinTaggerCommand.REFRESH,
      locale: this.locale_,
    });
  }

  /** @override */
  async parse(text) {
    if (!this.isEnabled() || !this.pumpkinTaggerReady_) {
      return null;
    }

    this.tagResolver_ = null;
    // Get results from Pumpkin.
    // TODO(crbug.com/1264544): Could increase the hypotheses count from 1
    // when we are ready to implement disambiguation.
    this.sendToSandboxedPumpkinTagger_({
      type: PumpkinConstants.ToPumpkinTaggerCommand.TAG,
      text,
      numResults: 1,
    });
    const taggerResults = await new Promise(resolve => {
      this.tagResolver_ = resolve;
    });

    if (!taggerResults || taggerResults.hypothesisList.length === 0) {
      return null;
    }

    return this.macroFromPumpkinHypothesis_(taggerResults.hypothesisList[0]);
  }

  /** @override */
  isEnabled() {
    return this.enabled;
  }

  /**
   * @param {boolean} ready
   * @private
   */
  setPumpkinTaggerReady_(ready) {
    this.pumpkinTaggerReady_ = ready;
    if (this.onPumpkinTaggerReadyChangedForTesting_) {
      this.onPumpkinTaggerReadyChangedForTesting_();
    }
  }
}
