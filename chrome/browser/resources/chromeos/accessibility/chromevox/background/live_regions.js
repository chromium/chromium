// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implements support for live regions in ChromeVox.
 */
import {AutomationPredicate} from '/common/automation_predicate.js';
import {AutomationUtil} from '/common/automation_util.js';
import {CursorRange} from '/common/cursors/range.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {QueueMode, TtsCategory} from '../common/tts_types.js';

import {ChromeVoxRange} from './chromevox_range.js';
import {Output} from './output/output.js';
import {OutputCustomEvent} from './output/output_types.js';

const AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;
const TreeChange = chrome.automation.TreeChange;
const TreeChangeObserverFilter = chrome.automation.TreeChangeObserverFilter;
const TreeChangeType = chrome.automation.TreeChangeType;

/**
 * Handles events and announcements associated with "live regions", which are
 * regions marked as likely to change and important to announce.
 */
export class LiveRegions {
  /** @private */
  constructor() {
    /** @private {!Date} */
    this.lastDesktopLiveRegionChangedTime_ = new Date(0);

    /** @private {string} */
    this.lastDesktopLiveRegionChangedText_ = '';

    /**
     * The time the last live region event was output.
     * @type {!Date}
     * @private
     */
    this.lastLiveRegionTime_ = new Date(0);

    /**
     * Set of nodes that have been announced as part of a live region since
     *     |this.lastLiveRegionTime_|, to prevent duplicate announcements.
     * @type {!WeakSet<AutomationNode>}
     * @private
     */
    this.liveRegionNodeSet_ = new WeakSet();

    /**
     * A list of nodes that have changed as part of one atomic tree update.
     * @private {!Array}
     */
    this.changedNodes_ = [];

    chrome.automation.addTreeChangeObserver(
        TreeChangeObserverFilter.LIVE_REGION_TREE_CHANGES,
        treeChange => this.onTreeChange(treeChange));
  }

  static init() {
    if (LiveRegions.instance) {
      throw 'Error: Trying to create two instances of singleton LiveRegions';
    }
    LiveRegions.instance = new LiveRegions();
  }

  /** @param {!AutomationNode} area */
  static announceDesktopLiveRegionChanged(area) {
    const desktopOrApplication =
        AutomationPredicate.roles([RoleType.DESKTOP, RoleType.APPLICATION]);
    if (!area.root || !desktopOrApplication(area.root)) {
      return;
    }

    const output = new Output();
    if (area.containerLiveStatus === 'assertive') {
      output.withQueueMode(QueueMode.CATEGORY_FLUSH);
    } else if (area.containerLiveStatus === 'polite') {
      output.withQueueMode(QueueMode.QUEUE);
    } else {
      return;
    }

    const withinDelay =
        (new Date() - LiveRegions.instance.lastDesktopLiveRegionChangedTime_) <
        DESKTOP_CHANGE_DELAY_MS;

    output
        .withRichSpeechAndBraille(
            CursorRange.fromNode(area), null, EventType.LIVE_REGION_CHANGED)
        .withSpeechCategory(TtsCategory.LIVE);
    if (withinDelay &&
        output.toString() ===
            LiveRegions.instance.lastDesktopLiveRegionChangedText_) {
      return;
    }

    LiveRegions.instance.lastDesktopLiveRegionChangedTime_ = new Date();
    LiveRegions.instance.lastDesktopLiveRegionChangedText_ = output.toString();
    output.go();
  }

  /**
   * Called when the automation tree is changed.
   * @param {TreeChange} treeChange
   */
  onTreeChange(treeChange) {
    const type = treeChange.type;
    const node = treeChange.target;
    if ((!node.containerLiveStatus || node.containerLiveStatus === 'off') &&
        type !== TreeChangeType.SUBTREE_UPDATE_END) {
      return;
    }

    if (this.shouldIgnoreLiveRegion_(node)) {
      return;
    }

    const relevant = node.containerLiveRelevant || '';
    const additions = relevant.indexOf('additions') >= 0;
    const text = relevant.indexOf('text') >= 0;
    const removals = relevant.indexOf('removals') >= 0;
    const all = relevant.indexOf('all') >= 0;

    if (all ||
        (additions &&
         (type === TreeChangeType.NODE_CREATED ||
          type === TreeChangeType.SUBTREE_CREATED))) {
      this.queueLiveRegionChange_(node);
    } else if (all || (text && type === TreeChangeType.TEXT_CHANGED)) {
      this.queueLiveRegionChange_(node);
    }

    if ((all || removals) && type === TreeChangeType.NODE_REMOVED) {
      this.outputLiveRegionChange_(node, '@live_regions_removed');
    }

    if (type === TreeChangeType.SUBTREE_UPDATE_END) {
      this.processQueuedTreeChanges_();
    }
  }

  /**
   * @param {!AutomationNode} node
   * @private
   */
  queueLiveRegionChange_(node) {
    this.changedNodes_.push(node);
  }

