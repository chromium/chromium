// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides output services for ChromeVox.
 */
import {AutomationPredicate} from '/common/automation_predicate.js';
import {AutomationUtil} from '/common/automation_util.js';
import {constants} from '/common/constants.js';
import {Cursor} from '/common/cursors/cursor.js';
import {CursorRange} from '/common/cursors/range.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {NavBraille} from '../../common/braille/nav_braille.js';
import {EventSourceType} from '../../common/event_source_type.js';
import {LocaleOutputHelper} from '../../common/locale_output_helper.js';
import {LogType} from '../../common/log_types.js';
import {Msgs} from '../../common/msgs.js';
import {SettingsManager} from '../../common/settings_manager.js';
import {Spannable} from '../../common/spannable.js';
import {QueueMode, TtsCategory, TtsSpeechProperties} from '../../common/tts_types.js';
import {ValueSelectionSpan, ValueSpan} from '../braille/spans.js';
import {ChromeVox} from '../chromevox.js';
import {EventSource} from '../event_source.js';
import {FocusBounds} from '../focus_bounds.js';

import {BrailleOutput} from './braille_output.js';
import {OutputAncestryInfo} from './output_ancestry_info.js';
import {OutputFormatter} from './output_formatter.js';
import {AnnotationOptions, OutputInterface, RenderArgs} from './output_interface.js';
import {OutputFormatLogger} from './output_logger.js';
import {Info, OutputRoleInfo} from './output_role_info.js';
import {AncestryOutputRule, OutputRule} from './output_rules.js';
import * as outputTypes from './output_types.js';

import ScreenRect = chrome.accessibilityPrivate.ScreenRect;
import AutomationNode = chrome.automation.AutomationNode;
import EventType = chrome.automation.EventType;
import RoleType = chrome.automation.RoleType;
import StateType = chrome.automation.StateType;
import Dir = constants.Dir;

interface AncestryArgs {
  node: AutomationNode;
  prevNode?: AutomationNode;
  buff: Spannable[];
  formatLog: OutputFormatLogger;
  type: outputTypes.OutputEventType;
  ancestors: AutomationNode[];
  navigationType: outputTypes.OutputNavigationType;
  exclude?: AutomationNode[]|undefined;
  excludePreviousAncestors?: boolean|undefined;
}

interface MessageHint {
  outputFormat?: string;
  msgId?: string;
  subs?: string[];
  props?: outputTypes.OutputSpeechProperties;
  text?: string|Spannable;
}

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
 *
 * TODO(b/314204374): Eliminate instances of null.
 * @implements {OutputInterface}
 */
export class Output extends OutputInterface {
  /**
   * If set, the next speech utterance will use this value instead of the normal
   * queueing mode.
   */
  private static forceModeForNextSpeechUtterance_?: QueueMode;

  private speechBuffer_: Spannable[] = [];
  private brailleOutput_ = new BrailleOutput();
  private locations_: ScreenRect[] = [];
  private speechEndCallback_: (optCleanupOnly?: boolean) => void;

  // Store output rules.
  private speechFormatLog_: OutputFormatLogger;

  private formatOptions_:
      {speech: boolean, braille: boolean, auralStyle: boolean};

  // The speech category for the generated speech utterance.
  private speechCategory_: TtsCategory;

  // The speech queue mode for the generated speech utterance.
  private queueMode_?: QueueMode;

  private contextOrder_: outputTypes.OutputContextOrder;
  private suppressions_: {[token: string]: boolean} = {};
  private enableHints_: boolean = true;
  private initialSpeechProps_: TtsSpeechProperties;
  private drawFocusRing_: boolean = true;

  /**
   * Tracks all ancestors which have received primary formatting in
   * |ancestryHelper_|.
   */
  private formattedAncestors_: WeakSet<AutomationNode>;
  private replacements_: {[text: string]: string} = {};

