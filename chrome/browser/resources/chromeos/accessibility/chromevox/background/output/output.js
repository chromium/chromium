// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides output services for ChromeVox.
 */
import {AutomationPredicate} from '../../../common/automation_predicate.js';
import {AutomationUtil} from '../../../common/automation_util.js';
import {constants} from '../../../common/constants.js';
import {Cursor, CURSOR_NODE_INDEX} from '../../../common/cursors/cursor.js';
import {CursorRange} from '../../../common/cursors/range.js';
import {AutomationTreeWalker} from '../../../common/tree_walker.js';
import {NavBraille} from '../../common/braille/nav_braille.js';
import {EventSourceType} from '../../common/event_source_type.js';
import {LocaleOutputHelper} from '../../common/locale_output_helper.js';
import {LogType} from '../../common/log_types.js';
import {Msgs} from '../../common/msgs.js';
import {Spannable} from '../../common/spannable.js';
import {QueueMode, TtsCategory, TtsSpeechProperties} from '../../common/tts_interface.js';
import {ValueSelectionSpan, ValueSpan} from '../braille/spans.js';
import {ChromeVox} from '../chromevox.js';
import {EventSourceState} from '../event_source.js';
import {FocusBounds} from '../focus_bounds.js';
import {LogStore} from '../logging/log_store.js';
import {PhoneticData} from '../phonetic_data.js';

import {OutputAncestryInfo} from './output_ancestry_info.js';
import {OutputFormatParser, OutputFormatParserObserver} from './output_format_parser.js';
import {OutputFormatTree} from './output_format_tree.js';
import {OutputRulesStr} from './output_logger.js';
import {OutputRoleInfo} from './output_role_info.js';
import * as outputTypes from './output_types.js';

const AriaCurrentState = chrome.automation.AriaCurrentState;
const AutomationNode = chrome.automation.AutomationNode;
const DescriptionFromType = chrome.automation.DescriptionFromType;
const Dir = constants.Dir;
const EventType = chrome.automation.EventType;
const NameFromType = chrome.automation.NameFromType;
const Restriction = chrome.automation.Restriction;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;

/**
 * An Output object formats a CursorRange into speech, braille, or both
 * representations. This is typically a |Spannable|.
 * The translation from Range to these output representations rely upon format
 * rules which specify how to convert AutomationNode objects into annotated
 * strings.
 * The format of these rules is as follows.
 * $ prefix: used to substitute either an attribute or a specialized value from
 *     an AutomationNode. Specialized values include role and state.
 *     For example, $value $role $enabled
 *
 * Note: (@) means @ to avoid Closure mistaking it for an annotation.
 *
 * (@) prefix: used to substitute a message. Note the ability to specify params
 * to the message.  For example, '@tag_html' '@selected_index($text_sel_start,
 *     $text_sel_end').
 *
 * (@@) prefix: similar to @, used to substitute a message, but also pulls the
 *     localized string through goog.i18n.MessageFormat to support locale
 *     aware plural handling.  The first argument should be a number which will
 *     be passed as a COUNT named parameter to MessageFormat.
 *     TODO(plundblad): Make subsequent arguments normal placeholder arguments
 *     when needed.
 * = suffix: used to specify substitution only if not previously appended.
 *     For example, $name= would insert the name attribute only if no name
 * attribute had been inserted previously.
 */
export class Output {
  constructor() {
    // TODO(dtseng): Include braille specific rules.
    /** @private {!Array<!Spannable>} */
    this.speechBuffer_ = [];
    /** @private {!Array<!Spannable>} */
    this.brailleBuffer_ = [];
    /** @private {!Array<!Object>} */
    this.locations_ = [];
    /** @private {function(boolean=)} */
    this.speechEndCallback_;

    // Store output rules.
    /** @private {!OutputRulesStr} */
    this.speechRulesStr_ = new OutputRulesStr('enableSpeechLogging');
    /** @private {!OutputRulesStr} */
    this.brailleRulesStr_ = new OutputRulesStr('enableBrailleLogging');

    /**
     * Current global options.
     * @private {{speech: boolean, braille: boolean, auralStyle: boolean}}
     */
    this.formatOptions_ = {speech: true, braille: false, auralStyle: false};

    /**
     * The speech category for the generated speech utterance.
     * @private {TtsCategory}
     */
    this.speechCategory_ = TtsCategory.NAV;

    /**
     * The speech queue mode for the generated speech utterance.
     * @private {QueueMode}
     */
    this.queueMode_;

    /** @private {!outputTypes.OutputContextOrder} */
    this.contextOrder_ = outputTypes.OutputContextOrder.LAST;

    /** @private {!Object<string, boolean>} */
    this.suppressions_ = {};

    /** @private {boolean} */
    this.enableHints_ = true;

    /** @private {!TtsSpeechProperties} */
    this.initialSpeechProps_ = new TtsSpeechProperties();

    /** @private {boolean} */
    this.drawFocusRing_ = true;

    /**
     * Tracks all ancestors which have received primary formatting in
     * |ancestryHelper_|.
     * @private {!WeakSet<!AutomationNode>}
     */
    this.formattedAncestors_ = new WeakSet();
    /** @private {!Object<string, string>} */
    this.replacements_ = {};
  }

  /**
   * Calling this will make the next speech utterance use |mode| even if it
   * would normally queue or do a category flush. This differs from the
   * |withQueueMode| instance method as it can apply to future output.
   * @param {QueueMode|undefined} mode
   */
  static forceModeForNextSpeechUtterance(mode) {
    if (Output.forceModeForNextSpeechUtterance_ === undefined ||
        mode === undefined ||
        // Only allow setting to higher queue modes.
        mode < Output.forceModeForNextSpeechUtterance_) {
      Output.forceModeForNextSpeechUtterance_ = mode;
    }
  }

  /**
   * For a given automation property, return true if the value
   * represents something 'truthy', e.g.: for checked:
   * 'true'|'mixed' -> true
   * 'false'|undefined -> false
   */
  static isTruthy(node, attrib) {
    switch (attrib) {
      case 'checked':
        return node.checked && node.checked !== 'false';
      case 'hasPopup':
        return node.hasPopup &&
            node.hasPopup !== chrome.automation.HasPopup.FALSE;

      // Chrome automatically calculates these attributes.
      case 'posInSet':
        return node.htmlAttributes['aria-posinset'] ||
            (node.root.role !== RoleType.ROOT_WEB_AREA && node.posInSet);
      case 'setSize':
        return node.htmlAttributes['aria-setsize'] || node.setSize;

      // These attributes default to false for empty strings.
      case 'roleDescription':
        return Boolean(node.roleDescription);
      case 'value':
        return Boolean(node.value);
      case 'selected':
        return node.selected === true;
      default:
        return node[attrib] !== undefined || node.state[attrib];
    }
  }

  /**
   * represents something 'falsey', e.g.: for selected:
   * node.selected === false
   */
  static isFalsey(node, attrib) {
    switch (attrib) {
      case 'selected':
        return node.selected === false;
      default:
        return !Output.isTruthy(node, attrib);
    }
  }

  /**
   * @return {boolean} True if there's any speech that will be output.
   */
  get hasSpeech() {
    for (let i = 0; i < this.speechBuffer_.length; i++) {
      if (this.speechBuffer_[i].length) {
        return true;
      }
    }
    return false;
  }

  /**
   * @return {boolean} True if there is only whitespace in this output.
   */
  get isOnlyWhitespace() {
    return this.speechBuffer_.every(buff => !/\S+/.test(buff.toString()));
  }

  /** @return {Spannable} */
  get braille() {
    return this.mergeBraille_(this.brailleBuffer_);
  }

  /**
   * Specify ranges for speech.
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   * @param {EventType|outputTypes.OutputEventType} type
   * @return {!Output}
   */
  withSpeech(range, prevRange, type) {
    this.formatOptions_ = {speech: true, braille: false, auralStyle: false};
    this.formattedAncestors_ = new WeakSet();
    this.render_(
        range, prevRange, type, this.speechBuffer_, this.speechRulesStr_);
    return this;
  }

  /**
   * Specify ranges for aurally styled speech.
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   * @param {EventType|outputTypes.OutputEventType} type
   * @return {!Output}
   */
  withRichSpeech(range, prevRange, type) {
    this.formatOptions_ = {speech: true, braille: false, auralStyle: true};
    this.formattedAncestors_ = new WeakSet();
    this.render_(
        range, prevRange, type, this.speechBuffer_, this.speechRulesStr_);
    return this;
  }

  /**
   * Specify ranges for braille.
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   * @param {EventType|outputTypes.OutputEventType} type
   * @return {!Output}
   */
  withBraille(range, prevRange, type) {
    this.formatOptions_ = {speech: false, braille: true, auralStyle: false};
    this.formattedAncestors_ = new WeakSet();

    // Braille sometimes shows contextual information depending on role.
    if (range.start.equals(range.end) && range.start.node &&
        AutomationPredicate.contextualBraille(range.start.node) &&
        range.start.node.parent) {
      let start = range.start.node.parent;
      while (start.firstChild) {
        start = start.firstChild;
      }
      let end = range.start.node.parent;
      while (end.lastChild) {
        end = end.lastChild;
      }
      prevRange = CursorRange.fromNode(range.start.node.parent);
      range = new CursorRange(Cursor.fromNode(start), Cursor.fromNode(end));
    }
    this.render_(
        range, prevRange, type, this.brailleBuffer_, this.brailleRulesStr_);
    return this;
  }

  /**
   * Specify ranges for location.
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   * @param {EventType|outputTypes.OutputEventType} type
   * @return {!Output}
   */
  withLocation(range, prevRange, type) {
    this.formatOptions_ = {speech: false, braille: false, auralStyle: false};
    this.formattedAncestors_ = new WeakSet();
    this.render_(
        range, prevRange, type, [] /*unused output*/,
        new OutputRulesStr('') /*unused log*/);
    return this;
  }

  /**
   * Specify the same ranges for speech and braille.
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   * @param {EventType|outputTypes.OutputEventType} type
   * @return {!Output}
   */
  withSpeechAndBraille(range, prevRange, type) {
    this.withSpeech(range, prevRange, type);
    this.withBraille(range, prevRange, type);
    return this;
  }

  /**
   * Specify the same ranges for aurally styled speech and braille.
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   * @param {EventType|outputTypes.OutputEventType} type
   * @return {!Output}
   */
  withRichSpeechAndBraille(range, prevRange, type) {
    this.withRichSpeech(range, prevRange, type);
    this.withBraille(range, prevRange, type);
    return this;
  }

  /**
   * Applies the given speech category to the output.
   * @param {TtsCategory} category
   * @return {!Output}
   */
  withSpeechCategory(category) {
    this.speechCategory_ = category;
    return this;
  }

  /**
   * Applies the given speech queue mode to the output.
   * @param {QueueMode} queueMode The queueMode for the speech.
   * @return {!Output}
   */
  withQueueMode(queueMode) {
    this.queueMode_ = queueMode;
    return this;
  }

