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
import {LocalStorage} from '../../../common/local_storage.js';
import {AutomationTreeWalker} from '../../../common/tree_walker.js';
import {Earcon} from '../../common/abstract_earcons.js';
import {NavBraille} from '../../common/braille/nav_braille.js';
import {EventSourceType} from '../../common/event_source_type.js';
import {LocaleOutputHelper} from '../../common/locale_output_helper.js';
import {LogType} from '../../common/log_types.js';
import {Msgs} from '../../common/msgs.js';
import {CustomRole} from '../../common/role_type.js';
import {Spannable} from '../../common/spannable.js';
import {QueueMode, TtsCategory, TtsSpeechProperties} from '../../common/tts_types.js';
import {ValueSelectionSpan, ValueSpan} from '../braille/spans.js';
import {ChromeVox} from '../chromevox.js';
import {EventSourceState} from '../event_source.js';
import {FocusBounds} from '../focus_bounds.js';
import {PhoneticData} from '../phonetic_data.js';

import {OutputAncestryInfo} from './output_ancestry_info.js';
import {OutputFormatParser, OutputFormatParserObserver} from './output_format_parser.js';
import {OutputFormatTree} from './output_format_tree.js';
import {OutputFormatter} from './output_formatter.js';
import {OutputInterface} from './output_interface.js';
import {OutputFormatLogger} from './output_logger.js';
import {OutputRoleInfo} from './output_role_info.js';
import {OutputRule, OutputRuleSpecifier} from './output_rules.js';
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
 * @implements {OutputInterface}
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
    /** @private {!OutputFormatLogger} */
    this.speechFormatLog_ =
        new OutputFormatLogger('enableSpeechLogging', LogType.SPEECH_RULE);
    /** @private {!OutputFormatLogger} */
    this.brailleFormatLog_ =
        new OutputFormatLogger('enableBrailleLogging', LogType.BRAILLE_RULE);

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
   * @param {!outputTypes.OutputEventType} type
   * @return {!Output}
   */
  withSpeech(range, prevRange, type) {
    this.formatOptions_ = {speech: true, braille: false, auralStyle: false};
    this.formattedAncestors_ = new WeakSet();
    this.render_(
        range, prevRange, type, this.speechBuffer_, this.speechFormatLog_);
    return this;
  }

  /**
   * Specify ranges for aurally styled speech.
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   * @param {!outputTypes.OutputEventType} type
   * @return {!Output}
   */
  withRichSpeech(range, prevRange, type) {
    this.formatOptions_ = {speech: true, braille: false, auralStyle: true};
    this.formattedAncestors_ = new WeakSet();
    this.render_(
        range, prevRange, type, this.speechBuffer_, this.speechFormatLog_);
    return this;
  }

  /**
   * Specify ranges for braille.
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   * @param {!outputTypes.OutputEventType} type
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
        range, prevRange, type, this.brailleBuffer_, this.brailleFormatLog_);
    return this;
  }

  /**
   * Specify ranges for location.
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   * @param {!outputTypes.OutputEventType} type
   * @return {!Output}
   */
  withLocation(range, prevRange, type) {
    this.formatOptions_ = {speech: false, braille: false, auralStyle: false};
    this.formattedAncestors_ = new WeakSet();
    this.render_(
        range, prevRange, type, [] /*unused output*/,
        new OutputFormatLogger('', LogType.SPEECH_RULE) /*unused log*/);
    return this;
  }

  /**
   * Specify the same ranges for speech and braille.
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   * @param {!outputTypes.OutputEventType} type
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
   * @param {!outputTypes.OutputEventType} type
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
    this.speechFormatLog_.write('withString: ' + value + '\n');
    this.brailleFormatLog_.write('withString: ' + value + '\n');
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
      outputFormatLogger: this.speechFormatLog_,
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
      outputFormatLogger: this.brailleFormatLog_,
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
    this.speechFormatLog_.commitLogs();

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
      this.brailleFormatLog_.commitLogs();
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

  /** @override */
  render_(range, prevRange, type, buff, formatLog, optionalArgs = {}) {
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
      this.subNode_(range, prevRange, type, buff, formatLog);
    } else {
      this.range_(range, prevRange, type, buff, formatLog, optionalArgs);
    }

    this.hint_(
        range, AutomationUtil.getUniqueAncestors(prevParent, range.start.node),
        type, buff, formatLog);
  }

  /** @override */
  format_(params) {
    const formatter = new OutputFormatter(this, params);
    new OutputFormatParser(formatter).parse(params.outputFormat);
  }

  /** @override */
  formatNode_(data, token, tree, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;
    let prevNode = data.opt_prevNode;

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
            formatLog.writeTokenWithValue(token, header);
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
            formatLog.writeTokenWithValue(token, header);
          }
        }
      }
    } else if (node[relationName]) {
      const related = node[relationName];
      this.node_(
          related, related, outputTypes.OutputCustomEvent.NAVIGATE, buff,
          formatLog);
    }
  }

  /** @override */
  formatTextContent_(data, token, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    if (node.name && token === 'nameOrTextContent') {
      formatLog.writeToken(token);
      this.format_({
        node,
        outputFormat: '$name',
        outputBuffer: buff,
        outputFormatLogger: formatLog,
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
    formatLog.writeTokenWithValue(token, finalOutput);
  }

  /** @override */
  formatAsFieldAccessor_(data, token, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    options.annotation.push(token);
    let value = node[token];
    if (typeof value === 'number') {
      value = String(value);
    }
    this.append_(buff, value, options);
    formatLog.writeTokenWithValue(token, value);
  }

  /** @override */
  formatAsStateValue_(data, token, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    options.annotation.push('state');
    const stateInfo = outputTypes.OUTPUT_STATE_INFO[token];
    let resolvedInfo = {};
    resolvedInfo = node.state[/** @type {StateType} */ (token)] ? stateInfo.on :
                                                                  stateInfo.off;
    if (!resolvedInfo) {
      return;
    }
    if (this.formatOptions_.speech && resolvedInfo.earcon) {
      options.annotation.push(
          new outputTypes.OutputEarconAction(resolvedInfo.earcon),
          node.location || undefined);
    }
    const msgId = this.formatOptions_.braille ? resolvedInfo.msgId + '_brl' :
                                                resolvedInfo.msgId;
    const msg = Msgs.getMsg(msgId);
    this.append_(buff, msg, options);
    formatLog.writeTokenWithValue(token, msg);
  }

  /** @override */
  formatPhoneticReading_(data) {
    const buff = data.outputBuffer;
    const node = data.node;

    const text =
        PhoneticData.forText(node.name || '', chrome.i18n.getUILanguage());
    this.append_(buff, text);
  }

  /** @override */
  formatListNestedLevel_(data) {
    const buff = data.outputBuffer;
    const node = data.node;

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

  /** @override */
  formatPrecedingBullet_(data) {
    const buff = data.outputBuffer;
    const node = data.node;

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

  /** @override */
  formatCustomFunction_(data, token, tree, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    // Custom functions.
    if (token === 'if') {
      formatLog.writeToken(token);
      const cond = tree.firstChild;
      const attrib = cond.value.slice(1);
      if (Output.isTruthy(node, attrib)) {
        formatLog.write(attrib + '==true => ');
        this.format_({
          node,
          outputFormat: cond.nextSibling || '',
          outputBuffer: buff,
          outputFormatLogger: formatLog,
        });
      } else if (Output.isFalsey(node, attrib)) {
        formatLog.write(attrib + '==false => ');
        this.format_({
          node,
          outputFormat: cond.nextSibling.nextSibling || '',
          outputBuffer: buff,
          outputFormatLogger: formatLog,
        });
      }
    } else if (token === 'nif') {
      formatLog.writeToken(token);
      const cond = tree.firstChild;
      const attrib = cond.value.slice(1);
      if (Output.isFalsey(node, attrib)) {
        formatLog.write(attrib + '==false => ');
        this.format_({
          node,
          outputFormat: cond.nextSibling || '',
          outputBuffer: buff,
          outputFormatLogger: formatLog,
        });
      } else if (Output.isTruthy(node, attrib)) {
        formatLog.write(attrib + '==true => ');
        this.format_({
          node,
          outputFormat: cond.nextSibling.nextSibling || '',
          outputBuffer: buff,
          outputFormatLogger: formatLog,
        });
      }
    } else if (token === 'earcon') {
      // Ignore unless we're generating speech output.
      if (!this.formatOptions_.speech) {
        return;
      }

      options.annotation.push(new outputTypes.OutputEarconAction(
          Earcon[tree.firstChild.value], node.location || undefined));
      this.append_(buff, '', options);
      formatLog.writeTokenWithValue(token, tree.firstChild.value);
    }
  }

  /** @override */
  formatMessage_(data, token, tree, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

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
    formatLog.write(token + '{');
    if (!isPluralized) {
      let curArg = tree.firstChild;
      while (curArg) {
        if (curArg.value[0] !== '$') {
          const errorMsg = 'Unexpected value: ' + curArg.value;
          formatLog.writeError(errorMsg);
          console.error(errorMsg);
          return;
        }
        let msgBuff = [];
        this.format_({
          node,
          outputFormat: curArg,
          outputBuffer: msgBuff,
          outputFormatLogger: formatLog,
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
      formatLog.writeError(errorMsg);
      console.error(errorMsg);
      return;
    }

    if (isPluralized) {
      const arg = tree.firstChild;
      if (!arg || arg.nextSibling) {
        const errorMsg = 'Pluralized messages take exactly one argument';
        formatLog.writeError(errorMsg);
        console.error(errorMsg);
        return;
      }
      if (arg.value[0] !== '$') {
        const errorMsg = 'Unexpected value: ' + arg.value;
        formatLog.writeError(errorMsg);
        console.error(errorMsg);
        return;
      }
      const argBuff = [];
      this.format_({
        node,
        outputFormat: arg,
        outputBuffer: argBuff,
        outputFormatLogger: formatLog,
      });
      const namedArgs = {COUNT: Number(argBuff[0])};
      msg = new goog.i18n.MessageFormat(msg).format(namedArgs);
    }
    formatLog.write('}');

    this.append_(buff, msg, options);
    formatLog.write(': ' + msg + '\n');
  }

  /**
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   * @param {!outputTypes.OutputEventType} type
   * @param {!Array<Spannable>} rangeBuff
   * @param {!OutputFormatLogger} formatLog
   * @param {{suppressStartEndAncestry: (boolean|undefined)}} optionalArgs
   * @private
   */
  range_(range, prevRange, type, rangeBuff, formatLog, optionalArgs = {}) {
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
            node, prevNode, type, buff, formatLog,
            {preferStart: preferStartOrEndAncestry});
      }
      this.node_(node, prevNode, type, buff, formatLog);
      if (addContextAfter) {
        this.ancestry_(
            node, prevNode, type, buff, formatLog,
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
          this.subNode_(partialRange, prevRange, type, rangeBuff, formatLog);
        }
      } else if (hasPartialNodeEnd && node === range.end.node) {
        if (range.end.index !== 0) {
          const partialRange = new CursorRange(
              new Cursor(node, 0), new Cursor(node, range.end.index));
          this.subNode_(partialRange, prevRange, type, rangeBuff, formatLog);
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
      this.ancestry_(target, prevRange.start.node, type, rangeBuff, formatLog);
    }
  }

  /**
   * @param {!AutomationNode} node
   * @param {!AutomationNode} prevNode
   * @param {!outputTypes.OutputEventType} type
   * @param {!Array<Spannable>} buff
   * @param {!OutputFormatLogger} formatLog
   * @param {{suppressStartEndAncestry: (boolean|undefined),
   *         preferStart: (boolean|undefined),
   *         preferEnd: (boolean|undefined)
   *        }} optionalArgs
   * @private
   */
  ancestry_(node, prevNode, type, buff, formatLog, optionalArgs = {}) {
    if (LocalStorage.get('useVerboseMode') === false) {
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
      formatLog,
      type,
      ancestors: info.leaveAncestors,
      formatName: 'leave',
      exclude: [...info.enterAncestors, node],
    });
    this.ancestryHelper_({
      node,
      prevNode,
      buff,
      formatLog,
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
        formatLog,
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
        formatLog,
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
   * type: !outputTypes.OutputEventType,
   * buff: !Array<Spannable>,
   * formatLog: !OutputFormatLogger,
   * ancestors: !Array<!AutomationNode>,
   * formatName: string,
   * exclude: (!Array<!AutomationNode>|undefined),
   * excludePreviousAncestors: (boolean|undefined)
   * }} args
   * @private
   */
  ancestryHelper_(args) {
    let {node, prevNode, buff, formatLog, type, ancestors, formatName} = args;

    const rule = new OutputRule(type);
    // First, look up the event type's format block.
    const eventBlock = OutputRule.RULES[rule.event];

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
      if (formatNode.role && eventBlock[formatNode.role] &&
          eventBlock[formatNode.role][formatName]) {
        rule.role = formatNode.role;
      } else if (
          parentRole && eventBlock[parentRole] &&
          eventBlock[parentRole][formatName]) {
        rule.role = parentRole;
      } else {
        rule.role = CustomRole.DEFAULT;
      }

      if (eventBlock[rule.role][formatName]) {
        rule.navigation = formatName;
        rule.output = eventBlock[rule.role][formatName].speak ?
            outputTypes.OutputFormatType.SPEAK :
            undefined;
        if (this.formatOptions_.braille) {
          buff = [];
          formatLog.bufferClear();
          if (eventBlock[rule.role][formatName].braille) {
            rule.output = outputTypes.OutputFormatType.BRAILLE;
          }
        }

        excludeRoles.add(formatNode.role);
        formatLog.writeRule(rule.specifier);
        const enterFormat = rule.output ?
            eventBlock[rule.role][formatName][rule.output] :
            eventBlock[rule.role][formatName];
        this.formattedAncestors_.add(formatNode);
        this.format_({
          node: formatNode,
          outputFormat: enterFormat,
          outputBuffer: buff,
          outputFormatLogger: formatLog,
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
   * @param {!outputTypes.OutputEventType} type
   * @param {!Array<Spannable>} buff
   * @param {!OutputFormatLogger} formatLog
   * @private
   */
  node_(node, prevNode, type, buff, formatLog) {
    const originalBuff = buff;

    if (this.formatOptions_.braille) {
      buff = [];
      formatLog.bufferClear();
    }

    const rule = new OutputRule(type);
    const eventBlock = OutputRule.RULES[rule.event];
    const parentRole =
        (OutputRoleInfo[node.role] || {}).inherits || CustomRole.NO_ROLE;
    /**
     * Use OutputRule.RULES for node.role if exists.
     * If not, use OutputRule.RULES for parentRole if exists.
     * If not, use OutputRule.RULES for CustomRole.DEFAULT.
     */
    if (node.role && (eventBlock[node.role] || {}).speak !== undefined) {
      rule.role = node.role;
    } else if ((eventBlock[parentRole] || {}).speak !== undefined) {
      rule.role = parentRole;
    } else {
      rule.role = CustomRole.DEFAULT;
    }
    rule.output = outputTypes.OutputFormatType.SPEAK;
    if (this.formatOptions_.braille) {
      // Overwrite rule by braille rule if exists.
      if (node.role && (eventBlock[node.role] || {}).braille !== undefined) {
        rule.role = node.role;
        rule.output = outputTypes.OutputFormatType.BRAILLE;
      } else if ((eventBlock[parentRole] || {}).braille !== undefined) {
        rule.role = parentRole;
        rule.output = outputTypes.OutputFormatType.BRAILLE;
      }
    }
    formatLog.writeRule(rule.specifier);
    this.format_({
      node,
      outputFormat: eventBlock[rule.role][rule.output],
      outputBuffer: buff,
      outputFormatLogger: formatLog,
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
   * @param {!outputTypes.OutputEventType} type
   * @param {!Array<Spannable>} buff
   * @private
   */
  subNode_(range, prevRange, type, buff, formatLog) {
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
      this.ancestry_(
          node, prevNode, type, buff, formatLog, {preferStart: true});
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

    if (LocalStorage.get('languageSwitching')) {
      this.assignLocaleAndAppend_(text, node, buff, options);
    } else {
      this.append_(buff, text, options);
    }
    formatLog.write('subNode_: ' + text + '\n');

    if (this.contextOrder_ === outputTypes.OutputContextOrder.LAST ||
        (this.contextOrder_ === outputTypes.OutputContextOrder.FIRST_AND_LAST &&
         range.end.index === range.end.getText().length)) {
      this.ancestry_(node, prevNode, type, buff, formatLog, {preferEnd: true});
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
   * @param {!outputTypes.OutputEventType} type
   * @param {!Array<Spannable>} buff Buffer to receive rendered output.
   * @param {!OutputFormatLogger} formatLog
   * @private
   */
  hint_(range, uniqueAncestors, type, buff, formatLog) {
    if (!this.enableHints_ || LocalStorage.get('useVerboseMode') !== true) {
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
        formatLog.write('hint_: ' + text + '\n');
      } else if (msg.text) {
        this.append_(buff, msg.text, {annotation: [msg.props]});
        formatLog.write('hint_: ' + msg.text + '\n');
      } else if (msg.outputFormat) {
        formatLog.write('hint_: ...');
        this.format_({
          node,
          outputFormat: msg.outputFormat,
          outputBuffer: buff,
          outputFormatLogger: formatLog,
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
   * @param {!outputTypes.OutputEventType} type
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

  /** @override */
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

  /** @override */
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
        if (info && info.earcon) {
          return new outputTypes.OutputEarconAction(
              info.earcon, node.location || undefined);
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

  /** @override */
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

  /** @override */
  shouldSuppress(token) {
    return this.suppressions_[token];
  }

  /** @override */
  get useAuralStyle() {
    return this.formatOptions_.auralStyle;
  }

  /** @override */
  get formatAsBraille() {
    return this.formatOptions_.braille;
  }
}

/**
 * Delimiter to use between output values.
 * @type {string}
 */
Output.SPACE = ' ';

/**
 * If set, the next speech utterance will use this value instead of the normal
 * queueing mode.
 * @type {QueueMode|undefined}
 * @private
 */
Output.forceModeForNextSpeechUtterance_;