  constructor() {
    super();
    // TODO(dtseng): Include braille specific rules.
    this.speechEndCallback_ = (() => {});

    this.speechFormatLog_ =
        new OutputFormatLogger('enableSpeechLogging', LogType.SPEECH_RULE);

    // Current global options.
    this.formatOptions_ = {speech: true, braille: false, auralStyle: false};

    this.speechCategory_ = TtsCategory.NAV;
    this.contextOrder_ = outputTypes.OutputContextOrder.LAST;
    this.initialSpeechProps_ = new TtsSpeechProperties();
    this.formattedAncestors_ = new WeakSet();
  }

  /**
   * Calling this will make the next speech utterance use |mode| even if it
   * would normally queue or do a category flush. This differs from the
   * |withQueueMode| instance method as it can apply to future output.
   */
  static forceModeForNextSpeechUtterance(mode?: QueueMode): void {
    if (Output.forceModeForNextSpeechUtterance_ === undefined ||
        mode === undefined ||
        // Only allow setting to higher queue modes.
        mode < Output.forceModeForNextSpeechUtterance_) {
      Output.forceModeForNextSpeechUtterance_ = mode;
    }
  }

  /** @return True if there is any speech that will be output. */
  get hasSpeech(): boolean {
    return this.speechBuffer_.some(speech => speech.length);
  }

  /** @return True if there is only whitespace in this output. */
  get isOnlyWhitespace(): boolean {
    return this.speechBuffer_.every(buff => !/\S+/.test(buff.toString()));
  }

  /** @return Spannable representing the braille output. */
  get braille(): Spannable {
    return BrailleOutput.mergeSpans(this.brailleOutput_.buffer);
  }

  /**
   * Specify ranges for speech.
   * @return |this| instance of Output for chaining.
   */
  withSpeech(
      range: CursorRange, prevRange: CursorRange|undefined,
      type: outputTypes.OutputEventType): this {
    this.formatOptions_ = {speech: true, braille: false, auralStyle: false};
    this.formattedAncestors_ = new WeakSet();
    this.render(
        range, prevRange, type, this.speechBuffer_, this.speechFormatLog_);
    return this;
  }

  /**
   * Specify ranges for aurally styled speech.
   * @return |this| instance of Output for chaining.
   */
  withRichSpeech(
      range: CursorRange, prevRange: CursorRange|undefined,
      type: outputTypes.OutputEventType): this {
    this.formatOptions_ = {speech: true, braille: false, auralStyle: true};
    this.formattedAncestors_ = new WeakSet();
    this.render(
        range, prevRange, type, this.speechBuffer_, this.speechFormatLog_);
    return this;
  }

  /**
   * Specify ranges for braille.
   * @return |this| instance of Output for chaining.
   */
  withBraille(
      range: CursorRange, prevRange: CursorRange|undefined,
      type: outputTypes.OutputEventType): this {
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
    this.render(
        range, prevRange, type, this.brailleOutput_.buffer,
        this.brailleOutput_.formatLog);
    return this;
  }

  /**
   * Specify ranges for location.
   *  @return |this| instance of Output for chaining.
   */
  withLocation(
      range: CursorRange, prevRange: CursorRange|undefined,
      type: outputTypes.OutputEventType): this {
    this.formatOptions_ = {speech: false, braille: false, auralStyle: false};
    this.formattedAncestors_ = new WeakSet();
    this.render(
        range, prevRange, type, [] /*unused output*/,
        new OutputFormatLogger('', LogType.SPEECH_RULE) /*unused log*/);
    return this;
  }

  /**
   * Specify the same ranges for speech and braille.
   * @return |this| instance of Output for chaining.
   */
  withSpeechAndBraille(
      range: CursorRange, prevRange: CursorRange|undefined,
      type: outputTypes.OutputEventType): this {
    this.withSpeech(range, prevRange, type);
    this.withBraille(range, prevRange, type);
    return this;
  }

  /**
   * Specify the same ranges for aurally styled speech and braille.
   * @return |this| instance of Output for chaining.
   */
  withRichSpeechAndBraille(
      range: CursorRange, prevRange: CursorRange|undefined,
      type: outputTypes.OutputEventType): this {
    this.withRichSpeech(range, prevRange, type);
    this.withBraille(range, prevRange, type);
    return this;
  }

  /**
   * Applies the given speech category to the output.
   * @return |this| instance of Output for chaining.
   */
  withSpeechCategory(category: TtsCategory): this {
    this.speechCategory_ = category;
    return this;
  }