  /**
   * Output a string literal.
   * @param {string} value
   * @return {!Output}
   */
  withString(value) {
    this.append_(this.speechBuffer_, value);
    this.append_(this.brailleBuffer_, value);
    this.speechRulesStr_.write('withString: ' + value + '\n');
    this.brailleRulesStr_.write('withString: ' + value + '\n');
    return this;
  }

  /**
   * Outputs formatting nodes after this will contain context first.
   * @return {!Output}
   */
  withContextFirst() {
    this.contextOrder_ = outputTypes.OutputContextOrder.FIRST;
    return this;
  }

  /**
   * Don't include hints in subsequent output.
   * @return {!Output}
   */
  withoutHints() {
    this.enableHints_ = false;
    return this;
  }

  /**
   * Don't draw a focus ring based on this output.
   * @return {!Output}
   */
  withoutFocusRing() {
    this.drawFocusRing_ = false;
    return this;
  }

  /**
   * Supply initial speech properties that will be applied to all output.
   * @param {!TtsSpeechProperties} speechProps
   * @return {!Output}
   */
  withInitialSpeechProperties(speechProps) {
    this.initialSpeechProps_ = speechProps;
    return this;
  }

  /**
   * Causes any speech output to apply the replacement.
   * @param {string} text The text to be replaced.
   * @param {string} replace What to replace |text| with.
   */
  withSpeechTextReplacement(text, replace) {
    this.replacements_[text] = replace;
  }

  /**
   * Suppresses processing of a token for subsequent formatting commands.
   * @param {string} token
   * @return {!Output}
   */
  suppress(token) {
    this.suppressions_[token] = true;
    return this;
  }

  /**
   * Apply a format string directly to the output buffer. This lets you
   * output a message directly to the buffer using the format syntax.
   * @param {string} formatStr
   * @param {!AutomationNode=} opt_node An optional node to apply the
   *     formatting to.
   * @return {!Output} |this| for chaining
   */
  format(formatStr, opt_node) {
    return this.formatForSpeech(formatStr, opt_node)
        .formatForBraille(formatStr, opt_node);
  }

  /**
   * Apply a format string directly to the speech output buffer. This lets you
   * output a message directly to the buffer using the format syntax.
   * @param {string} formatStr
   * @param {!AutomationNode=} opt_node An optional node to apply the
   *     formatting to.
   * @return {!Output} |this| for chaining
   */
  formatForSpeech(formatStr, opt_node) {
    const node = opt_node || null;

    this.formatOptions_ = {speech: true, braille: false, auralStyle: false};
    this.formattedAncestors_ = new WeakSet();
    this.format_({
      node,
      outputFormat: formatStr,
      outputBuffer: this.speechBuffer_,
      outputRuleString: this.speechRulesStr_,
    });

    return this;
  }

  /**
   * Apply a format string directly to the braille output buffer. This lets you
   * output a message directly to the buffer using the format syntax.
   * @param {string} formatStr
   * @param {!AutomationNode=} opt_node An optional node to apply the
   *     formatting to.
   * @return {!Output} |this| for chaining
   */
  formatForBraille(formatStr, opt_node) {
    const node = opt_node || null;

    this.formatOptions_ = {speech: false, braille: true, auralStyle: false};
    this.formattedAncestors_ = new WeakSet();
    this.format_({
      node,
      outputFormat: formatStr,
      outputBuffer: this.brailleBuffer_,
      outputRuleString: this.brailleRulesStr_,
    });
    return this;
  }

  /**
   * Triggers callback for a speech event.
   * @param {function()} callback
   * @return {!Output}
   */
  onSpeechEnd(callback) {
    this.speechEndCallback_ =
        /** @type {function(boolean=)} */ (function(opt_cleanupOnly) {
          if (!opt_cleanupOnly) {
            callback();
          }
        }.bind(this));
    return this;
  }

  /** Executes all specified output. */
  go() {
    // Speech.
    let queueMode = this.determineQueueMode_();

    if (this.speechBuffer_.length > 0) {
      Output.forceModeForNextSpeechUtterance_ = undefined;
    }

    let encounteredNonWhitespace = false;
    for (let i = 0; i < this.speechBuffer_.length; i++) {
      const buff = this.speechBuffer_[i];
      const text = buff.toString();

      // Consider empty strings as non-whitespace; they often have earcons
      // associated with them, so need to be "spoken".
      const isNonWhitespace = text === '' || /\S+/.test(text);
      encounteredNonWhitespace = isNonWhitespace || encounteredNonWhitespace;

      // Skip whitespace if we've already encountered non-whitespace. This
      // prevents output like 'foo', 'space', 'bar'.
      if (!isNonWhitespace && encounteredNonWhitespace) {
        continue;
      }

      let speechProps;
      const speechPropsInstance =
          /** @type {outputTypes.OutputSpeechProperties} */ (
              buff.getSpanInstanceOf(outputTypes.OutputSpeechProperties));

      if (!speechPropsInstance) {
        speechProps = this.initialSpeechProps_;
      } else {
        for (const [key, value] of Object.entries(this.initialSpeechProps_)) {
          if (speechPropsInstance.properties[key] === undefined) {
            speechPropsInstance.properties[key] = value;
          }
        }
        speechProps = new TtsSpeechProperties(speechPropsInstance.properties);
      }

      speechProps.category = this.speechCategory_;

      (function() {
        const scopedBuff = buff;
        speechProps.startCallback = function() {
          const actions =
              scopedBuff.getSpansInstanceOf(outputTypes.OutputAction);
          if (actions) {
            actions.forEach(action => action.run());
          }
        };
      }());

      if (i === this.speechBuffer_.length - 1) {
        speechProps.endCallback = this.speechEndCallback_;
      }
      let finalSpeech = buff.toString();
      for (const text in this.replacements_) {
        finalSpeech = finalSpeech.replace(text, this.replacements_[text]);
      }
      ChromeVox.tts.speak(finalSpeech, queueMode, speechProps);

      // Skip resetting |queueMode| if the |text| is empty. If we don't do this,
      // and the tts engine doesn't generate a callback, we might not properly
      // flush.
      if (text !== '') {
        queueMode = QueueMode.QUEUE;
      }
    }
    if (this.speechRulesStr_.str) {
      LogStore.getInstance().writeTextLog(
          this.speechRulesStr_.str, LogType.SPEECH_RULE);
    }

    // Braille.
    if (this.brailleBuffer_.length) {
      const buff = this.mergeBraille_(this.brailleBuffer_);
      const selSpan = buff.getSpanInstanceOf(outputTypes.OutputSelectionSpan);
      let startIndex = -1;
      let endIndex = -1;
      if (selSpan) {
        const valueStart = buff.getSpanStart(selSpan);
        const valueEnd = buff.getSpanEnd(selSpan);
        startIndex = valueStart + selSpan.startIndex;
        endIndex = valueStart + selSpan.endIndex;
        try {
          buff.setSpan(new ValueSpan(0), valueStart, valueEnd);
          buff.setSpan(new ValueSelectionSpan(), startIndex, endIndex);
        } catch (e) {
        }
      }

      const output = new NavBraille({text: buff, startIndex, endIndex});

      ChromeVox.braille.write(output);
      if (this.brailleRulesStr_.str) {
        LogStore.getInstance().writeTextLog(
            this.brailleRulesStr_.str, LogType.BRAILLE_RULE);
      }
    }

    // Display.
    if (this.speechCategory_ !== TtsCategory.LIVE && this.drawFocusRing_) {
      FocusBounds.set(this.locations_);
    }
  }

  /** @return {QueueMode} */
  determineQueueMode_() {
    if (Output.forceModeForNextSpeechUtterance_ !== undefined) {
      return Output.forceModeForNextSpeechUtterance_;
    }
    if (this.queueMode_ !== undefined) {
      return this.queueMode_;
    }
    return QueueMode.QUEUE;
  }

  /**
   * @return {boolean} True if this object is equal to |rhs|.
   */
  equals(rhs) {
    if (this.speechBuffer_.length !== rhs.speechBuffer_.length ||
        this.brailleBuffer_.length !== rhs.brailleBuffer_.length) {
      return false;
    }

    for (let i = 0; i < this.speechBuffer_.length; i++) {
      if (this.speechBuffer_[i].toString() !==
          rhs.speechBuffer_[i].toString()) {
        return false;
      }
    }

    for (let j = 0; j < this.brailleBuffer_.length; j++) {
      if (this.brailleBuffer_[j].toString() !==
          rhs.brailleBuffer_[j].toString()) {
        return false;
      }
    }

    return true;
  }

  /**
   * Renders the given range using optional context previous range and event
   * type.
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   * @param {EventType|outputTypes.OutputEventType} type
   * @param {!Array<Spannable>} buff Buffer to receive rendered output.
   * @param {!OutputRulesStr} ruleStr
   * @param {{suppressStartEndAncestry: (boolean|undefined)}} optionalArgs
   * @private
   */
  render_(range, prevRange, type, buff, ruleStr, optionalArgs = {}) {
    if (prevRange && !prevRange.isValid()) {
      prevRange = null;
    }

    // Scan all ancestors to get the value of |contextOrder|.
    let parent = range.start.node;
    const prevParent = prevRange ? prevRange.start.node : parent;
    if (!parent || !prevParent) {
      return;
    }

    while (parent) {
      if (parent.role === RoleType.WINDOW) {
        break;
      }
      if (OutputRoleInfo[parent.role] &&
          OutputRoleInfo[parent.role].contextOrder) {
        this.contextOrder_ =
            OutputRoleInfo[parent.role].contextOrder || this.contextOrder_;
        break;
      }

      parent = parent.parent;
    }

    if (range.isSubNode()) {
      this.subNode_(range, prevRange, type, buff, ruleStr);
    } else {
      this.range_(range, prevRange, type, buff, ruleStr, optionalArgs);
    }

    this.hint_(
        range, AutomationUtil.getUniqueAncestors(prevParent, range.start.node),
        type, buff, ruleStr);
  }

