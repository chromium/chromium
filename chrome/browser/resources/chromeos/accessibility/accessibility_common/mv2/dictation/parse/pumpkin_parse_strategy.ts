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
import {ToggleDictationMacro} from '/common/action_fulfillment/macros/toggle_dictation_macro.js';

import {LocaleInfo} from '../locale_info.js';
import {ListCommandsMacro} from '../macros/list_commands_macro.js';

import {ParseStrategy} from './parse_strategy.js';
import {FromPumpkinTagger, FromPumpkinTaggerCommand, HypothesisArgumentName, PumpkinData, PumpkinLocale, SANDBOXED_PUMPKIN_TAGGER_JS_FILE, SUPPORTED_LOCALES, ToPumpkinTagger, ToPumpkinTaggerCommand} from './pumpkin/pumpkin_constants.js';

/** A parsing strategy that utilizes the Pumpkin semantic parser. */
export class PumpkinParseStrategy extends ParseStrategy {
  /** @param {!InputController} inputController */
  constructor(inputController: InputController) {
    super(inputController);
    this.init_();
  }

  private pumpkinData_: PumpkinData|null = null;
  private pumpkinTaggerReady_ = false;
  private tagResolver_: (
      (results: proto.speech.pumpkin.PumpkinTaggerResults) => void)|null = null;
  private worker_: Worker|null = null;
  private locale_: PumpkinLocale|null = null;
  private requestedPumpkinInstall_ = false;
  private onPumpkinTaggerReadyChangedForTesting_: VoidFunction|null = null;

  private init_(): void {
    this.refreshLocale_();
    if (!this.locale_) {
      return;
    }

    this.requestedPumpkinInstall_ = true;
    chrome.accessibilityPrivate.installPumpkinForDictation(data => {
      // TODO(crbug.com/259352407): Consider retrying installation at a later
      // time if it failed.
      this.onPumpkinInstalled_(data);
    });
  }

  private onPumpkinInstalled_(data: PumpkinData): void {
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
        new URL(SANDBOXED_PUMPKIN_TAGGER_JS_FILE, import.meta.url),
        {type: 'module'});
    this.worker_.onmessage = (message) => this.onMessage_(message);
  }

  /**
   * Called when the SandboxedPumpkinTagger posts a message to the background
   * context.
   */
  private onMessage_(message: MessageEvent): void {
    const command: FromPumpkinTagger = message.data;
    switch (command.type) {
      case FromPumpkinTaggerCommand.READY:
        this.refreshLocale_();
        if (!this.locale_) {
          throw new Error(
              `Can't load SandboxedPumpkinTagger in an unsupported locale ${
                  LocaleInfo.locale}`);
        }

        this.sendToSandboxedPumpkinTagger_({
          type: ToPumpkinTaggerCommand.LOAD,
          locale: this.locale_,
          pumpkinData: this.pumpkinData_,
        });
        this.pumpkinData_ = null;
        return;
      case FromPumpkinTaggerCommand.FULLY_INITIALIZED:
        this.setPumpkinTaggerReady_(true);
        this.maybeRefresh_();
        return;
      case FromPumpkinTaggerCommand.TAG_RESULTS:
        // TODO(crbug.com/314203187): Not null asserted, check that this is
        // correct.
        this.tagResolver_!
            (command.results as proto.speech.pumpkin.PumpkinTaggerResults);
        return;
      case FromPumpkinTaggerCommand.REFRESHED:
        this.setPumpkinTaggerReady_(true);
        this.maybeRefresh_();
        return;
      default:
        throw new Error(
            `Unrecognized message received from SandboxedPumpkinTagger: ${
                command.type}`);
    }
  }

  private sendToSandboxedPumpkinTagger_(command: ToPumpkinTagger): void {
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
   * @return The macro matching the hypothesis if one can be found.
   */
  private macroFromPumpkinHypothesis_(
      hypothesis: proto.speech.pumpkin.HypothesisResult): Macro|null {
    const numArgs = hypothesis.actionArgumentList.length;
    if (!numArgs) {
      return null;
    }
    let repeat = 1;
    let text = '';
    let tag = MacroName.UNSPECIFIED;
    let beginPhrase = '';
    let endPhrase = '';
    for (let i = 0; i < numArgs; i++) {
      const argument = hypothesis.actionArgumentList[i];
      // See Variable Argument Placeholders in voiceaccess.patterns_template.
      if (argument.name === HypothesisArgumentName.SEM_TAG) {
        // Map Pumpkin's STOP_LISTENING to generic TOGGLE_DICTATION macro.
        // When this is run by Dictation, it always stops.
        if (argument.value === 'STOP_LISTENING') {
          tag = MacroName.TOGGLE_DICTATION;
        } else {
          tag = MacroName[argument.value as keyof typeof MacroName];
        }
      } else if (argument.name === HypothesisArgumentName.NUM_ARG) {
        repeat = argument.value;
      } else if (argument.name === HypothesisArgumentName.OPEN_ENDED_TEXT) {
        text = argument.value;
      } else if (argument.name === HypothesisArgumentName.BEGIN_PHRASE) {
        beginPhrase = argument.value;
      } else if (argument.name === HypothesisArgumentName.END_PHRASE) {
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
        return new ToggleDictationMacro(this.getInputController().isActive());
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

  private refreshLocale_(): void {
    this.locale_ = SUPPORTED_LOCALES[
        LocaleInfo.locale as keyof typeof SUPPORTED_LOCALES] || null;
  }

  /**
   * Refreshes SandboxedPumpkinTagger if the Dictation locale differs from
   * the pumpkin locale.
   */
  private maybeRefresh_(): void {
    const dictationLocale = SUPPORTED_LOCALES[
        LocaleInfo.locale as keyof typeof SUPPORTED_LOCALES] || null;
    if (dictationLocale !== this.locale_) {
      this.refresh();
    }
  }

  override refresh(): void {
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
      type: ToPumpkinTaggerCommand.REFRESH,
      locale: this.locale_,
    });
  }

  override async parse(text: string): Promise<Macro|null> {
    if (!this.isEnabled() || !this.pumpkinTaggerReady_) {
      return null;
    }

    this.tagResolver_ = null;
    // Get results from Pumpkin.
    // TODO(crbug.com/1264544): Could increase the hypotheses count from 1
    // when we are ready to implement disambiguation.
    this.sendToSandboxedPumpkinTagger_({
      type: ToPumpkinTaggerCommand.TAG,
      text,
      numResults: 1,
    });
    const taggerResults = await new Promise(
        (resolve: (results: proto.speech.pumpkin.PumpkinTaggerResults) =>
             void) => {
          this.tagResolver_ = resolve;
        });

    if (!taggerResults || taggerResults.hypothesisList.length === 0) {
      return null;
    }

    return this.macroFromPumpkinHypothesis_(taggerResults.hypothesisList[0]);
  }

  override isEnabled(): boolean {
    return this.enabled;
  }

  private setPumpkinTaggerReady_(ready: boolean) {
    this.pumpkinTaggerReady_ = ready;
    if (this.onPumpkinTaggerReadyChangedForTesting_) {
      this.onPumpkinTaggerReadyChangedForTesting_();
    }
  }
}