  /**
   * Applies the given speech queue mode to the output.
   * @return |this| instance of Output for chaining.
   */
  withQueueMode(queueMode: QueueMode|undefined): this {
    this.queueMode_ = queueMode;
    return this;
  }

  /**
   * Outputs a string literal.
   * @return |this| instance of Output for chaining.
   */
  withString(value: string): this {
    this.append(this.speechBuffer_, value);
    this.append(this.brailleOutput_.buffer, value);
    this.speechFormatLog_.write('withString: ' + value + '\n');
    this.brailleOutput_.formatLog.write('withString: ' + value + '\n');
    return this;
  }

  /**
   * Outputs formatting nodes after this will contain context first.
   * @return |this| instance of Output for chaining.
   */
  withContextFirst(): this {
    this.contextOrder_ = outputTypes.OutputContextOrder.FIRST;
    return this;
  }

  /**
   * Don't include hints in subsequent output.
   * @return |this| instance of Output for chaining.
   */
  withoutHints(): this {
    this.enableHints_ = false;
    return this;
  }

  /**
   * Don't draw a focus ring based on this output.
   * @return |this| instance of Output for chaining.
   */
  withoutFocusRing(): this {
    this.drawFocusRing_ = false;
    return this;
  }

  /**
   * Applies given initial speech properties to all output.
   * @return |this| instance of Output for chaining.
   */
  withInitialSpeechProperties(speechProps: TtsSpeechProperties): this {
    this.initialSpeechProps_ = speechProps;
    return this;
  }

  /**
   * Given a string of text and a string to replace |text| with,
   * causes any speech output to apply the replacement.
   * @param text The text to be replaced.
   * @param replace The string to replace |text| with.
   */
  withSpeechTextReplacement(text: string, replace: string): void {
    this.replacements_[text] = replace;
  }

  /**
   * Suppresses processing of a token for subsequent formatting commands.
   * @return |this| instance of Output for chaining.
   */
  suppress(token: string): this {
    this.suppressions_[token] = true;
    return this;
  }

  /**
   * Apply a format string directly to the output buffer. This lets you
   * output a message directly to the buffer using the format syntax.
   * @param formatStr The format string to apply.
   * @param optNode Optional node to apply the formatting to.
   * @return |this| instance of Output for chaining.
   */
  format(formatStr: string, optNode?: AutomationNode): this {
    return this.formatForSpeech(formatStr, optNode)
        .formatForBraille(formatStr, optNode);
  }

