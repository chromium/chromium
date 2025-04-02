// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles media automation events.
 */
import {AsyncUtil} from '/common/async_util.js';

import {SettingsManager} from '../../common/settings_manager.js';
import {ChromeVox} from '../chromevox.js';

import {BaseAutomationHandler} from './base_automation_handler.js';

type AutomationEvent = chrome.automation.AutomationEvent;
type AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;

export class MediaAutomationHandler extends BaseAutomationHandler {
  static MIN_WAITTIME_MS = 1000;
  static instance: MediaAutomationHandler|null = null;

  private mediaRoots_: Set<AutomationNode> = new Set();
  private lastTtsEvent_: Date = new Date();

  private async addListeners_(): Promise<void> {
    ChromeVox.tts.addCapturingEventListener(this);

    this.node_ = await AsyncUtil.getDesktop();

    this.addListener_(
        EventType.MEDIA_STARTED_PLAYING, this.onMediaStartedPlaying);
    this.addListener_(
        EventType.MEDIA_STOPPED_PLAYING, this.onMediaStoppedPlaying);
  }

  static async init(): Promise<void> {
    if (MediaAutomationHandler.instance) {
      throw 'Error: trying to create two instances of singleton MediaAutomationHandler';
    }
    MediaAutomationHandler.instance = new MediaAutomationHandler();
    await MediaAutomationHandler.instance.addListeners_();
  }

  onTtsStart(): void {
    this.lastTtsEvent_ = new Date();
    this.update_({start: true});
  }

  onTtsEnd(): void {
    const now = new Date();
    setTimeout(() => {
      const then = this.lastTtsEvent_;
      if (now < then) {
        return;
      }
      this.lastTtsEvent_ = now;
      this.update_({end: true});
    }, MediaAutomationHandler.MIN_WAITTIME_MS);
  }

  onTtsInterrupted(): void {
    this.onTtsEnd();
  }

  onMediaStartedPlaying(evt: AutomationEvent): void {
    this.mediaRoots_.add(evt.target);
    const audioStrategy = SettingsManager.get('audioStrategy');
    if (ChromeVox.tts.isSpeaking() && audioStrategy === 'audioDuck') {
      this.update_({start: true});
    }
  }

  onMediaStoppedPlaying(): void {
    // Intentionally does nothing (to cover resume).
  }

  /**
   * Updates the media state for all observed automation roots.
   */
  private update_(options: {start?: boolean; end?: boolean}): void {
    const it = this.mediaRoots_.values();
    let item = it.next();
    const audioStrategy = SettingsManager.get('audioStrategy');
    while (!item.done) {
      const root = item.value;
      if (options.start) {
        if (audioStrategy === 'audioDuck') {
          root.startDuckingMedia();
        } else if (audioStrategy === 'audioSuspend') {
          root.suspendMedia();
        }
      } else if (options.end) {
        if (audioStrategy === 'audioDuck') {
          root.stopDuckingMedia();
        } else if (audioStrategy === 'audioSuspend') {
          root.resumeMedia();
        }
      }
      item = it.next();
    }
  }
}
