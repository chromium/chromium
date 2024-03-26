// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox log page.
 */
import {BackgroundBridge} from '../common/background_bridge.js';
import {LogType, SerializableLog} from '../common/log_types.js';

/** Class to manage the log page. */
export class LogPage {
  static instance: LogPage;

  constructor() {
    this.initPage_();
  }

  static async init(): Promise<void> {
    if (LogPage.instance) {
      throw new Error('LogPage can only be initiated once.');
    }
    LogPage.instance = new LogPage();
    await LogPage.instance.update();
  }

  private addLogToPage_(log: SerializableLog): void {
    const div = document.getElementById(IdName.LIST);
    const p = document.createElement(ElementName.PARAGRAPH);

    const typeName = document.createElement(ElementName.SPAN);
    typeName.textContent = log.logType;
    typeName.className = ClassName.TYPE;
    p.appendChild(typeName);

    const timeStamp = document.createElement(ElementName.SPAN);
    timeStamp.textContent = this.formatTimeStamp_(log.date);
    timeStamp.className = ClassName.TIME;
    p.appendChild(timeStamp);

    /** Add hide tree button when logType is tree. */
    if (log.logType === LogType.TREE) {
      const toggle = document.createElement(ElementName.LABEL);
      const toggleCheckbox =
          document.createElement(ElementName.INPUT) as HTMLInputElement;
      toggleCheckbox.type = InputType.CHECKBOX;
      toggleCheckbox.checked = true;
      toggleCheckbox.onclick = event => textWrapper.hidden =
          !(event.target as HTMLInputElement).checked;

      const toggleText = document.createElement(ElementName.SPAN);
      toggleText.textContent = 'show tree';
      toggle.appendChild(toggleCheckbox);
      toggle.appendChild(toggleText);
      p.appendChild(toggle);
    }

    /** textWrapper should be in block scope, not function scope. */
    const textWrapper = document.createElement(ElementName.PRE);
    textWrapper.textContent = log.value;
    textWrapper.className = ClassName.TEXT;
    p.appendChild(textWrapper);

    // TODO(b/314203187): Not null asserted, check that this is correct.
    div!.appendChild(p);
  }

  private checkboxId_(type: LogType): string {
    return type + 'Filter';
  }

  private createFilterCheckbox_(type: LogType, checked: boolean): void {
    const label = document.createElement(ElementName.LABEL) as HTMLLabelElement;
    const input = document.createElement(ElementName.INPUT) as HTMLInputElement;
    input.id = this.checkboxId_(type);
    input.type = InputType.CHECKBOX;
    input.classList.add(ClassName.FILTER);
    input.checked = checked;
    input.addEventListener(EventType.CLICK, () => this.updateUrlParams_());
    label.appendChild(input);

    const span = document.createElement(ElementName.SPAN);
    span.textContent = type;
    label.appendChild(span);

    // TODO(b/314203187): Not null asserted, check that this is correct.
    document.getElementById(IdName.FILTER)!.appendChild(label);
  }

  private getDownloadFileName_(): string {
    const date = new Date();
    return [
      'chromevox_logpage',
      date.getMonth() + 1,
      date.getDate(),
      date.getHours(),
      date.getMinutes(),
      date.getSeconds(),
    ].join('_') +
        '.txt';
  }

  private initPage_(): void {
    const params = new URLSearchParams(location.search);
    for (const type of Object.values(LogType)) {
      const enabled =
          (params.get(type) === String(true) || params.get(type) === null);
      this.createFilterCheckbox_(type, enabled);
    }

    // TODO(b/314203187): Not null asserted, check that this is correct.
    const clearLogButton = document.getElementById(IdName.CLEAR);
    clearLogButton!.onclick = () => this.onClear_();

    const saveLogButton = document.getElementById(IdName.SAVE);
    saveLogButton!.onclick = event => this.onSave_(event);
  }

  private isEnabled_(type: LogType): boolean {
    const element =
        document.getElementById(this.checkboxId_(type)) as HTMLInputElement;
    return element.checked;
  }

  private logToString_(log: Element): string {
    const logText: string[] = [];
    // TODO(b/314203187): Not null asserted, check that this is correct.
    logText.push(log.querySelector(`.${ClassName.TYPE}`)!.textContent!);
    logText.push(log.querySelector(`.${ClassName.TIME}`)!.textContent!);
    logText.push(log.querySelector(`.${ClassName.TEXT}`)!.textContent!);
    return logText.join(' ');
  }

  private async onClear_(): Promise<void> {
    await BackgroundBridge.LogStore.clearLog();
    location.reload();
  }

  /**
   * When saveLog button is clicked this function runs.
   * Save the current log appeared in the page as a plain text.
   */
  private onSave_(_event: Event): void {
    let outputText = '';
    const logs =
        document.querySelectorAll(`#${IdName.LIST} ${ElementName.PARAGRAPH}`);
    for (const log of logs) {
      outputText += this.logToString_(log) + '\n';
    }

    const a = document.createElement(ElementName.ANCHOR) as HTMLAnchorElement;
    a.download = this.getDownloadFileName_();
    a.href = 'data:text/plain; charset=utf-8,' + encodeURI(outputText);
    a.click();
  }

  /** Update the logs. */
  async update(): Promise<void> {
    const logs = await BackgroundBridge.LogStore.getLogs();
    if (!logs) {
      return;
    }

    for (const log of logs) {
      if (this.isEnabled_(log.logType)) {
        this.addLogToPage_(log);
      }
    }
  }

  /** Update the URL parameter based on the checkboxes. */
  private updateUrlParams_(): void {
    const urlParams: string[] = [];
    for (const type of Object.values(LogType)) {
      urlParams.push(type + 'Filter=' + LogPage.instance.isEnabled_(type));
    }
    location.search = '?' + urlParams.join('&');
  }

  /**
   * Format time stamp.
   * In this log, events are dispatched many times in a short time, so
   * milliseconds order time stamp is required.
   */
  private formatTimeStamp_(dateStr: string): string {
    const date = new Date(dateStr);
    let time = date.getTime();
    time -= date.getTimezoneOffset() * 1000 * 60;
    let timeStr =
        ('00' + Math.floor(time / 1000 / 60 / 60) % 24).slice(-2) + ':';
    timeStr += ('00' + Math.floor(time / 1000 / 60) % 60).slice(-2) + ':';
    timeStr += ('00' + Math.floor(time / 1000) % 60).slice(-2) + '.';
    timeStr += ('000' + time % 1000).slice(-3);
    return timeStr;
  }
}

// Local to module.

enum ClassName {
  FILTER = 'log-filter',
  TEXT = 'log-text',
  TIME = 'log-time-tag',
  TYPE = 'log-type-tag',
}

enum ElementName {
  ANCHOR = 'a',
  INPUT = 'input',
  LABEL = 'label',
  PARAGRAPH = 'p',
  PRE = 'pre',
  SPAN = 'span',
}

enum EventType {
  CLICK = 'click',
}

enum IdName {
  CLEAR = 'clearLog',
  FILTER = 'logFilters',
  LIST = 'logList',
  SAVE = 'saveLog',
}

enum InputType {
  CHECKBOX = 'checkbox',
}
