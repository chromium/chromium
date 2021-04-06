// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides output services for ChromeVox.
 */

goog.provide('Output');
goog.provide('Output.EventType');

goog.require('AbstractEarcons');
goog.require('AutomationTreeWalker');
goog.require('ChromeVox');
goog.require('EarconEngine');
goog.require('EventSourceState');
goog.require('LocaleOutputHelper');
goog.require('LogStore');
goog.require('NavBraille');
goog.require('OutputFormatTree');
goog.require('OutputRulesStr');
goog.require('PhoneticData');
goog.require('Spannable');
goog.require('TextLog');
goog.require('TtsCategory');
goog.require('ValueSelectionSpan');
goog.require('ValueSpan');
goog.require('constants');
goog.require('cursors.Cursor');
goog.require('cursors.Range');
goog.require('cursors.Unit');
goog.require('goog.i18n.MessageFormat');

goog.scope(function() {
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
 * An Output object formats a cursors.Range into speech, braille, or both
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
Output = class {
  constructor() {
    // TODO(dtseng): Include braille specific rules.
    /** @type {!Array<!Spannable>} @private */
    this.speechBuffer_ = [];
    /** @type {!Array<!Spannable>} @private */
    this.brailleBuffer_ = [];
    /** @type {!Array<!Object>} @private */
    this.locations_ = [];
    /** @type {function(?)} @private */
    this.speechEndCallback_;

    /** Store output rules */
    /** @type {!OutputRulesStr} @private */
    this.speechRulesStr_ = new OutputRulesStr('enableSpeechLogging');
    /** @type {!OutputRulesStr} @private */
    this.brailleRulesStr_ = new OutputRulesStr('enableBrailleLogging');

    /**
     * Current global options.
     * @type {{speech: boolean, braille: boolean, auralStyle: boolean}}
     * @private
     */
    this.formatOptions_ = {speech: true, braille: false, auralStyle: false};

    /**
     * The speech category for the generated speech utterance.
     * @type {TtsCategory}
     * @private
     */
    this.speechCategory_ = TtsCategory.NAV;

    /**
     * The speech queue mode for the generated speech utterance.
     * @type {QueueMode}
     * @private
     */
    this.queueMode_;

    /**
     * @type {boolean}
     * @private
     */
    this.outputContextFirst_ = false;

    /** @private {!Object<string, boolean>} */
    this.suppressions_ = {};

    /** @private {boolean} */
    this.enableHints_ = true;

    /** @private {!Object} */
    this.initialSpeechProps_ = {};

    /** @private {boolean} */
    this.drawFocusRing_ = true;
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
        return !!node.roleDescription;
      case 'value':
        return !!node.value;
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
    return this.speechBuffer_.every(function(buff) {
      return !/\S+/.test(buff.toString());
    });
  }

  /** @return {Spannable} */
  get braille() {
    return this.mergeBraille_(this.brailleBuffer_);
  }

  /**
   * Specify ranges for speech.
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {EventType|Output.EventType} type
   * @return {!Output}
   */
  withSpeech(range, prevRange, type) {
    this.formatOptions_ = {speech: true, braille: false, auralStyle: false};
    this.render_(
        range, prevRange, type, this.speechBuffer_, this.speechRulesStr_);
    return this;
  }

  /**
   * Specify ranges for aurally styled speech.
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {EventType|Output.EventType} type
   * @return {!Output}
   */
  withRichSpeech(range, prevRange, type) {
    this.formatOptions_ = {speech: true, braille: false, auralStyle: true};
    this.render_(
        range, prevRange, type, this.speechBuffer_, this.speechRulesStr_);
    return this;
  }

  /**
   * Specify ranges for braille.
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {EventType|Output.EventType} type
   * @return {!Output}
   */
  withBraille(range, prevRange, type) {
    this.formatOptions_ = {speech: false, braille: true, auralStyle: false};

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
      prevRange = cursors.Range.fromNode(range.start.node.parent);
      range = new cursors.Range(
          cursors.Cursor.fromNode(start), cursors.Cursor.fromNode(end));
    }
    this.render_(
        range, prevRange, type, this.brailleBuffer_, this.brailleRulesStr_);
    return this;
  }

  /**
   * Specify ranges for location.
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {EventType|Output.EventType} type
   * @return {!Output}
   */
  withLocation(range, prevRange, type) {
    this.formatOptions_ = {speech: false, braille: false, auralStyle: false};
    this.render_(
        range, prevRange, type, [] /*unused output*/,
        new OutputRulesStr('') /*unused log*/);
    return this;
  }

  /**
   * Specify the same ranges for speech and braille.
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {EventType|Output.EventType} type
   * @return {!Output}
   */
  withSpeechAndBraille(range, prevRange, type) {
    this.withSpeech(range, prevRange, type);
    this.withBraille(range, prevRange, type);
    return this;
  }

  /**
   * Specify the same ranges for aurally styled speech and braille.
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {EventType|Output.EventType} type
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
    this.outputContextFirst_ = true;
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
   * @param {!Object} speechProps
   * @return {!Output}
   */
  withInitialSpeechProperties(speechProps) {
    this.initialSpeechProps_ = speechProps;
    return this;
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
    this.format_({
      node,
      outputFormat: formatStr,
      outputBuffer: this.speechBuffer_,
      outputRuleString: this.speechRulesStr_
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
    this.format_({
      node,
      outputFormat: formatStr,
      outputBuffer: this.brailleBuffer_,
      outputRuleString: this.brailleRulesStr_
    });
    return this;
  }

  /**
   * Triggers callback for a speech event.
   * @param {function()} callback
   * @return {!Output}
   */
  onSpeechEnd(callback) {
    this.speechEndCallback_ = function(opt_cleanupOnly) {
      if (!opt_cleanupOnly) {
        callback();
      }
    }.bind(this);
    return this;
  }

  /**
   * Executes all specified output.
   */
  go() {
    // Speech.
    let queueMode = QueueMode.QUEUE;
    if (Output.forceModeForNextSpeechUtterance_ !== undefined) {
      queueMode =
          /** @type{QueueMode} */ (Output.forceModeForNextSpeechUtterance_);
    } else if (this.queueMode_ !== undefined) {
      queueMode = /** @type{QueueMode} */ (this.queueMode_);
    }

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

      let speechProps = {};
      const speechPropsInstance = /** @type {Output.SpeechProperties} */ (
          buff.getSpanInstanceOf(Output.SpeechProperties));

      if (!speechPropsInstance) {
        speechProps = this.initialSpeechProps_;
      } else {
        for (const [key, value] of Object.entries(this.initialSpeechProps_)) {
          if (speechPropsInstance.properties[key] === undefined) {
            speechPropsInstance.properties[key] = value;
          }
        }
        speechProps = speechPropsInstance.properties;
      }

      speechProps.category = this.speechCategory_;

      (function() {
        const scopedBuff = buff;
        speechProps['startCallback'] = function() {
          const actions = scopedBuff.getSpansInstanceOf(Output.Action);
          if (actions) {
            actions.forEach(function(a) {
              a.run();
            });
          }
        };
      }());

      if (i === this.speechBuffer_.length - 1) {
        speechProps['endCallback'] = this.speechEndCallback_;
      }

      ChromeVox.tts.speak(buff.toString(), queueMode, speechProps);

      // Skip resetting |queueMode| if the |text| is empty. If we don't do this,
      // and the tts engine doesn't generate a callback, we might not properly
      // flush.
      if (text !== '') {
        queueMode = QueueMode.QUEUE;
      }
    }
    if (this.speechRulesStr_.str) {
      LogStore.getInstance().writeTextLog(
          this.speechRulesStr_.str, LogStore.LogType.SPEECH_RULE);
    }

    // Braille.
    if (this.brailleBuffer_.length) {
      const buff = this.mergeBraille_(this.brailleBuffer_);
      const selSpan = buff.getSpanInstanceOf(Output.SelectionSpan);
      let startIndex = -1, endIndex = -1;
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
            this.brailleRulesStr_.str, LogStore.LogType.BRAILLE_RULE);
      }
    }

    // Display.
    if (this.speechCategory_ !== TtsCategory.LIVE && this.drawFocusRing_) {
      ChromeVoxState.instance.setFocusBounds(this.locations_);
    }
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
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {EventType|Output.EventType} type
   * @param {!Array<Spannable>} buff Buffer to receive rendered output.
   * @param {!OutputRulesStr} ruleStr
   * @private
   */
  render_(range, prevRange, type, buff, ruleStr) {
    if (prevRange && !prevRange.isValid()) {
      prevRange = null;
    }

    // Scan unique ancestors to get the value of |outputContextFirst|.
    let parent = range.start.node;
    const prevParent = prevRange ? prevRange.start.node : parent;
    if (!parent || !prevParent) {
      return;
    }
    const uniqueAncestors =
        AutomationUtil.getUniqueAncestors(prevParent, parent);
    for (let i = 0; parent = uniqueAncestors[i]; i++) {
      if (parent.role === RoleType.WINDOW) {
        break;
      }
      if (Output.ROLE_INFO_[parent.role] &&
          Output.ROLE_INFO_[parent.role].outputContextFirst) {
        this.outputContextFirst_ = true;
        break;
      }
    }

    if (range.isSubNode()) {
      this.subNode_(range, prevRange, type, buff, ruleStr);
    } else {
      this.range_(range, prevRange, type, buff, ruleStr);
    }

    this.hint_(range, uniqueAncestors, type, buff, ruleStr);
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
   *    opt_speechProps: (!Output.SpeechProperties|undefined)
   * }} params An object containing all required and optional parameters.
   * @private
   */
  format_(params) {
    const node = params['node'];
    let format = params['outputFormat'];
    const buff = params['outputBuffer'];
    const ruleStr = params['outputRuleString'];
    const prevNode = params['opt_prevNode'];
    let speechProps = params['opt_speechProps'];

    let formatTrees = [];
    const args = null;

    // Hacky way to support args.
    if (typeof (format) === 'string') {
      format = format.replace(/([,:])\s+/gm, '$1');
      const words = format.split(' ');
      // Ignore empty strings.
      words.filter(word => !!word);

      formatTrees = words.map(word => OutputFormatTree.buildFromString(word));
    } else {
      formatTrees = format ? [format] : [];
    }

    formatTrees.forEach(function(tree) {
      // Obtain the operator token.
      let token = tree.value;

      // Set suffix options.
      const options = {};
      options.annotation = [];
      options.isUnique = token[token.length - 1] === '=';
      if (options.isUnique) {
        token = token.substring(0, token.length - 1);
      }

      // Process token based on prefix.
      const prefix = token[0];
      token = token.slice(1);

      // All possible tokens based on prefix.
      if (prefix === '$') {
        if (this.suppressions_[token]) {
          return;
        }

        if (token === 'value') {
          this.formatValue_(node, token, buff, options, ruleStr);
        } else if (token === 'name') {
          this.formatName_(node, prevNode, token, buff, options, ruleStr);
        } else if (token === 'description') {
          this.formatDescription_(node, token, buff, options, ruleStr);
        } else if (token === 'urlFilename') {
          this.formatUrlFilename_(node, token, buff, options, ruleStr);
        } else if (token === 'nameFromNode') {
          this.formatNameFromNode_(node, token, buff, options, ruleStr);
        } else if (token === 'nameOrDescendants') {
          // This token is similar to nameOrTextContent except it gathers rich
          // output for descendants. It also lets name from contents override
          // the descendants text if |node| has only static text children.
          this.formatNameOrDescendants_(node, token, buff, options, ruleStr);
        } else if (token === 'indexInParent') {
          this.formatIndexInParent_(node, token, tree, buff, options, ruleStr);
        } else if (token === 'restriction') {
          this.formatRestriction_(node, token, buff, ruleStr);
        } else if (token === 'checked') {
          this.formatChecked_(node, token, buff, ruleStr);
        } else if (token === 'pressed') {
          this.formatPressed_(node, token, buff, ruleStr);
        } else if (token === 'state') {
          this.formatState_(node, token, buff, ruleStr);
        } else if (token === 'find') {
          this.formatFind_(node, token, tree, buff, ruleStr);
        } else if (token === 'descendants') {
          this.formatDescendants_(node, token, buff, ruleStr);
        } else if (token === 'joinedDescendants') {
          this.formatJoinedDescendants_(node, token, buff, options, ruleStr);
        } else if (token === 'role') {
          if (localStorage['useVerboseMode'] === 'false') {
            return;
          }
          if (this.formatOptions_.auralStyle) {
            speechProps = new Output.SpeechProperties();
            speechProps.properties['relativePitch'] = -0.3;
          }

          this.formatRole_(node, token, buff, options, ruleStr);
        } else if (token === 'inputType') {
          this.formatInputType_(node, token, buff, options, ruleStr);
        } else if (
            token === 'tableCellRowIndex' || token === 'tableCellColumnIndex') {
          this.formatTableCellIndex_(node, token, buff, options, ruleStr);
        } else if (token === 'cellIndexText') {
          this.formatCellIndexText_(node, token, buff, options, ruleStr);
        } else if (token === 'node') {
          this.formatNode_(node, prevNode, token, tree, buff, options, ruleStr);
        } else if (token === 'nameOrTextContent' || token === 'textContent') {
          this.formatTextContent_(node, token, buff, options, ruleStr);
        } else if (node[token] !== undefined) {
          this.formatAsFieldAccessor_(node, token, buff, options, ruleStr);
        } else if (Output.STATE_INFO_[token]) {
          this.formatAsStateValue_(node, token, buff, options, ruleStr);
        } else if (token === 'posInSet') {
          this.formatPosInSetFallback_(node, token, buff, ruleStr);
        } else if (token === 'setSize') {
          this.formatSetSizeFallback_(node, token, buff, ruleStr);
        } else if (token === 'phoneticReading') {
          this.formatPhoneticReading_(node, buff);
        } else if (tree.firstChild) {
          this.formatCustomFunction_(node, token, tree, buff, options, ruleStr);
        }
      } else if (prefix === '@') {
        ruleStr.write(' @');
        if (this.formatOptions_.auralStyle) {
          if (!speechProps) {
            speechProps = new Output.SpeechProperties();
          }
          speechProps.properties['relativePitch'] = -0.2;
        }
        this.formatMessage_(node, token, tree, buff, options, ruleStr);
      } else if (prefix === '!') {
        ruleStr.write(' ! ' + token + '\n');
        speechProps = new Output.SpeechProperties();
        speechProps.properties[token] = true;
        if (tree.firstChild) {
          if (!this.formatOptions_.auralStyle) {
            speechProps = undefined;
            return;
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
          return;
        }
      }

      // Post processing.
      if (speechProps) {
        if (buff.length > 0) {
          buff[buff.length - 1].setSpan(speechProps, 0, 0);
          speechProps = null;
        }
      }
    }.bind(this));
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
      options.annotation.push(new Output.SelectionSpan(
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
      options.annotation.push(new Output.SelectionSpan(0, 0));
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
         node.children.every(function(child) {
           return child.role === RoleType.STATIC_TEXT;
         }))) {
      this.append_(buff, node.name || '', options);
      ruleStr.writeTokenWithValue(token, node.name);
    } else {
      ruleStr.writeToken(token);
      this.format_({
        node,
        outputFormat: '$descendants',
        outputBuffer: buff,
        outputRuleString: ruleStr
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
    const msg = Output.RESTRICTION_STATE_MAP[node.restriction];
    if (msg) {
      ruleStr.writeToken(token);
      this.format_({
        node,
        outputFormat: '@' + msg,
        outputBuffer: buff,
        outputRuleString: ruleStr
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
    const msg = Output.CHECKED_STATE_MAP[node.checked];
    if (msg) {
      ruleStr.writeToken(token);
      this.format_({
        node,
        outputFormat: '@' + msg,
        outputBuffer: buff,
        outputRuleString: ruleStr
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
    const msg = Output.PRESSED_STATE_MAP[node.checked];
    if (msg) {
      ruleStr.writeToken(token);
      this.format_({
        node,
        outputFormat: '@' + msg,
        outputBuffer: buff,
        outputRuleString: ruleStr
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
      Object.getOwnPropertyNames(node.state).forEach(function(s) {
        const stateInfo = Output.STATE_INFO_[s];
        if (stateInfo && !stateInfo.isRoleSpecific && stateInfo.on) {
          ruleStr.writeToken(token);
          this.format_({
            node,
            outputFormat: '$' + s,
            outputBuffer: buff,
            outputRuleString: ruleStr
          });
        }
      }.bind(this));
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
          outputRuleString: ruleStr
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

    const subrange = new cursors.Range(
        new cursors.Cursor(leftmost, cursors.NODE_INDEX),
        new cursors.Cursor(rightmost, cursors.NODE_INDEX));
    let prev = null;
    if (node) {
      prev = cursors.Range.fromNode(node);
    }
    ruleStr.writeToken(token);
    this.render_(subrange, prev, Output.EventType.NAVIGATE, buff, ruleStr);
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
      outputRuleString: ruleStr
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
    const info = Output.ROLE_INFO_[node.role];
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
        Output.INPUT_TYPE_MESSAGE_IDS_[node.inputType] || 'input_type_text';
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
        outputRuleString: ruleStr
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
      this.node_(related, related, Output.EventType.NAVIGATE, buff, ruleStr);
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
        outputRuleString: ruleStr
      });
      return;
    }

    if (!node.firstChild) {
      return;
    }

    const root = node;
    const walker = new AutomationTreeWalker(node, Dir.FORWARD, {
      visit: AutomationPredicate.leafOrStaticText,
      leaf: (n) => {
        // The root might be a leaf itself, but we still want to descend
        // into it.
        return n !== root && AutomationPredicate.leafOrStaticText(n);
      },
      root: (r) => r === root
    });
    const outputStrings = [];
    while (walker.next().node) {
      if (walker.node.name) {
        outputStrings.push(walker.node.name);
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
    const stateInfo = Output.STATE_INFO_[token];
    let resolvedInfo = {};
    resolvedInfo = node.state[/** @type {StateType} */ (token)] ? stateInfo.on :
                                                                  stateInfo.off;
    if (!resolvedInfo) {
      return;
    }
    if (this.formatOptions_.speech && resolvedInfo.earconId) {
      options.annotation.push(
          new Output.EarconAction(resolvedInfo.earconId),
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
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!OutputRulesStr} ruleStr
   */
  formatPosInSetFallback_(node, token, buff, ruleStr) {
    if (node.posInSet !== undefined) {
      // Unexpected case.
      this.append_(buff, String(node.posInSet));
      ruleStr.writeTokenWithValue(token, String(node.posInSet));
    } else {
      ruleStr.writeToken(token);
      this.format_({
        node,
        outputFormat: '$indexInParent',
        outputBuffer: buff,
        outputRuleString: ruleStr
      });
    }
  }

  /**
   * @param {AutomationNode} node
   * @param {string} token
   * @param {!Array<Spannable>} buff
   * @param {!OutputRulesStr} ruleStr
   */
  formatSetSizeFallback_(node, token, buff, ruleStr) {
    // Size is always expected to be 0.
    const size = node.setSize ? node.setSize : 0;
    this.append_(buff, String(size));
    ruleStr.writeTokenWithValue(token, String(node.setSize));
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
          outputRuleString: ruleStr
        });
      } else if (Output.isFalsey(node, attrib)) {
        ruleStr.write(attrib + '==false => ');
        this.format_({
          node,
          outputFormat: cond.nextSibling.nextSibling || '',
          outputBuffer: buff,
          outputRuleString: ruleStr
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
          outputRuleString: ruleStr
        });
      } else if (Output.isTruthy(node, attrib)) {
        ruleStr.write(attrib + '==true => ');
        this.format_({
          node,
          outputFormat: cond.nextSibling.nextSibling || '',
          outputBuffer: buff,
          outputRuleString: ruleStr
        });
      }
    } else if (token === 'earcon') {
      // Ignore unless we're generating speech output.
      if (!this.formatOptions_.speech) {
        return;
      }

      options.annotation.push(new Output.EarconAction(
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
    token = pieces.reduce(function(prev, cur) {
      let lookup = cur;
      if (cur[0] === '$') {
        lookup = node[cur.slice(1)];
      }
      return prev + lookup;
    }.bind(this), '');
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
          outputRuleString: ruleStr
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
        outputRuleString: ruleStr
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
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {EventType|Output.EventType} type
   * @param {!Array<Spannable>} rangeBuff
   * @param {!OutputRulesStr} ruleStr
   * @private
   */
  range_(range, prevRange, type, rangeBuff, ruleStr) {
    if (!range.start.node || !range.end.node) {
      return;
    }

    if (!prevRange && range.start.node.root) {
      prevRange = cursors.Range.fromNode(range.start.node.root);
    } else if (!prevRange) {
      return;
    }

    let cursor = cursors.Cursor.fromNode(range.start.node);
    let prevNode = prevRange.start.node;

    const formatNodeAndAncestors = function(node, prevNode) {
      const buff = [];

      if (this.outputContextFirst_) {
        this.ancestry_(node, prevNode, type, buff, ruleStr);
      }
      this.node_(node, prevNode, type, buff, ruleStr);
      if (!this.outputContextFirst_) {
        this.ancestry_(node, prevNode, type, buff, ruleStr);
      }
      if (node.location) {
        this.locations_.push(node.location);
      }
      return buff;
    }.bind(this);

    let lca = null;
    if (!this.outputContextFirst_) {
      if (range.start.node !== range.end.node) {
        lca = AutomationUtil.getLeastCommonAncestor(
            range.end.node, range.start.node);
      }

      prevNode = lca || prevNode;
    }

    const unit = range.isInlineText() ? cursors.Unit.TEXT : cursors.Unit.NODE;
    while (cursor.node && range.end.node &&
           AutomationUtil.getDirection(cursor.node, range.end.node) ===
               Dir.FORWARD) {
      const node = cursor.node;
      rangeBuff.push.apply(rangeBuff, formatNodeAndAncestors(node, prevNode));
      prevNode = node;
      cursor = cursor.move(unit, cursors.Movement.DIRECTIONAL, Dir.FORWARD);

      // Reached a boundary.
      if (cursor.node === prevNode) {
        break;
      }
    }

    // Finally, add on ancestry announcements, if needed.
    if (!this.outputContextFirst_) {
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
   * @param {EventType|Output.EventType} type
   * @param {!Array<Spannable>} buff
   * @param {!OutputRulesStr} ruleStr
   * @private
   */
  ancestry_(node, prevNode, type, buff, ruleStr) {
    if (Output.ROLE_INFO_[node.role] &&
        Output.ROLE_INFO_[node.role].ignoreAncestry) {
      return;
    }

    // Expects |ancestors| to be ordered from root down to leaf. Outputs in
    // reverse; place context first nodes at the end.
    function byContextFirst(ancestors) {
      let contextFirst = [];
      let rest = [];
      for (let i = 0; i < ancestors.length - 1; i++) {
        const node = ancestors[i];
        // Discard ancestors of deepest window.
        if (node.role === RoleType.WINDOW) {
          contextFirst = [];
          rest = [];
        }
        if ((Output.ROLE_INFO_[node.role] || {}).outputContextFirst) {
          contextFirst.push(node);
        } else {
          rest.push(node);
        }
      }
      return rest.concat(contextFirst.reverse());
    }
    const prevUniqueAncestors =
        byContextFirst(AutomationUtil.getUniqueAncestors(node, prevNode));
    const uniqueAncestors =
        byContextFirst(AutomationUtil.getUniqueAncestors(prevNode, node));

    /** Following types are contained: {event, role, navigation, output} */
    const rule = {};
    // First, look up the event type's format block.
    // Navigate is the default event.
    rule.event = Output.RULES[type] ? type : 'navigate';
    const eventBlock = Output.RULES[rule.event];

    // Hash the roles we've entered.
    const enteredRoleSet = {};
    for (let j = uniqueAncestors.length - 1, hashNode;
         (hashNode = uniqueAncestors[j]); j--) {
      enteredRoleSet[hashNode.role] = true;
    }

    for (let i = 0, formatPrevNode; (formatPrevNode = prevUniqueAncestors[i]);
         i++) {
      // This prevents very repetitive announcements.
      if (enteredRoleSet[formatPrevNode.role] ||
          node.role === formatPrevNode.role ||
          localStorage['useVerboseMode'] === 'false') {
        continue;
      }

      const parentRole =
          (Output.ROLE_INFO_[formatPrevNode.role] || {}).inherits;
      rule.role = (eventBlock[formatPrevNode.role] || {}).leave !== undefined ?
          formatPrevNode.role :
          (eventBlock[parentRole] || {}).leave !== undefined ? parentRole :
                                                               'default';
      if (eventBlock[rule.role].leave &&
          localStorage['useVerboseMode'] === 'true') {
        rule.navigation = 'leave';
        ruleStr.writeRule(rule);
        this.format_({
          node: formatPrevNode,
          outputFormat: eventBlock[rule.role].leave,
          outputBuffer: buff,
          outputRuleString: ruleStr,
          opt_prevNode: prevNode
        });
      }
    }

    // Customize for braille node annotations.
    const originalBuff = buff;
    const enterRole = {};
    for (let j = uniqueAncestors.length - 1, formatNode;
         (formatNode = uniqueAncestors[j]); j--) {
      const parentRole = (Output.ROLE_INFO_[formatNode.role] || {}).inherits;
      rule.role = (eventBlock[formatNode.role] || {}).enter !== undefined ?
          formatNode.role :
          (eventBlock[parentRole] || {}).enter !== undefined ? parentRole :
                                                               'default';
      if (eventBlock[rule.role].enter) {
        rule.navigation = 'enter';
        if (enterRole[formatNode.role]) {
          continue;
        }

        rule.output = eventBlock[rule.role].enter.speak ? 'speak' : undefined;
        if (this.formatOptions_.braille) {
          buff = [];
          ruleStr.bufferClear();
          if (eventBlock[rule.role].enter.braille) {
            rule.output = 'braille';
          }
        }

        enterRole[formatNode.role] = true;
        ruleStr.writeRule(rule);
        const enterFormat = rule.output ?
            eventBlock[rule.role]['enter'][rule.output] :
            eventBlock[rule.role]['enter'];
        this.format_({
          node: formatNode,
          outputFormat: enterFormat,
          outputBuffer: buff,
          outputRuleString: ruleStr,
          opt_prevNode: prevNode
        });

        if (this.formatOptions_.braille && buff.length) {
          const nodeSpan = this.mergeBraille_(buff);
          nodeSpan.setSpan(new Output.NodeSpan(formatNode), 0, nodeSpan.length);
          originalBuff.push(nodeSpan);
        }
      }
    }
  }

  /**
   * @param {!AutomationNode} node
   * @param {!AutomationNode} prevNode
   * @param {EventType|Output.EventType} type
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
    const parentRole = (Output.ROLE_INFO_[node.role] || {}).inherits || '';
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
      opt_prevNode: prevNode
    });

    // Restore braille and add an annotation for this node.
    if (this.formatOptions_.braille) {
      const nodeSpan = this.mergeBraille_(buff);
      nodeSpan.setSpan(new Output.NodeSpan(node), 0, nodeSpan.length);
      originalBuff.push(nodeSpan);
    }
  }

  /**
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {EventType|Output.EventType} type
   * @param {!Array<Spannable>} buff
   * @private
   */
  subNode_(range, prevRange, type, buff, ruleStr) {
    if (!prevRange) {
      prevRange = range;
    }
    const dir = cursors.Range.getDirection(prevRange, range);
    const node = range.start.node;
    const prevNode = prevRange.getBound(dir).node;
    if (!node || !prevNode) {
      return;
    }

    const options = {annotation: ['name'], isUnique: true};
    const rangeStart = range.start.index;
    const rangeEnd = range.end.index;
    if (this.formatOptions_.braille) {
      options.annotation.push(new Output.NodeSpan(node));
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
        options.annotation.push(new Output.SelectionSpan(
            selStart - rangeStart, selEnd - rangeStart));
      } else if (
          rangeStart !== 0 || rangeEnd !== range.start.getText().length) {
        // Non-editable text selection over less than the full contents
        // covered by the range. We exclude full content underlines because it
        // is distracting to read braille with all cells underlined with a
        // cursor.
        options.annotation.push(new Output.SelectionSpan(rangeStart, rangeEnd));
      }
    }

    if (this.outputContextFirst_) {
      this.ancestry_(node, prevNode, type, buff, ruleStr);
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

    if (!this.outputContextFirst_) {
      this.ancestry_(node, prevNode, type, buff, ruleStr);
    }

    range.start.node.boundsForRange(rangeStart, rangeEnd, (loc) => {
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
   * @param {!cursors.Range} range
   * @param {!Array<AutomationNode>} uniqueAncestors
   * @param {EventType|Output.EventType} type
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
      delayedMsgs[0].props = new Output.SpeechProperties();
      delayedMsgs[0].props.properties['delay'] = true;
    }

    const allMsgs = msgs.concat(delayedMsgs);
    for (const msg of allMsgs) {
      if (msg.msgId) {
        const text = Msgs.getMsg(msg.msgId);
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
          opt_speechProps: msg.props
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
          Output.ARIA_CURRENT_STATE_INFO_[currentNode.ariaCurrentState]) {
        ret.push({
          msgId: Output.ARIA_CURRENT_STATE_INFO_[currentNode.ariaCurrentState]
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
   * @param {EventType|Output.EventType} type
   * @return {!Array<{text: (string|undefined),
   *           msgId: (string|undefined),
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
                                                 'hint_double_tap_to_edit'
        });
        return ret;
      }

      const isWithinVirtualKeyboard = AutomationUtil.getAncestors(node).find(
          (n) => n.role === RoleType.KEYBOARD);
      if (AutomationPredicate.clickable(node) && !isWithinVirtualKeyboard) {
        ret.push({msgId: 'hint_double_tap'});
      }

      const enteredVirtualKeyboard =
          uniqueAncestors.find((n) => n.role === RoleType.KEYBOARD);
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

    // Only include tooltip as a hint as a last alternative. It may have been
    // included as the name or description previously. As a rule of thumb,
    // only include it if there's no name and no description.
    if (node.tooltip && !node.name && !node.description) {
      ret.push({text: node.tooltip});
    }

    if (AutomationPredicate.checkable(node)) {
      ret.push({msgId: 'hint_checkable'});
    } else if (AutomationPredicate.clickable(node)) {
      ret.push({msgId: 'hint_clickable'});
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
    if (uniqueAncestors.find(
            /** @type {function(?) : boolean} */ (AutomationPredicate.table))) {
      ret.push({msgId: 'hint_table'});
    }
    if (uniqueAncestors.find(/** @type {function(?) : boolean} */ (
            AutomationPredicate.roles([RoleType.MENU, RoleType.MENU_BAR])))) {
      ret.push({msgId: 'hint_menu'});
    }
    if (uniqueAncestors.find(
            /** @type {function(?) : boolean} */ (function(n) {
              return !!n.details;
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
        opt_options.annotation.every(function(a) {
          return !(a instanceof Output.Action) &&
              !(a instanceof Output.SelectionSpan);
        })) {
      return;
    }

    const spannableToAdd = new Spannable(value);
    opt_options.annotation.forEach(function(a) {
      spannableToAdd.setSpan(a, 0, spannableToAdd.length);
    });

    // |isUnique| specifies an annotation that cannot be duplicated.
    if (opt_options.isUnique) {
      const annotationSansNodes =
          opt_options.annotation.filter(function(annotation) {
            return !(annotation instanceof Output.NodeSpan);
          });

      const alreadyAnnotated = buff.some(function(s) {
        return annotationSansNodes.some(function(annotation) {
          if (!s.hasSpan(annotation)) {
            return false;
          }
          const start = s.getSpanStart(annotation);
          const end = s.getSpanEnd(annotation);
          const substr = s.substring(start, end);
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
    return spans.reduce(function(result, cur) {
      // Ignore empty spans except when they contain a selection.
      const hasSelection = cur.getSpanInstanceOf(Output.SelectionSpan);
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
          cur.getSpansInstanceOf(Output.NodeSpan).some(function(s) {
            if (!s.node) {
              return false;
            }
            return s.node.display === 'inline' ||
                s.node.role === RoleType.INLINE_TEXT_BOX;
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
   * @return {Output.Action}
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
        const info = Output.ROLE_INFO_[earconFinder.role];
        if (info && info.earconId) {
          return new Output.EarconAction(
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
    return this.speechBuffer_.reduce(function(prev, cur) {
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
    return this.speechBuffer_.reduce(function(prev, cur) {
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
    const speechProps = new Output.SpeechProperties();
    speechProps.properties['lang'] = data.locale;
    this.append_(buff, data.text, options);
    // Attach associated SpeechProperties if the buffer is
    // non-empty.
    if (buff.length > 0) {
      buff[buff.length - 1].setSpan(speechProps, 0, 0);
    }
  }
};

/**
 * Delimiter to use between output values.
 * @type {string}
 */
Output.SPACE = ' ';

/**
 * Metadata about supported automation roles.
 * @const {Object<{msgId: string,
 *                 earconId: (string|undefined),
 *                 inherits: (string|undefined),
 *                 outputContextFirst: (boolean|undefined),
 *                 ignoreAncestry: (boolean|undefined)}>}
 * msgId: the message id of the role. Each role used requires a speech entry in
 *        chromevox_strings.grd + an optional Braille entry (with _BRL suffix).
 * earconId: an optional earcon to play when encountering the role.
 * inherits: inherits rules from this role.
 * outputContextFirst: where to place the context output.
 * ignoreAncestry: ignores ancestry (context) output for this role.
 * @private
 */
Output.ROLE_INFO_ = {
  alert: {msgId: 'role_alert'},
  alertDialog: {msgId: 'role_alertdialog', outputContextFirst: true},
  article: {msgId: 'role_article', inherits: 'abstractItem'},
  application: {msgId: 'role_application', inherits: 'abstractContainer'},
  audio: {msgId: 'tag_audio', inherits: 'abstractContainer'},
  banner: {msgId: 'role_banner', inherits: 'abstractContainer'},
  button: {msgId: 'role_button', earconId: 'BUTTON'},
  buttonDropDown: {msgId: 'role_button', earconId: 'BUTTON'},
  checkBox: {msgId: 'role_checkbox'},
  columnHeader: {msgId: 'role_columnheader', inherits: 'cell'},
  comboBoxMenuButton: {msgId: 'role_combobox', earconId: 'LISTBOX'},
  complementary: {msgId: 'role_complementary', inherits: 'abstractContainer'},
  comment: {msgId: 'role_comment', inherits: 'abstractContainer'},
  contentDeletion: {
    msgId: 'role_content_deletion',
    inherits: 'abstractContainer',
    outputContextFirst: true
  },
  contentInsertion: {
    msgId: 'role_content_insertion',
    inherits: 'abstractContainer',
    outputContextFirst: true
  },
  contentInfo: {msgId: 'role_contentinfo', inherits: 'abstractContainer'},
  date: {msgId: 'input_type_date', inherits: 'abstractContainer'},
  definition: {msgId: 'role_definition', inherits: 'abstractContainer'},
  descriptionList: {msgId: 'role_description_list', inherits: 'abstractList'},
  descriptionListDetail:
      {msgId: 'role_description_list_detail', inherits: 'abstractItem'},
  dialog:
      {msgId: 'role_dialog', outputContextFirst: true, ignoreAncestry: true},
  directory: {msgId: 'role_directory', inherits: 'abstractContainer'},
  docAbstract: {msgId: 'role_doc_abstract', inherits: 'abstractContainer'},
  docAcknowledgments:
      {msgId: 'role_doc_acknowledgments', inherits: 'abstractContainer'},
  docAfterword: {msgId: 'role_doc_afterword', inherits: 'abstractContainer'},
  docAppendix: {msgId: 'role_doc_appendix', inherits: 'abstractContainer'},
  docBackLink:
      {msgId: 'role_doc_back_link', earconId: 'LINK', inherits: 'link'},
  docBiblioEntry: {
    msgId: 'role_doc_biblio_entry',
    earconId: 'LIST_ITEM',
    inherits: 'abstractItem'
  },
  docBibliography:
      {msgId: 'role_doc_bibliography', inherits: 'abstractContainer'},
  docBiblioRef:
      {msgId: 'role_doc_biblio_ref', earconId: 'LINK', inherits: 'link'},
  docChapter: {msgId: 'role_doc_chapter', inherits: 'abstractContainer'},
  docColophon: {msgId: 'role_doc_colophon', inherits: 'abstractContainer'},
  docConclusion: {msgId: 'role_doc_conclusion', inherits: 'abstractContainer'},
  docCover: {msgId: 'role_doc_cover', inherits: 'image'},
  docCredit: {msgId: 'role_doc_credit', inherits: 'abstractContainer'},
  docCredits: {msgId: 'role_doc_credits', inherits: 'abstractContainer'},
  docDedication: {msgId: 'role_doc_dedication', inherits: 'abstractContainer'},
  docEndnote: {
    msgId: 'role_doc_endnote',
    earconId: 'LIST_ITEM',
    inherits: 'abstractItem'
  },
  docEndnotes:
      {msgId: 'role_doc_endnotes', earconId: 'LISTBOX', inherits: 'list'},
  docEpigraph: {msgId: 'role_doc_epigraph', inherits: 'abstractContainer'},
  docEpilogue: {msgId: 'role_doc_epilogue', inherits: 'abstractContainer'},
  docErrata: {msgId: 'role_doc_errata', inherits: 'abstractContainer'},
  docExample: {msgId: 'role_doc_example', inherits: 'abstractContainer'},
  docFootnote: {
    msgId: 'role_doc_footnote',
    earconId: 'LIST_ITEM',
    inherits: 'abstractItem'
  },
  docForeword: {msgId: 'role_doc_foreword', inherits: 'abstractContainer'},
  docGlossary: {msgId: 'role_doc_glossary', inherits: 'abstractContainer'},
  docGlossRef:
      {msgId: 'role_doc_gloss_ref', earconId: 'LINK', inherits: 'link'},
  docIndex: {msgId: 'role_doc_index', inherits: 'abstractContainer'},
  docIntroduction:
      {msgId: 'role_doc_introduction', inherits: 'abstractContainer'},
  docNoteRef: {msgId: 'role_doc_note_ref', earconId: 'LINK', inherits: 'link'},
  docNotice: {msgId: 'role_doc_notice', inherits: 'abstractContainer'},
  docPageBreak: {msgId: 'role_doc_page_break', inherits: 'abstractContainer'},
  docPageFooter: {msgId: 'role_doc_page_footer', inherits: 'abstractContainer'},
  docPageHeader: {msgId: 'role_doc_page_header', inherits: 'abstractContainer'},
  docPageList: {msgId: 'role_doc_page_list', inherits: 'abstractContainer'},
  docPart: {msgId: 'role_doc_part', inherits: 'abstractContainer'},
  docPreface: {msgId: 'role_doc_preface', inherits: 'abstractContainer'},
  docPrologue: {msgId: 'role_doc_prologue', inherits: 'abstractContainer'},
  docPullquote: {msgId: 'role_doc_pullquote', inherits: 'abstractContainer'},
  docQna: {msgId: 'role_doc_qna', inherits: 'abstractContainer'},
  docSubtitle: {msgId: 'role_doc_subtitle', inherits: 'heading'},
  docTip: {msgId: 'role_doc_tip', inherits: 'abstractContainer'},
  docToc: {msgId: 'role_doc_toc', inherits: 'abstractContainer'},
  document: {msgId: 'role_document', inherits: 'abstractContainer'},
  form: {msgId: 'role_form', inherits: 'abstractContainer'},
  graphicsDocument:
      {msgId: 'role_graphics_document', inherits: 'abstractContainer'},
  graphicsObject:
      {msgId: 'role_graphics_object', inherits: 'abstractContainer'},
  graphicsSymbol: {msgId: 'role_graphics_symbol', inherits: 'image'},
  grid: {msgId: 'role_grid', inherits: 'table'},
  group: {msgId: 'role_group', inherits: 'abstractContainer'},
  heading: {
    msgId: 'role_heading',
  },
  image: {
    msgId: 'role_img',
  },
  inputTime: {msgId: 'input_type_time', inherits: 'abstractContainer'},
  link: {msgId: 'role_link', earconId: 'LINK'},
  list: {msgId: 'role_list', inherits: 'abstractList'},
  listBox:
      {msgId: 'role_listbox', earconId: 'LISTBOX', inherits: 'abstractList'},
  listBoxOption: {msgId: 'role_listitem', earconId: 'LIST_ITEM'},
  listGrid: {msgId: 'role_list_grid', inherits: 'table'},
  listItem:
      {msgId: 'role_listitem', earconId: 'LIST_ITEM', inherits: 'abstractItem'},
  log: {msgId: 'role_log', inherits: 'abstractNameFromContents'},
  main: {msgId: 'role_main', inherits: 'abstractContainer'},
  mark: {msgId: 'role_mark', inherits: 'abstractContainer'},
  marquee: {msgId: 'role_marquee', inherits: 'abstractNameFromContents'},
  math: {msgId: 'role_math', inherits: 'abstractContainer'},
  menu: {msgId: 'role_menu', outputContextFirst: true, ignoreAncestry: true},
  menuBar: {
    msgId: 'role_menubar',
  },
  menuItem: {msgId: 'role_menuitem'},
  menuItemCheckBox: {msgId: 'role_menuitemcheckbox'},
  menuItemRadio: {msgId: 'role_menuitemradio'},
  menuListOption: {msgId: 'role_menuitem'},
  menuListPopup: {msgId: 'role_menu'},
  meter: {msgId: 'role_meter', inherits: 'abstractRange'},
  navigation: {msgId: 'role_navigation', inherits: 'abstractContainer'},
  note: {msgId: 'role_note', inherits: 'abstractContainer'},
  progressIndicator:
      {msgId: 'role_progress_indicator', inherits: 'abstractRange'},
  popUpButton: {
    msgId: 'role_button',
    earconId: 'POP_UP_BUTTON',
    inherits: 'comboBoxMenuButton'
  },
  radioButton: {msgId: 'role_radio'},
  radioGroup: {msgId: 'role_radiogroup', inherits: 'abstractContainer'},
  region: {msgId: 'role_region', inherits: 'abstractContainer'},
  rootWebArea: {outputContextFirst: true},
  row: {msgId: 'role_row', inherits: 'abstractContainer'},
  rowHeader: {msgId: 'role_rowheader', inherits: 'cell'},
  scrollBar: {msgId: 'role_scrollbar', inherits: 'abstractRange'},
  section: {msgId: 'role_region', inherits: 'abstractContainer'},
  search: {msgId: 'role_search', inherits: 'abstractContainer'},
  separator: {msgId: 'role_separator', inherits: 'abstractContainer'},
  slider: {msgId: 'role_slider', inherits: 'abstractRange', earconId: 'SLIDER'},
  spinButton: {
    msgId: 'role_spinbutton',
    inherits: 'abstractRange',
    earconId: 'LISTBOX'
  },
  splitter: {msgId: 'role_separator'},
  status: {msgId: 'role_status', inherits: 'abstractNameFromContents'},
  suggestion: {
    msgId: 'role_suggestion',
    inherits: 'abstractContainer',
    outputContextFirst: true
  },
  tab: {msgId: 'role_tab'},
  tabList: {msgId: 'role_tablist', inherits: 'abstractContainer'},
  tabPanel: {msgId: 'role_tabpanel'},
  searchBox: {msgId: 'role_search', earconId: 'EDITABLE_TEXT'},
  textField: {msgId: 'input_type_text', earconId: 'EDITABLE_TEXT'},
  textFieldWithComboBox: {msgId: 'role_combobox', earconId: 'EDITABLE_TEXT'},
  time: {msgId: 'tag_time', inherits: 'abstractContainer'},
  timer: {msgId: 'role_timer', inherits: 'abstractNameFromContents'},
  toolbar: {msgId: 'role_toolbar', ignoreAncestry: true},
  toggleButton: {msgId: 'role_toggle_button', inherits: 'checkBox'},
  tree: {msgId: 'role_tree'},
  treeItem: {msgId: 'role_treeitem'},
  video: {msgId: 'tag_video', inherits: 'abstractContainer'},
  window: {ignoreAncestry: true}
};

/**
 * Metadata about supported automation states.
 * @const {!Object<string, {on: {msgId: string, earconId: string},
 *                          off: {msgId: string, earconId: string},
 *                          isRoleSpecific: (boolean|undefined)}>}
 *     on: info used to describe a state that is set to true.
 *     off: info used to describe a state that is set to undefined.
 *     isRoleSpecific: info used for specific roles.
 * @private
 */
Output.STATE_INFO_ = {
  collapsed: {on: {msgId: 'aria_expanded_false'}},
  default: {on: {msgId: 'default_state'}},
  expanded: {on: {msgId: 'aria_expanded_true'}},
  multiselectable: {on: {msgId: 'aria_multiselectable_true'}},
  required: {on: {msgId: 'aria_required_true'}},
  visited: {on: {msgId: 'visited_state'}}
};

/**
 * Maps aria-current state types to message IDs.
 * @const {Object<string>}
 * @private
 */
Output.ARIA_CURRENT_STATE_INFO_ = {
  [AriaCurrentState.TRUE]: 'aria_current_true',
  [AriaCurrentState.PAGE]: 'aria_current_page',
  [AriaCurrentState.STEP]: 'aria_current_step',
  [AriaCurrentState.LOCATION]: 'aria_current_location',
  [AriaCurrentState.DATE]: 'aria_current_date',
  [AriaCurrentState.TIME]: 'aria_current_time'
};

/**
 * Maps input types to message IDs.
 * @const {Object<string>}
 * @private
 */
Output.INPUT_TYPE_MESSAGE_IDS_ = {
  'email': 'input_type_email',
  'number': 'input_type_number',
  'password': 'input_type_password',
  'search': 'input_type_search',
  'tel': 'input_type_number',
  'text': 'input_type_text',
  'url': 'input_type_url',
};

/**
 * Rules for mapping the restriction property to a msg id
 * @const {Object<string>}
 * @private
 */
Output.RESTRICTION_STATE_MAP = {};
Output.RESTRICTION_STATE_MAP[Restriction.DISABLED] = 'aria_disabled_true';
Output.RESTRICTION_STATE_MAP[Restriction.READ_ONLY] = 'aria_readonly_true';

/**
 * Rules for mapping the checked property to a msg id
 * @const {Object<string>}
 * @private
 */
Output.CHECKED_STATE_MAP = {
  'true': 'checked_true',
  'false': 'checked_false',
  'mixed': 'checked_mixed'
};

/**
 * Rules for mapping the checked property to a msg id
 * @const {Object<string>}
 * @private
 */
Output.PRESSED_STATE_MAP = {
  'true': 'aria_pressed_true',
  'false': 'aria_pressed_false',
  'mixed': 'aria_pressed_mixed'
};

/**
 * Rules specifying format of AutomationNodes for output.
 * @type {!Object<Object<Object<string>>>}
 * Please see below for more information on properties.
 * speak: The speech rule for when ChromeVox range lands exactly on the node.
 * braille: The braille rule for when ChromeVox range lands exactly on the node.
 * enter: The rule for when ChromeVox range enters the node's subtree.
 *    Can contain speak and braille properties.
 * leave: The rule for when ChromeVox range exits the node's subtree.
 */
Output.RULES = {
  navigate: {
    'default': {
      speak: `$name $node(activeDescendant) $value $state $restriction $role
          $description`,
      braille: ``
    },
    abstractContainer: {
      enter: `$nameFromNode $role $state $description`,
      leave: `@exited_container($role)`
    },
    abstractItem: {
      // Note that ChromeVox generally does not output position/count. Only for
      // some roles (see sub-output rules) or when explicitly provided by an
      // author (via posInSet), do we include them in the output.
      enter: `$nameFromNode $role $state $restriction $description
          $if($posInSet, @describe_index($posInSet, $setSize))`,
      speak: `$state $nameOrTextContent= $role
          $if($posInSet, @describe_index($posInSet, $setSize))
          $description $restriction`
    },
    abstractList: {
      enter: `$nameFromNode $role @@list_with_items($setSize)
          $restriction $description`
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
          $if($maxValueForRange, @aria_value_max($maxValueForRange))`
    },
    alert: {
      enter: `$name $role $state`,
      speak: `$earcon(ALERT_NONMODAL) $role $nameOrTextContent $description
          $state`
    },
    alertDialog: {
      enter: `$earcon(ALERT_MODAL) $name $state $description $textContent`,
      speak: `$earcon(ALERT_MODAL) $name $nameOrTextContent $description $state
          $role`
    },
    button: {
      speak: `$name $node(activeDescendant) $state $restriction $role
          $description`
    },
    cell: {
      enter: {
        speak: `$cellIndexText $node(tableCellColumnHeaders) $nameFromNode
            $state`,
        braille: `$state $cellIndexText $node(tableCellColumnHeaders)
            $nameFromNode`,
      },
      speak: `$name $cellIndexText $node(tableCellColumnHeaders)
          $state $description`,
      braille: `$state
          $name $cellIndexText $node(tableCellColumnHeaders) $description
          $if($selected, @aria_selected_true)`
    },
    checkBox: {
      speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $name $role $if($checkedStateDescription, $checkedStateDescription, $checked)
          $description $state $restriction`
    },
    client: {speak: `$name`},
    comboBoxMenuButton: {
      speak: `$name $if($collapsed, $value) $role @aria_has_popup
          $if($setSize, @@list_with_items($setSize))
          $state $restriction $description`,
    },
    date: {enter: `$nameFromNode $role $state $restriction $description`},
    dialog: {enter: `$nameFromNode $role $description $textContent`},
    genericContainer: {
      enter: `$nameFromNode $description $state`,
      speak: `$nameOrTextContent $description $state`
    },
    embeddedObject: {speak: `$name`},
    grid: {
      speak: `$name $node(activeDescendant) $role $state $restriction
          $description`
    },
    group: {
      enter: `$nameFromNode $state $restriction $description`,
      speak: `$nameOrDescendants $value $state $restriction $roleDescription
          $description`,
      leave: ``
    },
    heading: {
      enter: `!relativePitch(hierarchicalLevel)
          $nameFromNode=
          $if($hierarchicalLevel, @tag_h+$hierarchicalLevel, $role) $state
          $description`,
      speak: `!relativePitch(hierarchicalLevel)
          $nameOrDescendants=
          $if($hierarchicalLevel, @tag_h+$hierarchicalLevel, $role) $state
          $restriction $description`
    },
    image: {
      speak: `$if($name, $name,
          $if($imageAnnotation, $imageAnnotation, $urlFilename))
          $value $state $role $description`,
    },
    imeCandidate:
        {speak: '$name $phoneticReading @describe_index($posInSet, $setSize)'},
    inlineTextBox: {speak: `$name=`},
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
          @@list_with_items($setSize) $description $state`
    },
    listBoxOption: {
      speak: `$state $name $role @describe_index($posInSet, $setSize)
          $description $restriction
          $nif($selected, @aria_selected_false)`,
      braille: `$state $name $role @describe_index($posInSet, $setSize)
          $description $restriction
          $if($selected, @aria_selected_true, @aria_selected_false)`
    },
    listMarker: {speak: `$name`},
    menu: {
      enter: `$name $role `,
      speak: `$name $node(activeDescendant)
          $role @@list_with_items($setSize) $description $state $restriction`
    },
    menuItem: {
      speak: `$name $role $if($hasPopup, @has_submenu)
          @describe_index($posInSet, $setSize) $description $state $restriction`
    },
    menuItemCheckBox: {
      speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $name $role $checked $state $restriction $description
          @describe_index($posInSet, $setSize)`
    },
    menuItemRadio: {
      speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $if($checked, @describe_menu_item_radio_selected($name),
          @describe_menu_item_radio_unselected($name)) $state $roleDescription
          $restriction $description
          @describe_index($posInSet, $setSize)`
    },
    menuListOption: {
      speak: `$name $role @describe_index($posInSet, $setSize) $state
          $nif($selected, @aria_selected_false)
          $restriction $description`,
      braille: `$name $role @describe_index($posInSet, $setSize) $state
          $if($selected, @aria_selected_true, @aria_selected_false)
          $restriction $description`
    },
    paragraph: {speak: `$nameOrDescendants`},
    radioButton: {
      speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $if($checked, @describe_radio_selected($name),
          @describe_radio_unselected($name))
          @describe_index($posInSet, $setSize)
          $roleDescription $description $state $restriction`
    },
    rootWebArea: {enter: `$name`, speak: `$if($name, $name, @web_content)`},
    region: {speak: `$state $nameOrTextContent $description $roleDescription`},
    row: {
      enter: `$node(tableRowHeader)`,
      speak: `$name $node(activeDescendant) $value $state $restriction $role
          $if($selected, @aria_selected_true) $description`
    },
    staticText: {speak: `$name= $description`},
    switch: {
      speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $if($checked, @describe_switch_on($name),
          @describe_switch_off($name)) $roleDescription
          $description $state $restriction`
    },
    tab: {
      speak: `@describe_tab($name) $roleDescription $description
          @describe_index($posInSet, $setSize) $state $restriction
          $if($selected, @aria_selected_true)`,
    },
    table: {
      enter: `@table_summary($name,
          $if($ariaRowCount, $ariaRowCount, $tableRowCount),
          $if($ariaColumnCount, $ariaColumnCount, $tableColumnCount))
          $node(tableHeader)`
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
          $description $state $restriction`
    },
    timer: {
      speak: `$nameFromNode $descendants $value $state $role
        $description`
    },
    toggleButton: {
      speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $name $role $pressed $description $state $restriction`
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
          @describe_depth($hierarchicalLevel)`
    },
    unknown: {speak: ``},
    window: {
      enter: `@describe_window($name)`,
      speak: `@describe_window($name) $earcon(OBJECT_OPEN)`
    }
  },
  menuStart:
      {'default': {speak: `@chrome_menu_opened($name)  $earcon(OBJECT_OPEN)`}},
  menuEnd: {'default': {speak: `@chrome_menu_closed $earcon(OBJECT_CLOSE)`}},
  menuListValueChanged: {
    'default': {
      speak: `$value $name
          $find({"state": {"selected": true, "invisible": false}},
          @describe_index($posInSet, $setSize)) `
    }
  },
  alert: {
    default: {
      speak: `$earcon(ALERT_NONMODAL) @role_alert
          $nameOrTextContent $description`
    }
  }
};

/**
 * Used to annotate utterances with speech properties.
 */
Output.SpeechProperties = class {
  constructor() {
    /** @private {!Object} */
    this.properties_ = {};
  }

  /** @return {!Object} */
  get properties() {
    return this.properties_;
  }

  /** @override */
  toJSON() {
    // Make a copy of our properties since the caller really shouldn't be
    // modifying our local state.
    const clone = {};
    for (const key in this.properties_) {
      clone[key] = this.properties_[key];
    }
    return clone;
  }
};

/**
 * Custom actions performed while rendering an output string.
 */
Output.Action = class {
  constructor() {}

  run() {}
};


/**
 * Action to play an earcon.
 */
Output.EarconAction = class extends Output.Action {
  /**
   * @param {string} earconId
   * @param {chrome.automation.Rect=} opt_location
   */
  constructor(earconId, opt_location) {
    super();

    /** @type {string} */
    this.earconId = earconId;
    /** @type {chrome.automation.Rect|undefined} */
    this.location = opt_location;
  }

  /** @override */
  run() {
    ChromeVox.earcons.playEarcon(Earcon[this.earconId], this.location);
  }

  /** @override */
  toJSON() {
    return {earconId: this.earconId};
  }
};


/**
 * Annotation for text with a selection inside it.
 */
Output.SelectionSpan = class {
  /**
   * @param {number} startIndex
   * @param {number} endIndex
   */
  constructor(startIndex, endIndex) {
    // TODO(dtseng): Direction lost below; should preserve for braille panning.
    this.startIndex = startIndex < endIndex ? startIndex : endIndex;
    this.endIndex = endIndex > startIndex ? endIndex : startIndex;
  }
};

/**
 * Wrapper for automation nodes as annotations.  Since the
 * {@code AutomationNode} constructor isn't exposed in the API, this class is
 * used to allow instanceof checks on these annotations.
 */
Output.NodeSpan = class {
  /**
   * @param {!AutomationNode} node
   * @param {number=} opt_offset Offsets into the node's text. Defaults to 0.
   */
  constructor(node, opt_offset) {
    this.node = node;
    this.offset = opt_offset ? opt_offset : 0;
  }
};

/**
 * Possible events handled by ChromeVox internally.
 * @enum {string}
 */
Output.EventType = {
  NAVIGATE: 'navigate'
};

/**
 * If set, the next speech utterance will use this value instead of the normal
 * queueing mode.
 * @type {QueueMode|undefined}
 * @private
 */
Output.forceModeForNextSpeechUtterance_;
});  // goog.scope