  /**
   * Format the node given the format specifier.
   * Please see below for more information on arguments.
   * node: The AutomationNode of interest.
   * outputFormat: The output format either specified as an output template
   * string or a parsed output format tree.
   * outputBuffer: Buffer to receive rendered output.
   * outputRuleString: Used for logging and recording output.
   * opt_prevNode: Optional argument. Helps provide context for certain speech
   * output.
   * opt_speechProps: Optional argument. Used to specify how speech should be
   * verbalized; can specify pitch, rate, language, etc.
   * @param {!{
   *    node: AutomationNode,
   *    outputFormat: (string|OutputFormatTree),
   *    outputBuffer: !Array<Spannable>,
   *    outputRuleString: !OutputRulesStr,
   *    opt_prevNode: (!AutomationNode|undefined),
   *    opt_speechProps: (!outputTypes.OutputSpeechProperties|undefined)
   * }} params An object containing all required and optional parameters.
   * @private
   */
  format_(params) {
    const node = params['node'];
    const format = params['outputFormat'];
    const buff = params['outputBuffer'];
    const ruleStr = params['outputRuleString'];
    const prevNode = params['opt_prevNode'];
    let speechProps = params['opt_speechProps'];
    const owner = this;
    const observer =
        new /** @implements {OutputFormatParserObserver} */ (class {
          /** @override */
          onTokenStart() {}

          /** @override */
          onNodeAttributeOrSpecialToken(token, tree, options) {
            if (owner.suppressions_[token]) {
              return true;
            }

            if (token === 'value') {
              owner.formatValue_(node, token, buff, options, ruleStr);
            } else if (token === 'name') {
              owner.formatName_(node, prevNode, token, buff, options, ruleStr);
            } else if (token === 'description') {
              owner.formatDescription_(node, token, buff, options, ruleStr);
            } else if (token === 'urlFilename') {
              owner.formatUrlFilename_(node, token, buff, options, ruleStr);
            } else if (token === 'nameFromNode') {
              owner.formatNameFromNode_(node, token, buff, options, ruleStr);
            } else if (token === 'nameOrDescendants') {
              // This token is similar to nameOrTextContent except it gathers
              // rich output for descendants. It also lets name from contents
              // override the descendants text if |node| has only static text
              // children.
              owner.formatNameOrDescendants_(
                  node, token, buff, options, ruleStr);
            } else if (token === 'indexInParent') {
              owner.formatIndexInParent_(
                  node, token, tree, buff, options, ruleStr);
            } else if (token === 'restriction') {
              owner.formatRestriction_(node, token, buff, ruleStr);
            } else if (token === 'checked') {
              owner.formatChecked_(node, token, buff, ruleStr);
            } else if (token === 'pressed') {
              owner.formatPressed_(node, token, buff, ruleStr);
            } else if (token === 'state') {
              owner.formatState_(node, token, buff, ruleStr);
            } else if (token === 'find') {
              owner.formatFind_(node, token, tree, buff, ruleStr);
            } else if (token === 'descendants') {
              owner.formatDescendants_(node, token, buff, ruleStr);
            } else if (token === 'joinedDescendants') {
              owner.formatJoinedDescendants_(
                  node, token, buff, options, ruleStr);
            } else if (token === 'role') {
              if (localStorage['useVerboseMode'] === 'false') {
                return true;
              }
              if (owner.formatOptions_.auralStyle) {
                speechProps = new outputTypes.OutputSpeechProperties();
                speechProps.properties['relativePitch'] = -0.3;
              }

              owner.formatRole_(node, token, buff, options, ruleStr);
            } else if (token === 'inputType') {
              owner.formatInputType_(node, token, buff, options, ruleStr);
            } else if (
                token === 'tableCellRowIndex' ||
                token === 'tableCellColumnIndex') {
              owner.formatTableCellIndex_(node, token, buff, options, ruleStr);
            } else if (token === 'cellIndexText') {
              owner.formatCellIndexText_(node, token, buff, options, ruleStr);
            } else if (token === 'node') {
              owner.formatNode_(
                  node, prevNode, token, tree, buff, options, ruleStr);
            } else if (
                token === 'nameOrTextContent' || token === 'textContent') {
              owner.formatTextContent_(node, token, buff, options, ruleStr);
            } else if (node[token] !== undefined) {
              owner.formatAsFieldAccessor_(node, token, buff, options, ruleStr);
            } else if (outputTypes.OUTPUT_STATE_INFO[token]) {
              owner.formatAsStateValue_(node, token, buff, options, ruleStr);
            } else if (token === 'phoneticReading') {
              owner.formatPhoneticReading_(node, buff);
            } else if (token === 'listNestedLevel') {
              owner.formatListNestedLevel_(node, buff);
            } else if (token === 'precedingBullet') {
              owner.formatPrecedingBullet_(node, buff);
            } else if (tree.firstChild) {
              owner.formatCustomFunction_(
                  node, token, tree, buff, options, ruleStr);
            }
          }

          /** @override */
          onMessageToken(token, tree, options) {
            ruleStr.write(' @');
            if (owner.formatOptions_.auralStyle) {
              if (!speechProps) {
                speechProps = new outputTypes.OutputSpeechProperties();
              }
              speechProps.properties['relativePitch'] = -0.2;
            }
            owner.formatMessage_(node, token, tree, buff, options, ruleStr);
          }

          /** @override */
          onSpeechPropertyToken(token, tree, options) {
            ruleStr.write(' ! ' + token + '\n');
            speechProps = new outputTypes.OutputSpeechProperties();
            speechProps.properties[token] = true;
            if (tree.firstChild) {
              if (!owner.formatOptions_.auralStyle) {
                speechProps = undefined;
                return true;
              }

              let value = tree.firstChild.value;

              // Currently, speech params take either attributes or floats.
              let float = 0;
              if (float = parseFloat(value)) {
                value = float;
              } else {
                value = parseFloat(node[value]) / -10.0;
              }
              speechProps.properties[token] = value;
              return true;
            }
          }

          /** @override */
          onTokenEnd() {
            // Post processing.
            if (speechProps) {
              if (buff.length > 0) {
                buff[buff.length - 1].setSpan(speechProps, 0, 0);
                speechProps = null;
              }
            }
          }
        })();

    new OutputFormatParser(observer).parse(format);
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @param {!OutputRulesStr} ruleStr
   */
  formatValue_(node, token, buff, options, ruleStr) {
    const text = node.value || '';
    if (!node.state[StateType.EDITABLE] && node.name === text) {
      return;
    }

    let selectedText = '';
    if (node.textSelStart !== undefined) {
      options.annotation.push(new outputTypes.OutputSelectionSpan(
          node.textSelStart || 0, node.textSelEnd || 0));

      if (node.value) {
        selectedText =
            node.value.substring(node.textSelStart || 0, node.textSelEnd || 0);
      }
    }
    options.annotation.push(token);
    if (selectedText && !this.formatOptions_.braille &&
        node.state[StateType.FOCUSED]) {
      this.append_(buff, selectedText, options);
      this.append_(buff, Msgs.getMsg('selected'));
      ruleStr.writeTokenWithValue(token, selectedText);
      ruleStr.write('selected\n');
    } else {
      this.append_(buff, text, options);
      ruleStr.writeTokenWithValue(token, text);
    }
  }

  /**
   * @param {AutomationNode} node
   * @param {!AutomationNode|undefined} prevNode
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @param {!OutputRulesStr} ruleStr
   */
  formatName_(node, prevNode, token, buff, options, ruleStr) {
    options.annotation.push(token);
    const earcon = node ? this.findEarcon_(node, prevNode) : null;
    if (earcon) {
      options.annotation.push(earcon);
    }

    // Place the selection on the first character of the name if the
    // node is the active descendant. This ensures the braille window is
    // panned appropriately.
    if (node.activeDescendantFor && node.activeDescendantFor.length > 0) {
      options.annotation.push(new outputTypes.OutputSelectionSpan(0, 0));
    }

    if (localStorage['languageSwitching'] === 'true') {
      this.assignLocaleAndAppend_(node.name || '', node, buff, options);
    } else {
      this.append_(buff, node.name || '', options);
    }

    ruleStr.writeTokenWithValue(token, node.name);
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @param {!OutputRulesStr} ruleStr
   */
  formatDescription_(node, token, buff, options, ruleStr) {
    if (node.name === node.description) {
      return;
    }

    options.annotation.push(token);
    this.append_(buff, node.description || '', options);
    ruleStr.writeTokenWithValue(token, node.description);
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @param {!OutputRulesStr} ruleStr
   */
  formatUrlFilename_(node, token, buff, options, ruleStr) {
    options.annotation.push('name');
    const url = node.url || '';
    let filename = '';
    if (url.substring(0, 4) !== 'data') {
      filename = url.substring(url.lastIndexOf('/') + 1, url.lastIndexOf('.'));

      // Hack to not speak the filename if it's ridiculously long.
      if (filename.length >= 30) {
        filename = filename.substring(0, 16) + '...';
      }
    }
    this.append_(buff, filename, options);
    ruleStr.writeTokenWithValue(token, filename);
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @param {!OutputRulesStr} ruleStr
   */
  formatNameFromNode_(node, token, buff, options, ruleStr) {
    if (node.nameFrom === NameFromType.CONTENTS) {
      return;
    }

    options.annotation.push('name');
    this.append_(buff, node.name || '', options);
    ruleStr.writeTokenWithValue(token, node.name);
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @param {!OutputRulesStr} ruleStr
   */
  formatNameOrDescendants_(node, token, buff, options, ruleStr) {
    options.annotation.push(token);
    if (node.name &&
        (node.nameFrom !== NameFromType.CONTENTS ||
         node.children.every(child => child.role === RoleType.STATIC_TEXT))) {
      this.append_(buff, node.name || '', options);
      ruleStr.writeTokenWithValue(token, node.name);
    } else {
      ruleStr.writeToken(token);
      this.format_({
        node,
        outputFormat: '$descendants',
        outputBuffer: buff,
        outputRuleString: ruleStr,
      });
    }
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!OutputFormatTree} tree
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @param {!OutputRulesStr} ruleStr
   */
  formatIndexInParent_(node, token, tree, buff, options, ruleStr) {
    if (node.parent) {
      options.annotation.push(token);
      let roles;
      if (tree.firstChild) {
        roles = this.createRoles_(tree);
      } else {
        roles = new Set();
        roles.add(node.role);
      }

      let count = 0;
      for (let i = 0, child; child = node.parent.children[i]; i++) {
        if (roles.has(child.role)) {
          count++;
        }
        if (node === child) {
          break;
        }
      }
      this.append_(buff, String(count));
      ruleStr.writeTokenWithValue(token, String(count));
    }
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!OutputRulesStr} ruleStr
   */
  formatRestriction_(node, token, buff, ruleStr) {
    const msg = outputTypes.OutputPropertyMap.RESTRICTION[node.restriction];
    if (msg) {
      ruleStr.writeToken(token);
      this.format_({
        node,
        outputFormat: '@' + msg,
        outputBuffer: buff,
        outputRuleString: ruleStr,
      });
    }
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!OutputRulesStr} ruleStr
   */
  formatChecked_(node, token, buff, ruleStr) {
    const msg = outputTypes.OutputPropertyMap.CHECKED[node.checked];
    if (msg) {
      ruleStr.writeToken(token);
      this.format_({
        node,
        outputFormat: '@' + msg,
        outputBuffer: buff,
        outputRuleString: ruleStr,
      });
    }
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!OutputRulesStr} ruleStr
   */
  formatPressed_(node, token, buff, ruleStr) {
    const msg = outputTypes.OutputPropertyMap.PRESSED[node.checked];
    if (msg) {
      ruleStr.writeToken(token);
      this.format_({
        node,
        outputFormat: '@' + msg,
        outputBuffer: buff,
        outputRuleString: ruleStr,
      });
    }
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!OutputRulesStr} ruleStr
   */
  formatState_(node, token, buff, ruleStr) {
    if (node.state) {
      Object.getOwnPropertyNames(node.state).forEach(state => {
        const stateInfo = outputTypes.OUTPUT_STATE_INFO[state];
        if (stateInfo && !stateInfo.isRoleSpecific && stateInfo.on) {
          ruleStr.writeToken(token);
          this.format_({
            node,
            outputFormat: '$' + state,
            outputBuffer: buff,
            outputRuleString: ruleStr,
          });
        }
      });
    }
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!OutputFormatTree} tree
   * @param {!Array<Spannable>} buff
   * @param {!OutputRulesStr} ruleStr
   */
  formatFind_(node, token, tree, buff, ruleStr) {
    // Find takes two arguments: JSON query string and format string.
    if (tree.firstChild) {
      const jsonQuery = tree.firstChild.value;
      node = node.find(
          /** @type {chrome.automation.FindParams}*/ (JSON.parse(jsonQuery)));
      const formatString = tree.firstChild.nextSibling || '';
      if (node) {
        ruleStr.writeToken(token);
        this.format_({
          node,
          outputFormat: formatString,
          outputBuffer: buff,
          outputRuleString: ruleStr,
        });
      }
    }
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!OutputRulesStr} ruleStr
   */
  formatDescendants_(node, token, buff, ruleStr) {
    if (!node) {
      return;
    }

    let leftmost = node;
    let rightmost = node;
    if (AutomationPredicate.leafOrStaticText(node)) {
      // Find any deeper leaves, if any, by starting from one level
      // down.
      leftmost = node.firstChild;
      rightmost = node.lastChild;
      if (!leftmost || !rightmost) {
        return;
      }
    }

    // Construct a range to the leftmost and rightmost leaves. This
    // range gets rendered below which results in output that is the
    // same as if a user navigated through the entire subtree of |node|.
    leftmost = AutomationUtil.findNodePre(
        leftmost, Dir.FORWARD, AutomationPredicate.leafOrStaticText);
    rightmost = AutomationUtil.findNodePre(
        rightmost, Dir.BACKWARD, AutomationPredicate.leafOrStaticText);
    if (!leftmost || !rightmost) {
      return;
    }

    const subrange = new CursorRange(
        new Cursor(leftmost, CURSOR_NODE_INDEX),
        new Cursor(rightmost, CURSOR_NODE_INDEX));
    let prev = null;
    if (node) {
      prev = CursorRange.fromNode(node);
    }
    ruleStr.writeToken(token);
    this.render_(
        subrange, prev, outputTypes.OutputEventType.NAVIGATE, buff, ruleStr,
        {suppressStartEndAncestry: true});
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @param {!OutputRulesStr} ruleStr
   */
  formatJoinedDescendants_(node, token, buff, options, ruleStr) {
    const unjoined = [];
    ruleStr.write('joinedDescendants {');
    this.format_({
      node,
      outputFormat: '$descendants',
      outputBuffer: unjoined,
      outputRuleString: ruleStr,
    });
    this.append_(buff, unjoined.join(' '), options);
    ruleStr.write(
        '}: ' + (unjoined.length ? unjoined.join(' ') : 'EMPTY') + '\n');
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @param {!OutputRulesStr} ruleStr
   */
  formatRole_(node, token, buff, options, ruleStr) {
    options.annotation.push(token);
    let msg = node.role;
    const info = OutputRoleInfo[node.role];
    if (node.roleDescription) {
      msg = node.roleDescription;
    } else if (info) {
      if (this.formatOptions_.braille) {
        msg = Msgs.getMsg(info.msgId + '_brl');
      } else if (info.msgId) {
        msg = Msgs.getMsg(info.msgId);
      }
    } else {
      // We can safely ignore this role. ChromeVox output tests cover
      // message id validity.
      return;
    }
    this.append_(buff, msg || '', options);
    ruleStr.writeTokenWithValue(token, msg);
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @param {!OutputRulesStr} ruleStr
   */
  formatInputType_(node, token, buff, options, ruleStr) {
    if (!node.inputType) {
      return;
    }
    options.annotation.push(token);
    let msgId =
        outputTypes.INPUT_TYPE_MESSAGE_IDS[node.inputType] || 'input_type_text';
    if (this.formatOptions_.braille) {
      msgId = msgId + '_brl';
    }
    this.append_(buff, Msgs.getMsg(msgId), options);
    ruleStr.writeTokenWithValue(token, Msgs.getMsg(msgId));
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @param {!OutputRulesStr} ruleStr
   */
  formatTableCellIndex_(node, token, buff, options, ruleStr) {
    let value = node[token];
    if (value === undefined) {
      return;
    }
    value = String(value + 1);
    options.annotation.push(token);
    this.append_(buff, value, options);
    ruleStr.writeTokenWithValue(token, value);
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @param {!OutputRulesStr} ruleStr
   */
  formatCellIndexText_(node, token, buff, options, ruleStr) {
    if (node.htmlAttributes['aria-coltext']) {
      let value = node.htmlAttributes['aria-coltext'];
      let row = node;
      while (row && row.role !== RoleType.ROW) {
        row = row.parent;
      }
      if (!row || !row.htmlAttributes['aria-rowtext']) {
        return;
      }
      value += row.htmlAttributes['aria-rowtext'];
      this.append_(buff, value, options);
      ruleStr.writeTokenWithValue(token, value);
    } else {
      ruleStr.write(token);
      this.format_({
        node,
        outputFormat: ` @cell_summary($if($tableCellAriaRowIndex,
                  $tableCellAriaRowIndex, $tableCellRowIndex),
                $if($tableCellAriaColumnIndex, $tableCellAriaColumnIndex,
                  $tableCellColumnIndex))`,
        outputBuffer: buff,
        outputRuleString: ruleStr,
      });
    }
  }

  /**
   * @param {AutomationNode} node
   * @param {!AutomationNode|undefined} prevNode
   * @param {string} token
   * @param {!OutputFormatTree} tree
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @param {!OutputRulesStr} ruleStr
   */
  formatNode_(node, prevNode, token, tree, buff, options, ruleStr) {
    if (!tree.firstChild) {
      return;
    }

    const relationName = tree.firstChild.value;
    if (relationName === 'tableCellColumnHeaders') {
      // Skip output when previous position falls on the same column.
      while (prevNode && !AutomationPredicate.cellLike(prevNode)) {
        prevNode = prevNode.parent;
      }
      if (prevNode &&
          prevNode.tableCellColumnIndex === node.tableCellColumnIndex) {
        return;
      }

      const headers = node.tableCellColumnHeaders;
      if (headers) {
        for (let i = 0; i < headers.length; i++) {
          const header = headers[i].name;
          if (header) {
            this.append_(buff, header, options);
            ruleStr.writeTokenWithValue(token, header);
          }
        }
      }
    } else if (relationName === 'tableCellRowHeaders') {
      const headers = node.tableCellRowHeaders;
      if (headers) {
        for (let i = 0; i < headers.length; i++) {
          const header = headers[i].name;
          if (header) {
            this.append_(buff, header, options);
            ruleStr.writeTokenWithValue(token, header);
          }
        }
      }
    } else if (node[relationName]) {
      const related = node[relationName];
      this.node_(
          related, related, outputTypes.OutputEventType.NAVIGATE, buff,
          ruleStr);
    }
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @param {!OutputRulesStr} ruleStr
   */
  formatTextContent_(node, token, buff, options, ruleStr) {
    if (node.name && token === 'nameOrTextContent') {
      ruleStr.writeToken(token);
      this.format_({
        node,
        outputFormat: '$name',
        outputBuffer: buff,
        outputRuleString: ruleStr,
      });
      return;
    }

    if (!node.firstChild) {
      return;
    }

    const root = node;
    const walker = new AutomationTreeWalker(node, Dir.FORWARD, {
      visit: AutomationPredicate.leafOrStaticText,
      leaf: n => {
        // The root might be a leaf itself, but we still want to descend
        // into it.
        return n !== root && AutomationPredicate.leafOrStaticText(n);
      },
      root: r => r === root,
    });
    const outputStrings = [];
    while (walker.next().node) {
      if (walker.node.name) {
        outputStrings.push(walker.node.name.trim());
      }
    }
    const finalOutput = outputStrings.join(' ');
    this.append_(buff, finalOutput, options);
    ruleStr.writeTokenWithValue(token, finalOutput);
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @param {!OutputRulesStr} ruleStr
   */
  formatAsFieldAccessor_(node, token, buff, options, ruleStr) {
    options.annotation.push(token);
    let value = node[token];
    if (typeof value === 'number') {
      value = String(value);
    }
    this.append_(buff, value, options);
    ruleStr.writeTokenWithValue(token, value);
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @param {!OutputRulesStr} ruleStr
   */
  formatAsStateValue_(node, token, buff, options, ruleStr) {
    options.annotation.push('state');
    const stateInfo = outputTypes.OUTPUT_STATE_INFO[token];
    let resolvedInfo = {};
    resolvedInfo = node.state[/** @type {StateType} */ (token)] ? stateInfo.on :
                                                                  stateInfo.off;
    if (!resolvedInfo) {
      return;
    }
    if (this.formatOptions_.speech && resolvedInfo.earconId) {
      options.annotation.push(
          new outputTypes.OutputEarconAction(resolvedInfo.earconId),
          node.location || undefined);
    }
    const msgId = this.formatOptions_.braille ? resolvedInfo.msgId + '_brl' :
                                                resolvedInfo.msgId;
    const msg = Msgs.getMsg(msgId);
    this.append_(buff, msg, options);
    ruleStr.writeTokenWithValue(token, msg);
  }

  /**
   * @param {AutomationNode} node
   * @param {!Array<Spannable>} buff
   */
  formatPhoneticReading_(node, buff) {
    const text =
        PhoneticData.forText(node.name || '', chrome.i18n.getUILanguage());
    this.append_(buff, text);
  }

  /**
   * @param {!AutomationNode} node
   * @param {!Array<Spannable>} buff
   */
  formatListNestedLevel_(node, buff) {
    let level = 0;
    let current = node;
    while (current) {
      if (current.role === RoleType.LIST) {
        level += 1;
      }
      current = current.parent;
    }
    this.append_(buff, level.toString());
  }

  /**
   * @param {!AutomationNode} node
   * @param {!Array<Spannable>} buff
   */
  formatPrecedingBullet_(node, buff) {
    let current = node;
    if (current.role === RoleType.INLINE_TEXT_BOX) {
      current = current.parent;
    }
    if (!current || current.role !== RoleType.STATIC_TEXT) {
      return;
    }
    current = current.previousSibling;
    if (current && current.role === RoleType.LIST_MARKER) {
      this.append_(buff, current.name || '');
    }
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!OutputFormatTree} tree
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @param {!OutputRulesStr} ruleStr
   */
  formatCustomFunction_(node, token, tree, buff, options, ruleStr) {
    // Custom functions.
    if (token === 'if') {
      ruleStr.writeToken(token);
      const cond = tree.firstChild;
      const attrib = cond.value.slice(1);
      if (Output.isTruthy(node, attrib)) {
        ruleStr.write(attrib + '==true => ');
        this.format_({
          node,
          outputFormat: cond.nextSibling || '',
          outputBuffer: buff,
          outputRuleString: ruleStr,
        });
      } else if (Output.isFalsey(node, attrib)) {
        ruleStr.write(attrib + '==false => ');
        this.format_({
          node,
          outputFormat: cond.nextSibling.nextSibling || '',
          outputBuffer: buff,
          outputRuleString: ruleStr,
        });
      }
    } else if (token === 'nif') {
      ruleStr.writeToken(token);
      const cond = tree.firstChild;
      const attrib = cond.value.slice(1);
      if (Output.isFalsey(node, attrib)) {
        ruleStr.write(attrib + '==false => ');
        this.format_({
          node,
          outputFormat: cond.nextSibling || '',
          outputBuffer: buff,
          outputRuleString: ruleStr,
        });
      } else if (Output.isTruthy(node, attrib)) {
        ruleStr.write(attrib + '==true => ');
        this.format_({
          node,
          outputFormat: cond.nextSibling.nextSibling || '',
          outputBuffer: buff,
          outputRuleString: ruleStr,
        });
      }
    } else if (token === 'earcon') {
      // Ignore unless we're generating speech output.
      if (!this.formatOptions_.speech) {
        return;
      }

      options.annotation.push(new outputTypes.OutputEarconAction(
          tree.firstChild.value, node.location || undefined));
      this.append_(buff, '', options);
      ruleStr.writeTokenWithValue(token, tree.firstChild.value);
    }
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!OutputFormatTree} tree
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @param {!OutputRulesStr} ruleStr
   */
  formatMessage_(node, token, tree, buff, options, ruleStr) {
    const isPluralized = (token[0] === '@');
    if (isPluralized) {
      token = token.slice(1);
    }
    // Tokens can have substitutions.
    const pieces = token.split('+');
    token = pieces.reduce((prev, cur) => {
      let lookup = cur;
      if (cur[0] === '$') {
        lookup = node[cur.slice(1)];
      }
      return prev + lookup;
    }, '');
    const msgId = token;
    let msgArgs = [];
    ruleStr.write(token + '{');
    if (!isPluralized) {
      let curArg = tree.firstChild;
      while (curArg) {
        if (curArg.value[0] !== '$') {
          const errorMsg = 'Unexpected value: ' + curArg.value;
          ruleStr.writeError(errorMsg);
          console.error(errorMsg);
          return;
        }
        let msgBuff = [];
        this.format_({
          node,
          outputFormat: curArg,
          outputBuffer: msgBuff,
          outputRuleString: ruleStr,
        });
        // Fill in empty string if nothing was formatted.
        if (!msgBuff.length) {
          msgBuff = [''];
        }
        msgArgs = msgArgs.concat(msgBuff);
        curArg = curArg.nextSibling;
      }
    }
    let msg = Msgs.getMsg(msgId, msgArgs);
    try {
      if (this.formatOptions_.braille) {
        msg = Msgs.getMsg(msgId + '_brl', msgArgs) || msg;
      }
    } catch (e) {
    }

    if (!msg) {
      const errorMsg = 'Could not get message ' + msgId;
      ruleStr.writeError(errorMsg);
      console.error(errorMsg);
      return;
    }

    if (isPluralized) {
      const arg = tree.firstChild;
      if (!arg || arg.nextSibling) {
        const errorMsg = 'Pluralized messages take exactly one argument';
        ruleStr.writeError(errorMsg);
        console.error(errorMsg);
        return;
      }
      if (arg.value[0] !== '$') {
        const errorMsg = 'Unexpected value: ' + arg.value;
        ruleStr.writeError(errorMsg);
        console.error(errorMsg);
        return;
      }
      const argBuff = [];
      this.format_({
        node,
        outputFormat: arg,
        outputBuffer: argBuff,
        outputRuleString: ruleStr,
      });
      const namedArgs = {COUNT: Number(argBuff[0])};
      msg = new goog.i18n.MessageFormat(msg).format(namedArgs);
    }
    ruleStr.write('}');

    this.append_(buff, msg, options);
    ruleStr.write(': ' + msg + '\n');
  }

  /**
   * @param {!OutputFormatTree} tree
   * @return {!Set}
   * @private
   */
  createRoles_(tree) {
    const roles = new Set();
    let currentNode = tree.firstChild;
    for (; currentNode; currentNode = currentNode.nextSibling) {
      roles.add(currentNode.value);
    }
    return roles;
  }

  /**
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   * @param {EventType|outputTypes.OutputEventType} type
   * @param {!Array<Spannable>} rangeBuff
   * @param {!OutputRulesStr} ruleStr
   * @param {{suppressStartEndAncestry: (boolean|undefined)}} optionalArgs
   * @private
   */
  range_(range, prevRange, type, rangeBuff, ruleStr, optionalArgs = {}) {
    if (!range.start.node || !range.end.node) {
      return;
    }

    if (!prevRange && range.start.node.root) {
      prevRange = CursorRange.fromNode(range.start.node.root);
    } else if (!prevRange) {
      return;
    }
    const isForward = prevRange.compare(range) === Dir.FORWARD;
    const addContextBefore =
        this.contextOrder_ === outputTypes.OutputContextOrder.FIRST ||
        this.contextOrder_ === outputTypes.OutputContextOrder.FIRST_AND_LAST ||
        (this.contextOrder_ === outputTypes.OutputContextOrder.DIRECTED &&
         isForward);
    const addContextAfter =
        this.contextOrder_ === outputTypes.OutputContextOrder.LAST ||
        this.contextOrder_ === outputTypes.OutputContextOrder.FIRST_AND_LAST ||
        (this.contextOrder_ === outputTypes.OutputContextOrder.DIRECTED &&
         !isForward);
    const preferStartOrEndAncestry =
        this.contextOrder_ === outputTypes.OutputContextOrder.FIRST_AND_LAST;
    let prevNode = prevRange.start.node;
    let node = range.start.node;

    const formatNodeAndAncestors = function(node, prevNode) {
      const buff = [];

      if (addContextBefore) {
        this.ancestry_(
            node, prevNode, type, buff, ruleStr,
            {preferStart: preferStartOrEndAncestry});
      }
      this.node_(node, prevNode, type, buff, ruleStr);
      if (addContextAfter) {
        this.ancestry_(
            node, prevNode, type, buff, ruleStr,
            {preferEnd: preferStartOrEndAncestry});
      }
      if (node.location) {
        this.locations_.push(node.location);
      }
      return buff;
    }.bind(this);

    let lca = null;
    if (range.start.node !== range.end.node) {
      lca = AutomationUtil.getLeastCommonAncestor(
          range.end.node, range.start.node);
    }
    if (addContextAfter) {
      prevNode = lca || prevNode;
    }

    // Do some bookkeeping to see whether this range partially covers node(s) at
    // its endpoints.
    let hasPartialNodeStart = false;
    let hasPartialNodeEnd = false;
    if (AutomationPredicate.selectableText(range.start.node) &&
        range.start.index > 0) {
      hasPartialNodeStart = true;
    }

    if (AutomationPredicate.selectableText(range.end.node) &&
        range.end.index >= 0 && range.end.index < range.end.node.name.length) {
      hasPartialNodeEnd = true;
    }

    let pred;
    if (range.isInlineText()) {
      pred = AutomationPredicate.leaf;
    } else if (hasPartialNodeStart || hasPartialNodeEnd) {
      pred = AutomationPredicate.selectableText;
    } else {
      pred = AutomationPredicate.object;
    }

    // Computes output for nodes (including partial subnodes) between endpoints
    // of |range|.
    while (node && range.end.node &&
           AutomationUtil.getDirection(node, range.end.node) === Dir.FORWARD) {
      if (hasPartialNodeStart && node === range.start.node) {
        if (range.start.index !== range.start.node.name.length) {
          const partialRange = new CursorRange(
              new Cursor(node, range.start.index),
              new Cursor(
                  node, node.name.length, {preferNodeStartEquivalent: true}));
          this.subNode_(partialRange, prevRange, type, rangeBuff, ruleStr);
        }
      } else if (hasPartialNodeEnd && node === range.end.node) {
        if (range.end.index !== 0) {
          const partialRange = new CursorRange(
              new Cursor(node, 0), new Cursor(node, range.end.index));
          this.subNode_(partialRange, prevRange, type, rangeBuff, ruleStr);
        }
      } else {
        rangeBuff.push.apply(rangeBuff, formatNodeAndAncestors(node, prevNode));
      }

      // End early if the range is just a single node.
      if (range.start.node === range.end.node) {
        break;
      }

      prevNode = node;
      node = AutomationUtil.findNextNode(
                 node, Dir.FORWARD, pred, {root: r => r === lca}) ||
          prevNode;

      // Reached a boundary.
      if (node === prevNode) {
        break;
      }
    }

    // Finally, add on ancestry announcements, if needed.
    if (addContextAfter) {
      // No lca; the range was already fully described.
      if (lca == null || !prevRange.start.node) {
        return;
      }

      // Since the lca itself needs to be part of the ancestry output, use its
      // first child as a target.
      const target = lca.firstChild || lca;
      this.ancestry_(target, prevRange.start.node, type, rangeBuff, ruleStr);
    }
  }

  /**
   * @param {!AutomationNode} node
   * @param {!AutomationNode} prevNode
   * @param {EventType|outputTypes.OutputEventType} type
   * @param {!Array<Spannable>} buff
   * @param {!OutputRulesStr} ruleStr
   * @param {{suppressStartEndAncestry: (boolean|undefined),
   *         preferStart: (boolean|undefined),
   *         preferEnd: (boolean|undefined)
   *        }} optionalArgs
   * @private
   */
  ancestry_(node, prevNode, type, buff, ruleStr, optionalArgs = {}) {
    if (localStorage['useVerboseMode'] === 'false') {
      return;
    }

    if (OutputRoleInfo[node.role] && OutputRoleInfo[node.role].ignoreAncestry) {
      return;
    }

    const info = new OutputAncestryInfo(
        node, prevNode, Boolean(optionalArgs.suppressStartEndAncestry));

    // Enter, leave ancestry.
    this.ancestryHelper_({
      node,
      prevNode,
      buff,
      ruleStr,
      type,
      ancestors: info.leaveAncestors,
      formatName: 'leave',
      exclude: [...info.enterAncestors, node],
    });
    this.ancestryHelper_({
      node,
      prevNode,
      buff,
      ruleStr,
      type,
      ancestors: info.enterAncestors,
      formatName: 'enter',
      excludePreviousAncestors: true,
    });

    if (optionalArgs.suppressStartEndAncestry) {
      return;
    }

    // Start of, end of ancestry.
    if (!optionalArgs.preferEnd) {
      this.ancestryHelper_({
        node,
        prevNode,
        buff,
        ruleStr,
        type,
        ancestors: info.startAncestors,
        formatName: 'startOf',
        excludePreviousAncestors: true,
      });
    }

    if (!optionalArgs.preferStart) {
      this.ancestryHelper_({
        node,
        prevNode,
        buff,
        ruleStr,
        type,
        ancestors: info.endAncestors,
        formatName: 'endOf',
        exclude: [...info.startAncestors].concat(node),
      });
    }
  }

  /**
   * @param {{
   * node: !AutomationNode,
   * prevNode: !AutomationNode,
   * type: (EventType|outputTypes.OutputEventType),
   * buff: !Array<Spannable>,
   * ruleStr: !OutputRulesStr,
   * ancestors: !Array<!AutomationNode>,
   * formatName: string,
   * exclude: (!Array<!AutomationNode>|undefined),
   * excludePreviousAncestors: (boolean|undefined)
   * }} args
   * @private
   */
  ancestryHelper_(args) {
    let {node, prevNode, buff, ruleStr, type, ancestors, formatName} = args;

    /** Following types are contained: {event, role, navigation, output} */
    const rule = {};
    // First, look up the event type's format block.
    // Navigate is the default event.
    rule.event = Output.RULES[type] ? type : 'navigate';
    const eventBlock = Output.RULES[rule.event];

    const excludeRoles =
        args.exclude ? new Set(args.exclude.map(node => node.role)) : new Set();

    // Customize for braille node annotations.
    const originalBuff = buff;
    for (let j = ancestors.length - 1, formatNode; (formatNode = ancestors[j]);
         j--) {
      const roleInfo = OutputRoleInfo[formatNode.role] || {};
      if (!roleInfo.verboseAncestry &&
          (excludeRoles.has(formatNode.role) ||
           (args.excludePreviousAncestors &&
            this.formattedAncestors_.has(formatNode)))) {
        continue;
      }

      const parentRole = roleInfo.inherits;
      if (eventBlock[formatNode.role] &&
          eventBlock[formatNode.role][formatName]) {
        rule.role = formatNode.role;
      } else if (eventBlock[parentRole] && eventBlock[parentRole][formatName]) {
        rule.role = parentRole;
      } else {
        rule.role = 'default';
      }

      if (eventBlock[rule.role][formatName]) {
        rule.navigation = formatName;
        rule.output =
            eventBlock[rule.role][formatName].speak ? 'speak' : undefined;
        if (this.formatOptions_.braille) {
          buff = [];
          ruleStr.bufferClear();
          if (eventBlock[rule.role][formatName].braille) {
            rule.output = 'braille';
          }
        }

        excludeRoles.add(formatNode.role);
        ruleStr.writeRule /** @type {OutputRulesStr.Rule} */ ((rule));
        const enterFormat = rule.output ?
            eventBlock[rule.role][formatName][rule.output] :
            eventBlock[rule.role][formatName];
        this.formattedAncestors_.add(formatNode);
        this.format_({
          node: formatNode,
          outputFormat: enterFormat,
          outputBuffer: buff,
          outputRuleString: ruleStr,
          opt_prevNode: prevNode,
        });

        if (this.formatOptions_.braille && buff.length) {
          const nodeSpan = this.mergeBraille_(buff);
          nodeSpan.setSpan(
              new outputTypes.OutputNodeSpan(formatNode), 0, nodeSpan.length);
          originalBuff.push(nodeSpan);
        }
      }
    }
  }

  /**
   * @param {!AutomationNode} node
   * @param {!AutomationNode} prevNode
   * @param {EventType|outputTypes.OutputEventType} type
   * @param {!Array<Spannable>} buff
   * @param {!OutputRulesStr} ruleStr
   * @private
   */
  node_(node, prevNode, type, buff, ruleStr) {
    const originalBuff = buff;

    if (this.formatOptions_.braille) {
      buff = [];
      ruleStr.bufferClear();
    }

    const rule = {};

    // Navigate is the default event.
    rule.event = Output.RULES[type] ? type : 'navigate';
    const eventBlock = Output.RULES[rule.event];
    const parentRole = (OutputRoleInfo[node.role] || {}).inherits || '';
    /**
     * Use Output.RULES for node.role if exists.
     * If not, use Output.RULES for parentRole if exists.
     * If not, use Output.RULES for 'default'.
     */
    if (node.role && (eventBlock[node.role] || {}).speak !== undefined) {
      rule.role = node.role;
    } else if ((eventBlock[parentRole] || {}).speak !== undefined) {
      rule.role = parentRole;
    } else {
      rule.role = 'default';
    }
    rule.output = 'speak';
    if (this.formatOptions_.braille) {
      // Overwrite rule by braille rule if exists.
      if (node.role && (eventBlock[node.role] || {}).braille !== undefined) {
        rule.role = node.role;
        rule.output = 'braille';
      } else if ((eventBlock[parentRole] || {}).braille !== undefined) {
        rule.role = parentRole;
        rule.output = 'braille';
      }
    }
    ruleStr.writeRule(rule);
    this.format_({
      node,
      outputFormat: eventBlock[rule.role][rule.output],
      outputBuffer: buff,
      outputRuleString: ruleStr,
      opt_prevNode: prevNode,
    });

    // Restore braille and add an annotation for this node.
    if (this.formatOptions_.braille) {
      const nodeSpan = this.mergeBraille_(buff);
      nodeSpan.setSpan(
          new outputTypes.OutputNodeSpan(node), 0, nodeSpan.length);
      originalBuff.push(nodeSpan);
    }
  }

  /**
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   * @param {EventType|outputTypes.OutputEventType} type
   * @param {!Array<Spannable>} buff
   * @private
   */
  subNode_(range, prevRange, type, buff, ruleStr) {
    if (!prevRange) {
      prevRange = range;
    }
    const dir = CursorRange.getDirection(prevRange, range);
    const node = range.start.node;
    const prevNode = prevRange.getBound(dir).node;
    if (!node || !prevNode) {
      return;
    }

    const options = {annotation: ['name'], isUnique: true};
    const rangeStart = range.start.index;
    const rangeEnd = range.end.index;
    if (this.formatOptions_.braille) {
      options.annotation.push(new outputTypes.OutputNodeSpan(node));
      const selStart = node.textSelStart;
      const selEnd = node.textSelEnd;

      if (selStart !== undefined && selEnd >= rangeStart &&
          selStart <= rangeEnd) {
        // Editable text selection.

        // |rangeStart| and |rangeEnd| are indices set by the caller and are
        // assumed to be inside of the range. In braille, we only ever expect
        // to get ranges surrounding a line as anything smaller doesn't make
        // sense.

        // |selStart| and |selEnd| reflect the editable selection. The
        // relative selStart and relative selEnd for the current line are then
        // just the difference between |selStart|, |selEnd| with |rangeStart|.
        // See editing_test.js for examples.
        options.annotation.push(new outputTypes.OutputSelectionSpan(
            selStart - rangeStart, selEnd - rangeStart));
      } else if (
          rangeStart !== 0 || rangeEnd !== range.start.getText().length) {
        // Non-editable text selection over less than the full contents
        // covered by the range. We exclude full content underlines because it
        // is distracting to read braille with all cells underlined with a
        // cursor.
        options.annotation.push(
            new outputTypes.OutputSelectionSpan(rangeStart, rangeEnd));
      }
    }

    // Intentionally skip subnode output for
    // outputTypes.OutputContextOrder.DIRECTED.
    if (this.contextOrder_ === outputTypes.OutputContextOrder.FIRST ||
        (this.contextOrder_ === outputTypes.OutputContextOrder.FIRST_AND_LAST &&
         range.start.index === 0)) {
      this.ancestry_(node, prevNode, type, buff, ruleStr, {preferStart: true});
    }
    const earcon = this.findEarcon_(node, prevNode);
    if (earcon) {
      options.annotation.push(earcon);
    }
    let text = '';

    if (this.formatOptions_.braille && !node.state[StateType.EDITABLE]) {
      // In braille, we almost always want to show the entire contents and
      // simply place the cursor under the SelectionSpan we set above.
      text = range.start.getText();
    } else {
      // This is output for speech or editable braille.
      text = range.start.getText().substring(rangeStart, rangeEnd);
    }

    if (localStorage['languageSwitching'] === 'true') {
      this.assignLocaleAndAppend_(text, node, buff, options);
    } else {
      this.append_(buff, text, options);
    }
    ruleStr.write('subNode_: ' + text + '\n');

    if (this.contextOrder_ === outputTypes.OutputContextOrder.LAST ||
        (this.contextOrder_ === outputTypes.OutputContextOrder.FIRST_AND_LAST &&
         range.end.index === range.end.getText().length)) {
      this.ancestry_(node, prevNode, type, buff, ruleStr, {preferEnd: true});
    }

    range.start.node.boundsForRange(rangeStart, rangeEnd, loc => {
      if (loc) {
        this.locations_.push(loc);
      }
    });
  }

  /**
   * Renders all hints for the given |range|.
   *
   * Add new hints to either method computeHints_ or computeDelayedHints_. Hints
   * are outputted in order, so consider the relative priority of any new
   * additions. Rendering processes these two methods in order. The only
   * distinction is a small delay gets introduced before the first hint in
   * |computeDelayedHints_|.
   * @param {!CursorRange} range
   * @param {!Array<AutomationNode>} uniqueAncestors
   * @param {EventType|outputTypes.OutputEventType} type
   * @param {!Array<Spannable>} buff Buffer to receive rendered output.
   * @param {!OutputRulesStr} ruleStr
   * @private
   */
  hint_(range, uniqueAncestors, type, buff, ruleStr) {
    if (!this.enableHints_ || localStorage['useVerboseMode'] !== 'true') {
      return;
    }

    // No hints for alerts, which can be targeted at controls.
    if (type === EventType.ALERT) {
      return;
    }

    // Hints are not yet specialized for braille.
    if (this.formatOptions_.braille) {
      return;
    }

    const node = range.start.node;
    if (node.restriction === chrome.automation.Restriction.DISABLED) {
      return;
    }

    const msgs = Output.computeHints_(node, uniqueAncestors);
    const delayedMsgs =
        Output.computeDelayedHints_(node, uniqueAncestors, type);
    if (delayedMsgs.length > 0) {
      delayedMsgs[0].props = new outputTypes.OutputSpeechProperties();
      delayedMsgs[0].props.properties['delay'] = true;
    }

    const allMsgs = msgs.concat(delayedMsgs);
    for (const msg of allMsgs) {
      if (msg.msgId) {
        const text = Msgs.getMsg(msg.msgId, msg.subs);
        this.append_(buff, text, {annotation: [msg.props]});
        ruleStr.write('hint_: ' + text + '\n');
      } else if (msg.text) {
        this.append_(buff, msg.text, {annotation: [msg.props]});
        ruleStr.write('hint_: ' + msg.text + '\n');
      } else if (msg.outputFormat) {
        ruleStr.write('hint_: ...');
        this.format_({
          node,
          outputFormat: msg.outputFormat,
          outputBuffer: buff,
          outputRuleString: ruleStr,
          opt_speechProps: msg.props,
        });
      } else {
        throw new Error('Unexpected hint: ' + msg);
      }
    }
  }

  /**
   * Internal helper to |hint_|. Returns a list of message hints.
   * @param {!AutomationNode} node
   * @param {!Array<AutomationNode>} uniqueAncestors
   * @return {!Array<{text: (string|undefined),
   *           msgId: (string|undefined),
   *           outputFormat: (string|undefined)}>} Note that the above caller
   * expects one and only one key be set.
   * @private
   */
  static computeHints_(node, uniqueAncestors) {
    const ret = [];
    if (node.errorMessage) {
      ret.push({outputFormat: '$node(errorMessage)'});
    }

    // Provide a hint for sort direction.
    let sortDirectionNode = node;
    while (sortDirectionNode && sortDirectionNode !== sortDirectionNode.root) {
      if (!sortDirectionNode.sortDirection) {
        sortDirectionNode = sortDirectionNode.parent;
        continue;
      }
      if (sortDirectionNode.sortDirection ===
          chrome.automation.SortDirectionType.ASCENDING) {
        ret.push({msgId: 'sort_ascending'});
      } else if (
          sortDirectionNode.sortDirection ===
          chrome.automation.SortDirectionType.DESCENDING) {
        ret.push({msgId: 'sort_descending'});
      }
      break;
    }

    let currentNode = node;
    let ancestorIndex = 0;
    do {
      if (currentNode.ariaCurrentState &&
          outputTypes.OutputPropertyMap.STATE[currentNode.ariaCurrentState]) {
        ret.push({
          msgId:
              outputTypes.OutputPropertyMap.STATE[currentNode.ariaCurrentState],
        });
        break;
      }
      currentNode = uniqueAncestors[ancestorIndex++];
    } while (currentNode);

    return ret;
  }

  /**
   * Internal helper to |hint_|. Returns a list of message hints.
   * @param {!AutomationNode} node
   * @param {!Array<AutomationNode>} uniqueAncestors
   * @param {EventType|outputTypes.OutputEventType} type
   * @return {!Array<{text: (string|undefined),
   *           msgId: (string|undefined),
   *           subs: (Array<string>|undefined),
   *           outputFormat: (string|undefined)}>} Note that the above caller
   * expects one and only one key be set.
   * @private
   */
  static computeDelayedHints_(node, uniqueAncestors, type) {
    const ret = [];
    if (EventSourceState.get() === EventSourceType.TOUCH_GESTURE) {
      if (node.state[StateType.EDITABLE]) {
        ret.push({
          msgId: node.state[StateType.FOCUSED] ? 'hint_is_editing' :
                                                 'hint_double_tap_to_edit',
        });
        return ret;
      }

      const isWithinVirtualKeyboard = AutomationUtil.getAncestors(node).find(
          n => n.role === RoleType.KEYBOARD);
      if (AutomationPredicate.clickable(node) && !isWithinVirtualKeyboard) {
        ret.push({
          msgId: 'hint_actionable',
          subs: [
            Msgs.getMsg('action_double_tap', []),
            node.doDefaultLabel ? node.doDefaultLabel :
                                  Msgs.getMsg('label_activate', []),
          ],
        });
      }

      const enteredVirtualKeyboard =
          uniqueAncestors.find(n => n.role === RoleType.KEYBOARD);
      if (enteredVirtualKeyboard) {
        ret.push({msgId: 'hint_touch_type'});
      }
      return ret;
    }

    if (node.state[StateType.EDITABLE] && node.state[StateType.FOCUSED] &&
        (node.state[StateType.MULTILINE] ||
         node.state[StateType.RICHLY_EDITABLE])) {
      ret.push({msgId: 'hint_search_within_text_field'});
    }

    if (node.placeholder) {
      ret.push({text: node.placeholder});
    }

    // Invalid Grammar text.
    if (uniqueAncestors.find(
            /** @type {function(?) : boolean} */
            (AutomationPredicate.hasInvalidGrammarMarker))) {
      ret.push({msgId: 'hint_invalid_grammar'});
    }

    // Invalid Spelling text.
    if (uniqueAncestors.find(
            /** @type {function(?) : boolean} */
            (AutomationPredicate.hasInvalidSpellingMarker))) {
      ret.push({msgId: 'hint_invalid_spelling'});
    }

    // Only include tooltip as a hint as a last alternative. It may have been
    // included as the name or description previously. As a rule of thumb,
    // only include it if there's no name and no description.
    if (node.tooltip && !node.name && !node.description) {
      ret.push({text: node.tooltip});
    }

    if (AutomationPredicate.checkable(node)) {
      ret.push({msgId: 'hint_checkable'});
    } else if (AutomationPredicate.clickable(node)) {
      ret.push({
        msgId: 'hint_actionable',
        subs: [
          Msgs.getMsg('action_search_plus_space', []),
          node.doDefaultLabel ? node.doDefaultLabel :
                                Msgs.getMsg('label_activate', []),
        ],
      });
    }

    if (AutomationPredicate.longClickable(node)) {
      ret.push({
        msgId: 'hint_actionable',
        subs: [
          Msgs.getMsg('action_search_plus_shift_plus_space', []),
          node.longClickLabel ? node.longClickLabel :
                                Msgs.getMsg('label_long_click', []),
        ],
      });
    }

    if (node.autoComplete === 'list' || node.autoComplete === 'both' ||
        node.state[StateType.AUTOFILL_AVAILABLE]) {
      ret.push({msgId: 'hint_autocomplete_list'});
    }
    if (node.autoComplete === 'inline' || node.autoComplete === 'both') {
      ret.push({msgId: 'hint_autocomplete_inline'});
    }
    if (node.customActions && node.customActions.length > 0) {
      ret.push({msgId: 'hint_action'});
    }
    if (node.accessKey) {
      ret.push({text: Msgs.getMsg('access_key', [node.accessKey])});
    }

    // Ancestry based hints.
    /** @type {AutomationNode|undefined} */
    let foundAncestor;
    if (uniqueAncestors.find(
            /** @type {function(?) : boolean} */ (AutomationPredicate.table))) {
      ret.push({msgId: 'hint_table'});
    }

    // This hint is not based on the role (it uses state), so we need to care
    // about ordering; prefer deepest ancestor first.
    if ((foundAncestor = uniqueAncestors.reverse().find(
             /** @type {function(?) : boolean} */ (AutomationPredicate.roles(
                 [RoleType.MENU, RoleType.MENU_BAR]))))) {
      ret.push({
        msgId: foundAncestor.state.horizontal ? 'hint_menu_horizontal' :
                                                'hint_menu',
      });
    }
    if (uniqueAncestors.find(
            /** @type {function(?) : boolean} */ (function(n) {
              return Boolean(n.details);
            }))) {
      ret.push({msgId: 'hint_details'});
    }

    return ret;
  }

  /**
   * Appends output to the |buff|.
   * @param {!Array<Spannable>} buff
   * @param {string|!Spannable} value
   * @param {{annotation: Array<*>, isUnique: (boolean|undefined)}=} opt_options
   */
  append_(buff, value, opt_options) {
    opt_options = opt_options || {isUnique: false, annotation: []};

    // Reject empty values without meaningful annotations.
    if ((!value || value.length === 0) &&
        opt_options.annotation.every(
            annotation => !(annotation instanceof outputTypes.OutputAction) &&
                !(annotation instanceof outputTypes.OutputSelectionSpan))) {
      return;
    }

    const spannableToAdd = new Spannable(value);
    opt_options.annotation.forEach(
        annotation =>
            spannableToAdd.setSpan(annotation, 0, spannableToAdd.length));

    // |isUnique| specifies an annotation that cannot be duplicated.
    if (opt_options.isUnique) {
      const annotationSansNodes = opt_options.annotation.filter(
          annotation => !(annotation instanceof outputTypes.OutputNodeSpan));

      const alreadyAnnotated = buff.some(spannable => {
        annotationSansNodes.some(annotation => {
          if (!spannable.hasSpan(annotation)) {
            return false;
          }
          const start = spannable.getSpanStart(annotation);
          const end = spannable.getSpanEnd(annotation);
          const substr = spannable.substring(start, end);
          if (substr && value) {
            return substr.toString() === value.toString();
          } else {
            return false;
          }
        });
      });
      if (alreadyAnnotated) {
        return;
      }
    }

    buff.push(spannableToAdd);
  }

  /**
   * Converts the braille |spans| buffer to a single spannable.
   * @param {!Array<Spannable>} spans
   * @return {!Spannable}
   * @private
   */
  mergeBraille_(spans) {
    let separator = '';  // Changes to space as appropriate.
    let prevHasInlineNode = false;
    let prevIsName = false;
    return spans.reduce((result, cur) => {
      // Ignore empty spans except when they contain a selection.
      const hasSelection =
          cur.getSpanInstanceOf(outputTypes.OutputSelectionSpan);
      if (cur.length === 0 && !hasSelection) {
        return result;
      }

      // For empty selections, we just add the space separator to account for
      // showing the braille cursor.
      if (cur.length === 0 && hasSelection) {
        result.append(cur);
        result.append(Output.SPACE);
        separator = '';
        return result;
      }

      // Keep track of if there's an inline node associated with
      // |cur|.
      const hasInlineNode =
          cur.getSpansInstanceOf(outputTypes.OutputNodeSpan).some(spannable => {
            if (!spannable.node) {
              return false;
            }
            return spannable.node.display === 'inline' ||
                spannable.node.role === RoleType.INLINE_TEXT_BOX;
          });

      const isName = cur.hasSpan('name');

      // Now, decide whether we should include separators between the previous
      // span and |cur|.
      // Never separate chunks without something already there at this point.

      // The only case where we know for certain that a separator is not
      // needed is when the previous and current values are in-lined and part
      // of the node's name. In all other cases, use the surrounding
      // whitespace to ensure we only have one separator between the node
      // text.
      if (result.length === 0 ||
          (hasInlineNode && prevHasInlineNode && isName && prevIsName)) {
        separator = '';
      } else if (
          result.toString()[result.length - 1] === Output.SPACE ||
          cur.toString()[0] === Output.SPACE) {
        separator = '';
      } else {
        separator = Output.SPACE;
      }

      prevHasInlineNode = hasInlineNode;
      prevIsName = isName;
      result.append(separator);
      result.append(cur);
      return result;
    }, new Spannable());
  }

  /**
   * Find the earcon for a given node (including ancestry).
   * @param {!AutomationNode} node
   * @param {!AutomationNode=} opt_prevNode
   * @return {outputTypes.OutputAction}
   */
  findEarcon_(node, opt_prevNode) {
    if (node === opt_prevNode) {
      return null;
    }

    if (this.formatOptions_.speech) {
      let earconFinder = node;
      let ancestors;
      if (opt_prevNode) {
        ancestors = AutomationUtil.getUniqueAncestors(opt_prevNode, node);
      } else {
        ancestors = AutomationUtil.getAncestors(node);
      }

      while (earconFinder = ancestors.pop()) {
        const info = OutputRoleInfo[earconFinder.role];
        if (info && info.earconId) {
          return new outputTypes.OutputEarconAction(
              info.earconId, node.location || undefined);
          break;
        }
        earconFinder = earconFinder.parent;
      }
    }
    return null;
  }

  /**
   * Gets a human friendly string with the contents of output.
   * @return {string}
   */
  toString() {
    return this.speechBuffer_.reduce((prev, cur) => {
      if (prev === null || prev === '') {
        return cur.toString();
      }
      prev += ' ' + cur.toString();
      return prev;
    }, null);
  }

  /**
   * Gets the spoken output with separator '|'.
   * @return {!Spannable}
   */
  get speechOutputForTest() {
    return this.speechBuffer_.reduce((prev, cur) => {
      if (prev === null) {
        return cur;
      }
      prev.append('|');
      prev.append(cur);
      return prev;
    }, null);
  }

  /**
   * Gets the output buffer for braille.
   * @return {!Spannable}
   */
  get brailleOutputForTest() {
    return this.mergeBraille_(this.brailleBuffer_);
  }

  /**
   * @param {string} text
   * @param {!AutomationNode} contextNode
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @private
   */
  assignLocaleAndAppend_(text, contextNode, buff, options) {
    const data =
        LocaleOutputHelper.instance.computeTextAndLocale(text, contextNode);
    const speechProps = new outputTypes.OutputSpeechProperties();
    speechProps.properties['lang'] = data.locale;
    this.append_(buff, data.text, options);
    // Attach associated SpeechProperties if the buffer is
    // non-empty.
    if (buff.length > 0) {
      buff[buff.length - 1].setSpan(speechProps, 0, 0);
    }
  }
}

/**
 * Delimiter to use between output values.
 * @type {string}
 */
Output.SPACE = ' ';

/**
 * Rules specifying format of AutomationNodes for output.
 * @type {!Object<Object<Object<string>>>}
 * Please see below for more information on properties.
 * speak: The speech rule for when ChromeVox range lands exactly on the node.
 * braille: The braille rule for when ChromeVox range lands exactly on the node.
 * enter: The rule for when ChromeVox range enters the node's subtree.
 *    Can contain speak and braille properties.
 * leave: The rule for when ChromeVox range exits the node's subtree.
 * startOf: The rule applied for each ancestor diff of a range and its previous
 * leaf range. endOf: The rule applied for each ancestor diff of a range and its
 * next leaf range.
 */
Output.RULES = {
  navigate: {
    'default': {
      speak: `$name $node(activeDescendant) $value $state $restriction $role
          $description`,
      braille: ``,
    },
    abstractContainer: {
      startOf: `$nameFromNode $role $state $description`,
      endOf: `@end_of_container($role)`,
    },
    abstractFormFieldContainer: {
      enter: `$nameFromNode $role $state $description`,
      leave: `@exited_container($role)`,
    },
    abstractItem: {
      // Note that ChromeVox generally does not output position/count. Only for
      // some roles (see sub-output rules) or when explicitly provided by an
      // author (via posInSet), do we include them in the output.
      enter: `$nameFromNode $role $state $restriction $description
          $if($posInSet, @describe_index($posInSet, $setSize))`,
      speak: `$state $nameOrTextContent= $role
          $if($posInSet, @describe_index($posInSet, $setSize))
          $description $restriction`,
    },
    abstractList: {
      startOf: `$nameFromNode $role @@list_with_items($setSize)
          $restriction $description`,
      endOf: `@end_of_container($role) @@list_nested_level($listNestedLevel)`,
    },
    abstractNameFromContents: {
      speak: `$nameOrDescendants $node(activeDescendant) $value $state
          $restriction $role $description`,
    },
    abstractRange: {
      speak: `$name $node(activeDescendant) $description $role
          $if($value, $value, $if($valueForRange, $valueForRange))
          $state $restriction
          $if($minValueForRange, @aria_value_min($minValueForRange))
          $if($maxValueForRange, @aria_value_max($maxValueForRange))`,
    },
    abstractSpan: {
      startOf: `$nameFromNode $role $state $description`,
      endOf: `@end_of_container($role)`,
    },
    alert: {
      enter: `$name $role $state`,
      speak: `$earcon(ALERT_NONMODAL) $role $nameOrTextContent $description
          $state`,
    },
    alertDialog: {
      enter: `$earcon(ALERT_MODAL) $name $state $description $roleDescription
          $textContent`,
      speak: `$earcon(ALERT_MODAL) $name $nameOrTextContent $description $state
          $role`,
    },
    button: {
      speak: `$name $node(activeDescendant) $state $restriction $role
          $description`,
    },
    cell: {
      enter: {
        speak: `$cellIndexText $node(tableCellColumnHeaders) $nameFromNode
            $roleDescription $state`,
        braille: `$state $cellIndexText $node(tableCellColumnHeaders)
            $nameFromNode $roleDescription`,
      },
      speak: `$nameFromNode $descendants $cellIndexText
          $node(tableCellColumnHeaders) $roleDescription $state $description`,
      braille: `$state
          $name $cellIndexText $node(tableCellColumnHeaders) $roleDescription
          $description
          $if($selected, @aria_selected_true)`,
    },
    checkBox: {
      speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $name $role $if($checkedStateDescription, $checkedStateDescription, $checked)
          $description $state $restriction`,
    },
    client: {speak: `$name`},
    comboBoxMenuButton: {
      speak: `$name $value $role @aria_has_popup
          $if($setSize, @@list_with_items($setSize))
          $state $restriction $description`,
    },
    date: {enter: `$nameFromNode $role $state $restriction $description`},
    dialog: {enter: `$nameFromNode $role $description`},
    genericContainer: {
      enter: `$nameFromNode $description $state`,
      speak: `$nameOrTextContent $description $state`,
    },
    embeddedObject: {speak: `$name`},
    grid: {
      speak: `$name $node(activeDescendant) $role $state $restriction
          $description`,
    },
    group: {
      enter: `$nameFromNode $roleDescription $state $restriction $description`,
      speak: `$nameOrDescendants $value $state $restriction $roleDescription
          $description`,
      leave: ``,
    },
    heading: {
      enter: `!relativePitch(hierarchicalLevel)
          $nameFromNode=
          $if($hierarchicalLevel, @tag_h+$hierarchicalLevel, $role) $state
          $description`,
      speak: `!relativePitch(hierarchicalLevel)
          $nameOrDescendants=
          $if($hierarchicalLevel, @tag_h+$hierarchicalLevel, $role) $state
          $restriction $description`,
    },
    image: {
      speak: `$if($name, $name,
          $if($imageAnnotation, $imageAnnotation, $urlFilename))
          $value $state $role $description`,
    },
    imeCandidate:
        {speak: '$name $phoneticReading @describe_index($posInSet, $setSize)'},
    inlineTextBox: {speak: `$precedingBullet $name=`},
    inputTime: {enter: `$nameFromNode $role $state $restriction $description`},
    labelText: {
      speak: `$name $value $state $restriction $roleDescription $description`,
    },
    lineBreak: {speak: `$name=`},
    link: {
      enter: `$nameFromNode= $role $state $restriction`,
      speak: `$name $value $state $restriction
          $if($inPageLinkTarget, @internal_link, $role) $description`,
    },
    list: {
      speak: `$nameFromNode $descendants $role
          @@list_with_items($setSize) $description $state`,
    },
    listBox: {
      enter: `$nameFromNode $role @@list_with_items($setSize)
          $restriction $description`,
    },
    listBoxOption: {
      speak: `$state $name $role @describe_index($posInSet, $setSize)
          $description $restriction
          $nif($selected, @aria_selected_false)`,
      braille: `$state $name $role @describe_index($posInSet, $setSize)
          $description $restriction
          $if($selected, @aria_selected_true, @aria_selected_false)`,
    },
    listMarker: {speak: `$name`},
    menu: {
      enter: `$name $role `,
      speak: `$name $node(activeDescendant)
          $role @@list_with_items($setSize) $description $state $restriction`,
    },
    menuItem: {
      speak: `$name $role $if($hasPopup, @has_submenu)
          @describe_index($posInSet, $setSize) $description $state $restriction`,
    },
    menuItemCheckBox: {
      speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $name $role $checked $state $restriction $description
          @describe_index($posInSet, $setSize)`,
    },
    menuItemRadio: {
      speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $if($checked, @describe_menu_item_radio_selected($name),
          @describe_menu_item_radio_unselected($name)) $state $roleDescription
          $restriction $description
          @describe_index($posInSet, $setSize)`,
    },
    menuListOption: {
      speak: `$name $role @describe_index($posInSet, $setSize) $state
          $nif($selected, @aria_selected_false)
          $restriction $description`,
      braille: `$name $role @describe_index($posInSet, $setSize) $state
          $if($selected, @aria_selected_true, @aria_selected_false)
          $restriction $description`,
    },
    paragraph: {speak: `$nameOrDescendants $roleDescription`},
    radioButton: {
      speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $if($checked, @describe_radio_selected($name),
          @describe_radio_unselected($name))
          @describe_index($posInSet, $setSize)
          $roleDescription $description $state $restriction`,
    },
    rootWebArea: {enter: `$name`, speak: `$if($name, $name, @web_content)`},
    region: {speak: `$state $nameOrTextContent $description $roleDescription`},
    row: {
      startOf: `$node(tableRowHeader) $roleDescription
          $if($hierarchicalLevel, @describe_depth($hierarchicalLevel))`,
      speak: ` $if($hierarchicalLevel, @describe_depth($hierarchicalLevel))
          $name $node(activeDescendant) $value $state $restriction $role
          $if($selected, @aria_selected_true) $description`,
    },
    staticText: {speak: `$precedingBullet $name= $description`},
    switch: {
      speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $if($checked, @describe_switch_on($name),
          @describe_switch_off($name)) $roleDescription
          $description $state $restriction`,
    },
    tab: {
      speak: `@describe_tab($name) $roleDescription $description
          @describe_index($posInSet, $setSize) $state $restriction
          $if($selected, @aria_selected_true)`,
    },
    table: {
      enter: `$roleDescription @table_summary($name,
          $if($ariaRowCount, $ariaRowCount, $tableRowCount),
          $if($ariaColumnCount, $ariaColumnCount, $tableColumnCount))
          $node(tableHeader)`,
    },
    tabList: {
      speak: `$name $node(activeDescendant) $state $restriction $role
          $description`,
    },
    textField: {
      speak: `$name $value
          $if($roleDescription, $roleDescription,
              $if($multiline, @tag_textarea,
                  $if($inputType, $inputType, $role)))
          $description $state $restriction`,
    },
    timer: {
      speak: `$nameFromNode $descendants $value $state $role
        $description`,
    },
    toggleButton: {
      speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $name $role $pressed $description $state $restriction`,
    },
    toolbar: {enter: `$name $role $description $restriction`},
    tree: {enter: `$name $role @@list_with_items($setSize) $restriction`},
    treeItem: {
      enter: `$role $expanded $collapsed $restriction
          @describe_index($posInSet, $setSize)
          @describe_depth($hierarchicalLevel)`,
      speak: `$name
          $role $description $state $restriction
          $nif($selected, @aria_selected_false)
          @describe_index($posInSet, $setSize)
          @describe_depth($hierarchicalLevel)`,
    },
    unknown: {speak: ``},
    window: {
      enter: `@describe_window($name) $description`,
      speak: `@describe_window($name) $description $earcon(OBJECT_OPEN)`,
    },
  },
  menuStart:
      {'default': {speak: `@chrome_menu_opened($name)  $earcon(OBJECT_OPEN)`}},
  menuEnd: {'default': {speak: `@chrome_menu_closed $earcon(OBJECT_CLOSE)`}},
  menuListValueChanged: {
    'default': {
      speak: `$value $name
          $find({"state": {"selected": true, "invisible": false}},
          @describe_index($posInSet, $setSize)) `,
    },
  },
  alert: {
    default: {speak: `$earcon(ALERT_NONMODAL) $nameOrTextContent $description`},
  },
};

/**
 * If set, the next speech utterance will use this value instead of the normal
 * queueing mode.
 * @type {QueueMode|undefined}
 * @private
 */
Output.forceModeForNextSpeechUtterance_;