  /**
   * Apply a format string directly to the speech output buffer. This lets you
   * output a message directly to the buffer using the format syntax.
   * @param formatStr The format string to apply.
   * @param optNode Optional node to apply the formatting to.
   * @return |this| instance of Output for chaining.
   */
  formatForSpeech(formatStr: string, optNode?: AutomationNode): this {
    const node = optNode || undefined;

    this.formatOptions_ = {speech: true, braille: false, auralStyle: false};
    this.formattedAncestors_ = new WeakSet();
    OutputFormatter.format(this, {
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
   * @param formatStr The format string to apply.
   * @param optNode Optional node to apply the formatting to.
   * @return |this| instance of Output for chaining.
   */
  formatForBraille(formatStr: string, optNode?: AutomationNode): this {
    const node = optNode || undefined;

    this.formatOptions_ = {speech: false, braille: true, auralStyle: false};
    this.formattedAncestors_ = new WeakSet();
    OutputFormatter.format(this, {
      node,
      outputFormat: formatStr,
      outputBuffer: this.brailleOutput_.buffer,
      outputFormatLogger: this.brailleOutput_.formatLog,
    });
    return this;
  }

  /**
   * Triggers callback for a speech event.
   * @param callback Callback function that takes in an optional boolean and has
   *     a void return.
   * @return |this| instance of Output for chaining.
   */
  onSpeechEnd(callback: (optCleanupOnly?: boolean) => void): this {
    this.speechEndCallback_ = (optCleanupOnly => {
      if (!optCleanupOnly) {
        callback();
      }
    });
    return this;
  }

  /** Executes all specified output. */
  go(): void {
    // Speech.
    this.sendSpeech_();

    // Braille.
    if (this.brailleOutput_.buffer.length) {
      this.sendBraille_();
    }

    // Display.
    if (this.speechCategory_ !== TtsCategory.LIVE && this.drawFocusRing_) {
      FocusBounds.set(this.locations_);
      if (this.locations_ !== undefined && this.locations_.length !== 0) {
        chrome.accessibilityPrivate.setChromeVoxFocus(this.locations_[0]);
      }
    }
  }

  private determineQueueMode_(): QueueMode {
    if (Output.forceModeForNextSpeechUtterance_ !== undefined) {
      const result = Output.forceModeForNextSpeechUtterance_;
      if (this.speechBuffer_.length > 0) {
        Output.forceModeForNextSpeechUtterance_ = undefined;
      }
      return result;
    }
    if (this.queueMode_ !== undefined) {
      return this.queueMode_;
    }
    return QueueMode.QUEUE;
  }

  /**
   * @return speech properties for given buffer.
   */
  private getSpeechPropsForBuff_(buff: Spannable): TtsSpeechProperties {
    let speechProps;
    const speechPropsInstance =
        (buff.getSpanInstanceOf(outputTypes.OutputSpeechProperties));

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
    speechProps.startCallback = () => {
      const actions = buff.getSpansInstanceOf(outputTypes.OutputAction);
      if (actions) {
        actions.forEach(action => action.run());
      }
    };

    return speechProps;
  }

  private sendBraille_(): void {
    const buff = BrailleOutput.mergeSpans(this.brailleOutput_.buffer);
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
        console.log(e);
      }
    }

    const output = new NavBraille({text: buff, startIndex, endIndex});

    ChromeVox.braille.write(output);
    this.brailleOutput_.formatLog.commitLogs();
  }

  private sendSpeech_(): void {
    let queueMode = this.determineQueueMode_();

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

      const speechProps = this.getSpeechPropsForBuff_(buff);

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
  }

  /**
   * @param rhs Object to compare.
   * @return True if this object is equal to |rhs|.
   */
  equals(rhs: Output): boolean {
    if (this.speechBuffer_.length !== rhs.speechBuffer_.length) {
      return false;
    }

    for (let i = 0; i < this.speechBuffer_.length; i++) {
      if (this.speechBuffer_[i].toString() !==
          rhs.speechBuffer_[i].toString()) {
        return false;
      }
    }

    return this.brailleOutput_.equals(rhs.brailleOutput_);
  }

  override render(
      range: CursorRange, prevRange: CursorRange|undefined,
      type: outputTypes.OutputEventType, buff: Spannable[],
      formatLog: OutputFormatLogger, _optionalArgs?: {}): void {
    if (prevRange && !prevRange.isValid()) {
      prevRange = undefined;
    }

    // Scan all ancestors to get the value of |contextOrder|.
    let parent: AutomationNode|undefined = range.start.node;
    const prevParent = prevRange ? prevRange.start.node : parent;
    if (!parent || !prevParent) {
      return;
    }

    while (parent) {
      // TODO(b/314203187): Determine if not null assertion is acceptable.
      if (parent.role! === RoleType.WINDOW) {
        break;
      }
      // TODO(b/314203187): Determine if not null assertion is acceptable.
      if (OutputRoleInfo[parent.role!] &&
          OutputRoleInfo[parent.role!]?.contextOrder) {
        // TODO(b/314203187): Determine if not null assertion is acceptable.
        this.contextOrder_ =
            OutputRoleInfo[parent.role!]?.contextOrder || this.contextOrder_;
        break;
      }

      parent = parent.parent;
    }

    if (range.isSubNode()) {
      this.subNode_(range, prevRange, type, buff, formatLog);
    } else {
      this.range_(range, prevRange, type, buff, formatLog, _optionalArgs);
    }

    this.hint_(
        range, AutomationUtil.getUniqueAncestors(prevParent, range.start.node),
        type, buff, formatLog);
  }

  private range_(
      range: CursorRange, prevRange: CursorRange|undefined,
      type: outputTypes.OutputEventType, rangeBuff: Spannable[],
      formatLog: OutputFormatLogger, _optionalArgs?: {}): void {
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

    const formatNodeAndAncestors =
        (node: AutomationNode, prevNode: AutomationNode): Spannable[] => {
          const buff: Spannable[] = [];

          if (addContextBefore) {
            this.ancestry_(
                node, prevNode, type, buff, formatLog,
                {preferStart: preferStartOrEndAncestry});
          }
          this.formatNode(node, prevNode, type, buff, formatLog);
          if (addContextAfter) {
            this.ancestry_(
                node, prevNode, type, buff, formatLog,
                {preferEnd: preferStartOrEndAncestry});
          }

          if (node.activeDescendant?.location) {
            this.locations_.push(node.activeDescendant.location);
          } else if (node.location) {
            this.locations_.push(node.location);
          }
          return buff;
        };

    let lca: AutomationNode|undefined|null = null;
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

    // TODO(b/314203187): Determine if not null assertion is acceptable.
    if (AutomationPredicate.selectableText(range.end.node) &&
        range.end.index >= 0 && range.end.index < range.end.node.name!.length) {
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
        const rangeStartNodeName: string|undefined = range.start.node.name;
        const nodeName: string|undefined = node.name;
        if (range.start.index !== rangeStartNodeName?.length) {
          const partialRange = new CursorRange(
              new Cursor(node, range.start.index),
              // TODO(b/314203187): Determine if not null assertion is
              // acceptable.
              new Cursor(
                  node, nodeName?.length!, {preferNodeStartEquivalent: true}));
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

  private ancestry_(
      node: AutomationNode, prevNode: AutomationNode,
      type: outputTypes.OutputEventType, buff: Spannable[],
      formatLog: OutputFormatLogger, optionalArgs?: RenderArgs): void {
    if (!SettingsManager.get('useVerboseMode')) {
      return;
    }

    if (node.role && OutputRoleInfo[node.role] &&
        OutputRoleInfo[node.role]?.ignoreAncestry) {
      return;
    }

    const info = new OutputAncestryInfo(
        node, prevNode, Boolean(optionalArgs?.suppressStartEndAncestry));

    // Enter, leave ancestry.
    this.ancestryHelper_({
      node,
      prevNode,
      buff,
      formatLog,
      type,
      ancestors: info.leaveAncestors,
      navigationType: outputTypes.OutputNavigationType.LEAVE,
      exclude: [...info.enterAncestors, node],
    });
    this.ancestryHelper_({
      node,
      prevNode,
      buff,
      formatLog,
      type,
      ancestors: info.enterAncestors,
      navigationType: outputTypes.OutputNavigationType.ENTER,
      excludePreviousAncestors: true,
    });

    if (optionalArgs?.suppressStartEndAncestry) {
      return;
    }

    // Start of, end of ancestry.
    if (!optionalArgs?.preferEnd) {
      this.ancestryHelper_({
        node,
        prevNode,
        buff,
        formatLog,
        type,
        ancestors: info.startAncestors,
        navigationType: outputTypes.OutputNavigationType.START_OF,
        excludePreviousAncestors: true,
      });
    }

    if (!optionalArgs?.preferStart) {
      this.ancestryHelper_({
        node,
        prevNode,
        buff,
        formatLog,
        type,
        ancestors: info.endAncestors,
        navigationType: outputTypes.OutputNavigationType.END_OF,
        exclude: [...info.startAncestors].concat(node),
      });
    }
  }

  private ancestryHelper_(args: AncestryArgs): void {
    let {prevNode, buff, formatLog, type, ancestors, navigationType} = args;

    const excludeRoles =
        args.exclude ? new Set(args.exclude.map(node => node.role)) : new Set();

    // Customize for braille node annotations.
    const originalBuff = buff;
    for (let j = ancestors.length - 1, formatNode; (formatNode = ancestors[j]);
         j--) {
      // TODO(b/314203187): Determine if not null assertion is acceptable.
      const roleInfo: Info|undefined = OutputRoleInfo[formatNode.role!] || {};
      if (!roleInfo?.verboseAncestry &&
          (excludeRoles.has(formatNode.role) ||
           (args.excludePreviousAncestors &&
            this.formattedAncestors_.has(formatNode)))) {
        continue;
      }

      const rule = new AncestryOutputRule(
          type, formatNode.role, navigationType, this.formatAsBraille);
      if (!rule.defined) {
        continue;
      }

      if (this.formatAsBraille) {
        buff = [];
        formatLog.bufferClear();
      }

      excludeRoles.add(formatNode.role);
      formatLog.writeRule(rule.specifier);
      this.formattedAncestors_.add(formatNode);
      OutputFormatter.format(this, {
        node: formatNode,
        outputFormat: rule.enterFormat,
        outputBuffer: buff,
        outputFormatLogger: formatLog,
        opt_prevNode: prevNode,
      });

      if (this.formatAsBraille && buff.length) {
        const nodeSpan = BrailleOutput.mergeSpans(buff);
        nodeSpan.setSpan(
            new outputTypes.OutputNodeSpan(formatNode), 0, nodeSpan.length);
        originalBuff.push(nodeSpan);
      }
    }
  }

  override formatNode(
      node: AutomationNode, prevNode: AutomationNode,
      type: outputTypes.OutputEventType, buff: Spannable[],
      formatLog: OutputFormatLogger): void {
    const originalBuff = buff;

    if (this.formatOptions_.braille) {
      buff = [];
      formatLog.bufferClear();
    }

    const rule = new OutputRule(type);
    rule.output = outputTypes.OutputFormatType.SPEAK;
    rule.populateRole(node.role, rule.output);
    if (this.formatOptions_.braille) {
      // Overwrite rule by braille rule if exists.
      if (rule.populateRole(node.role, outputTypes.OutputFormatType.BRAILLE)) {
        rule.output = outputTypes.OutputFormatType.BRAILLE;
      }
    }
    formatLog.writeRule(rule.specifier);
    OutputFormatter.format(this, {
      node,
      outputFormat: rule.formatString,
      outputBuffer: buff,
      outputFormatLogger: formatLog,
      opt_prevNode: prevNode,
    });

    // Restore braille and add an annotation for this node.
    if (this.formatOptions_.braille) {
      const nodeSpan = BrailleOutput.mergeSpans(buff);
      nodeSpan.setSpan(
          new outputTypes.OutputNodeSpan(node), 0, nodeSpan.length);
      originalBuff.push(nodeSpan);
    }
  }

  private subNode_(
      range: CursorRange, prevRange: CursorRange|undefined,
      type: outputTypes.OutputEventType, buff: Spannable[],
      formatLog: OutputFormatLogger): void {
    if (!prevRange) {
      prevRange = range;
    }
    const dir = CursorRange.getDirection(prevRange, range);
    const node = range.start.node;
    const prevNode = prevRange.getBound(dir).node;
    if (!node || !prevNode) {
      return;
    }

    let options:
        outputTypes.AppendOptions = {annotation: ['name'], isUnique: true};
    const rangeStart = range.start.index;
    const rangeEnd = range.end.index;

    if (this.formatOptions_.braille) {
      options = this.brailleOutput_.subNode(range, options);
    }

    // Intentionally skip subnode output for
    // outputTypes.OutputContextOrder.DIRECTED.
    if (this.contextOrder_ === outputTypes.OutputContextOrder.FIRST ||
        (this.contextOrder_ === outputTypes.OutputContextOrder.FIRST_AND_LAST &&
         range.start.index === 0)) {
      this.ancestry_(
          node, prevNode, type, buff, formatLog, {preferStart: true});
    }
    const earcon = this.findEarcon(node, prevNode);
    if (earcon) {
      options.annotation.push(earcon);
    }
    let text = '';

    // TODO(b/314203187): Determine if not null assertion is acceptable.
    if (this.formatOptions_.braille && !node.state![StateType.EDITABLE]) {
      // In braille, we almost always want to show the entire contents and
      // simply place the cursor under the SelectionSpan we set above.
      text = range.start.getText();
    } else {
      // This is output for speech or editable braille.
      text = range.start.getText().substring(rangeStart, rangeEnd);
    }

    if (SettingsManager.get('languageSwitching')) {
      this.assignLocaleAndAppend(text, node, buff, options);
    } else {
      this.append(buff, text, options);
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
   *
   * @param buff receives the rendered output.
   */
  private hint_(
      range: CursorRange, uniqueAncestors: AutomationNode[],
      type: outputTypes.OutputEventType, buff: Spannable[],
      formatLog: OutputFormatLogger): void {
    if (!this.enableHints_ || !SettingsManager.get('useVerboseMode')) {
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
    const delayedMsgs = Output.computeDelayedHints_(node, uniqueAncestors);
    if (delayedMsgs.length > 0) {
      delayedMsgs[0].props = new outputTypes.OutputSpeechProperties();
      delayedMsgs[0].props.properties['delay'] = true;
    }

    const allMsgs = msgs.concat(delayedMsgs);
    for (const msg of allMsgs) {
      if (msg.msgId) {
        const text = Msgs.getMsg(msg.msgId, msg.subs);
        this.append(buff, text, {annotation: [msg.props]});
        formatLog.write('hint_: ' + text + '\n');
      } else if (msg.text) {
        this.append(buff, msg.text, {annotation: [msg.props]});
        formatLog.write('hint_: ' + msg.text + '\n');
      } else if (msg.outputFormat) {
        formatLog.write('hint_: ...');
        OutputFormatter.format(this, {
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
   * Note that the above caller
   * expects one and only one key be set.
   * @return a list of message hints.
   */
  private static computeHints_(
      node: AutomationNode, uniqueAncestors: AutomationNode[]): MessageHint[] {
    const ret: MessageHint[] = [];
    if (node['errorMessage']) {
      ret.push({outputFormat: '$node(errorMessage)'});
    }

    // Provide a hint for sort direction.
    let sortDirectionNode: AutomationNode|undefined = node;
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
          outputTypes
              .OutputPropertyMap['STATE'][currentNode.ariaCurrentState]) {
        ret.push({
          msgId: outputTypes
                     .OutputPropertyMap['STATE'][currentNode.ariaCurrentState],
        });
        break;
      }
      currentNode = uniqueAncestors[ancestorIndex++];
    } while (currentNode);

    return ret;
  }

  /**
   * Internal helper to |hint_|. Returns a list of message hints.
   * Note that the above caller
   * expects one and only one key be set.
   * @return a list of message hints.
   */
  private static computeDelayedHints_(
      node: AutomationNode, uniqueAncestors: AutomationNode[]): MessageHint[] {
    const ret: MessageHint[] = [];
    // TODO(b/314203187): Determine if not null assertion is acceptable.
    const nodeState: Partial<Record<StateType, boolean>> = node.state!;
    if (EventSource.get() === EventSourceType.TOUCH_GESTURE) {
      if (nodeState[StateType.EDITABLE]) {
        ret.push({
          msgId: nodeState[StateType.FOCUSED] ? 'hint_is_editing' :
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

    if (nodeState[StateType.EDITABLE] && nodeState[StateType.FOCUSED] &&
        (nodeState[StateType.MULTILINE] ||
         nodeState[StateType.RICHLY_EDITABLE])) {
      ret.push({msgId: 'hint_search_within_text_field'});
    }

    if (node.placeholder) {
      ret.push({text: node.placeholder});
    }

    // Invalid Grammar text.
    if (uniqueAncestors.find(AutomationPredicate.hasInvalidGrammarMarker)) {
      ret.push({msgId: 'hint_invalid_grammar'});
    }

    // Invalid Spelling text.
    if (uniqueAncestors.find(AutomationPredicate.hasInvalidSpellingMarker)) {
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
        nodeState[StateType.AUTOFILL_AVAILABLE]) {
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
    let foundAncestor: AutomationNode|undefined;
    if (uniqueAncestors.find(AutomationPredicate.table)) {
      ret.push({msgId: 'hint_table'});
    }

    // This hint is not based on the role (it uses state), so we need to care
    // about ordering; prefer deepest ancestor first.
    if (foundAncestor = uniqueAncestors.reverse().find(
            (AutomationPredicate.roles([RoleType.MENU, RoleType.MENU_BAR])))) {
      if (foundAncestor.state && foundAncestor.state['horizontal']) {
        ret.push({msgId: 'hint_menu_horizontal'});
      } else {
        ret.push({msgId: 'hint_menu'});
      }
    }
    if (uniqueAncestors.find(function(n) {
          return Boolean(n.details);
        })) {
      ret.push({msgId: 'hint_details'});
    }

    return ret;
  }

  override append(
      buff: Spannable[], value?: string|Spannable,
      optOptions?: AnnotationOptions): void {
    optOptions = optOptions || {isUnique: false, annotation: []};

    // Reject empty values without meaningful annotations.
    if ((!value || value.length === 0) &&
        optOptions.annotation.every(
            annotation => !(annotation instanceof outputTypes.OutputAction) &&
                !(annotation instanceof outputTypes.OutputSelectionSpan))) {
      return;
    }

    const spannableToAdd = new Spannable(value);
    optOptions.annotation.forEach(
        annotation =>
            spannableToAdd.setSpan(annotation, 0, spannableToAdd.length));

    // |isUnique| specifies an annotation that cannot be duplicated.
    if (optOptions.isUnique) {
      const annotationSansNodes = optOptions.annotation.filter(
          annotation => !(annotation instanceof outputTypes.OutputNodeSpan));

      const alreadyAnnotated = buff.some((spannable: Spannable) => {
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
   * @return found OutputAction or if not found, undefined/null.
   */
  override findEarcon(node: AutomationNode, optPrevNode?: AutomationNode):
      outputTypes.OutputAction|undefined {
    if (node === optPrevNode) {
      // TODO(b/314203187): Determine if not null assertion is acceptable.
      return null!;
    }

    if (this.formatOptions_.speech) {
      let earconFinder: AutomationNode|undefined = node;
      let ancestors;
      if (optPrevNode) {
        ancestors = AutomationUtil.getUniqueAncestors(optPrevNode, node);
      } else {
        ancestors = AutomationUtil.getAncestors(node);
      }

      while (earconFinder = ancestors.pop()) {
        // TODO(b/314203187): Determine if not null assertion is acceptable.
        const role: chrome.automation.RoleType = earconFinder.role!;
        const info = OutputRoleInfo[role];
        if (info && info.earcon) {
          return new outputTypes.OutputEarconAction(
              info.earcon, node.location || undefined);
        }
        earconFinder = earconFinder.parent;
      }
    }
    // TODO(b/314203187): Determine if not null assertion is acceptable.
    return null!;
  }

  /**
   * @returns a human friendly string with the contents of output.
   */
  override toString(): string {
    return this.speechBuffer_.map(v => v.toString()).join(' ');
  }

  /**
   * Gets the spoken output with separator '|'.
   * @return Spannable containing spoken output with separator '|'.
   */
  get speechOutputForTest(): Spannable {
    return this.speechBuffer_.reduce(
        (prev: Spannable|null, cur: Spannable|null) => {
          if (prev === null) {
            return cur;
          }
          prev.append('|');
          prev.append(cur!);
          return prev;
          // TODO(b/314203187): Determine if not null assertion is acceptable.
        },
        null)!;
  }

  override assignLocaleAndAppend(
      text: string, contextNode: AutomationNode, buff: Spannable[],
      options?: AnnotationOptions): void {
    const data =
        LocaleOutputHelper.instance.computeTextAndLocale(text, contextNode);
    const speechProps = new outputTypes.OutputSpeechProperties();
    speechProps.properties['lang'] = data.locale;
    this.append(buff, data.text, options);
    // Attach associated SpeechProperties if the buffer is
    // non-empty.
    if (buff.length > 0) {
      buff[buff.length - 1].setSpan(speechProps, 0, 0);
    }
  }

  override shouldSuppress(token: string): boolean {
    return this.suppressions_[token];
  }

  override get useAuralStyle(): boolean {
    return this.formatOptions_.auralStyle;
  }

  override get formatAsBraille(): boolean {
    return this.formatOptions_.braille;
  }

  override get formatAsSpeech(): boolean {
    return this.formatOptions_.speech;
  }
}

TestImportManager.exportForTesting(Output);