  /**
   * @private
   */
  processQueuedTreeChanges_() {
    // Schedule all live regions after all events in the native C++
    // EventBundle.
    this.liveRegionNodeSet_ = new WeakSet();
    for (let i = 0; i < this.changedNodes_.length; i++) {
      const node = this.changedNodes_[i];
      this.outputLiveRegionChange_(node, null);
    }
    this.changedNodes_ = [];
  }

  /**
   * Given a node that needs to be spoken as part of a live region
   * change and an additional optional format string, output the
   * live region description.
   * @param {!AutomationNode} node The changed node.
   * @param {?string=} opt_prependFormatStr If set, a format string for
   *     Output to prepend to the output.
   * @private
   */
  outputLiveRegionChange_(node, opt_prependFormatStr) {
    if (node.containerLiveBusy) {
      return;
    }

    while (node.containerLiveAtomic && !node.liveAtomic && node.parent) {
      node = node.parent;
    }

    if (this.liveRegionNodeSet_.has(node)) {
      this.lastLiveRegionTime_ = new Date();
      return;
    }

    this.outputLiveRegionChangeForNode_(node, opt_prependFormatStr);
  }

  /**
   * @param {!AutomationNode} node The changed node.
   * @param {?string=} opt_prependFormatStr If set, a format string for
   *     Output to prepend to the output.
   * @private
   */
  outputLiveRegionChangeForNode_(node, opt_prependFormatStr) {
    const range = CursorRange.fromNode(node);
    const output = new Output();
    output.withSpeechCategory(TtsCategory.LIVE);

    // Queue live regions coming from background tabs.
    let hostView = AutomationUtil.getTopLevelRoot(node);
    hostView = hostView ? hostView.parent : null;
    const currentRange = ChromeVoxRange.current;
    const forceQueue = !hostView || !hostView.state.focused ||
        (currentRange && currentRange.start.node.root !== node.root) ||
        node.containerLiveStatus === 'polite';

    // Enqueue live region updates that were received at approximately
    // the same time, otherwise flush previous live region updates.
    const queueTime = LiveRegions.LIVE_REGION_QUEUE_TIME_MS;
    const currentTime = new Date();
    const delta = currentTime - this.lastLiveRegionTime_;
    if (delta > queueTime && !forceQueue) {
      output.withQueueMode(QueueMode.CATEGORY_FLUSH);
    } else {
      output.withQueueMode(QueueMode.QUEUE);
    }

    if (opt_prependFormatStr) {
      output.format(opt_prependFormatStr);
    }
    output.withSpeech(range, range, OutputCustomEvent.NAVIGATE);

    if (!output.hasSpeech && node.liveAtomic) {
      output.format('$joinedDescendants', node);
    }

    if (!output.hasSpeech) {
      return;
    }

    // We also have to add recursively the children of this live region node
    // since all children could potentially get described and we don't want to
    // describe their tree changes especially during page load within the
    // LiveRegions.LIVE_REGION_MIN_SAME_NODE_MS to prevent excessive chatter.
    this.addNodeToNodeSetRecursive_(node);
    output.go();
    this.lastLiveRegionTime_ = currentTime;
  }

  /**
   * @param {AutomationNode} root
   * @private
   */
  addNodeToNodeSetRecursive_(root) {
    this.liveRegionNodeSet_.add(root);
    for (let child = root.firstChild; child; child = child.nextSibling) {
      this.addNodeToNodeSetRecursive_(child);
    }
  }

  /**
   * @param {!AutomationNode} node
   * @return {boolean}
   * @private
   */
  shouldIgnoreLiveRegion_(node) {
    if (LiveRegions.announceLiveRegionsFromBackgroundTabs_) {
      return false;
    }

    const currentRange = ChromeVoxRange.current;
    if (currentRange && currentRange.start.node.root === node.root) {
      return false;
    }

    let hostView = AutomationUtil.getTopLevelRoot(node);
    hostView = hostView ? hostView.parent : null;
    if (!hostView) {
      return true;
    }

    if (hostView.role === RoleType.WINDOW &&
        !hostView.state[StateType.INVISIBLE]) {
      return false;
    }

    if (hostView.state.focused) {
      return false;
    }

    return true;
  }
}

/**
 * Live region events received in fewer than this many milliseconds will
 * queue, otherwise they'll be output with a category flush.
 * @const {number}
 */
LiveRegions.LIVE_REGION_QUEUE_TIME_MS = 5000;

/**
 * Live region events received on the same node in fewer than this many
 * milliseconds will be dropped to avoid a stream of constant chatter.
 * @const {number}
 */
LiveRegions.LIVE_REGION_MIN_SAME_NODE_MS = 20;

/**
 * Whether live regions from background tabs should be announced or not.
 * @private {boolean}
 */
LiveRegions.announceLiveRegionsFromBackgroundTabs_ = false;

/** @type {LiveRegions} */
LiveRegions.instance;

/**
 * Time to wait until processing more live region change events on the same
 * text content.
 * @const {number}
 */
const DESKTOP_CHANGE_DELAY_MS = 100;

TestImportManager.exportForTesting(LiveRegions);
