// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../shared_style.css.js';
import '../shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './activity_log_stream_item.html.js';

export interface StreamItem {
  name?: string;
  timestamp: number;
  activityType: chrome.activityLogPrivate.ExtensionActivityType;
  pageUrl?: string;
  argUrl: string;
  args: string;
  webRequestInfo?: string;
  expanded: boolean;
}

/**
 * A struct used to describe each argument for an activity (each item in
 * the parsed version of |data.args|). Contains the argument's value itself
 * and its index.
 */
export interface StreamArgItem {
  arg: string;
  index: number;
}

/**
 * Placeholder for arg_url that can occur in |StreamItem.args|. Sometimes we
 * see this as '\u003Carg_url>' (opening arrow is unicode converted) but
 * string comparison with the non-unicode value still returns true so we
 * don't need to convert.
 */
export const ARG_URL_PLACEHOLDER: string = '<arg_url>';

/**
 * Regex pattern for |ARG_URL_PLACEHOLDER| for String.replace. A regex of the
 * exact string with a global search flag is needed to replace all
 * occurrences.
 */
const ARG_URL_PLACEHOLDER_REGEX: RegExp = /"<arg_url>"/g;

export class ActivityLogStreamItemElement extends PolymerElement {
  static get is() {
    return 'activity-log-stream-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The underlying ActivityGroup that provides data for the
       * ActivityLogItem displayed.
       */
      data: Object,

      argsList_: {
        type: Array,
        computed: 'computeArgsList_(data.args)',
      },

      isExpandable_: {
        type: Boolean,
        computed: 'computeIsExpandable_(data)',
      },
    };
  }

  data: StreamItem;
  private argsList_: StreamArgItem[];
  private isExpandable_: boolean;

  private computeIsExpandable_(): boolean {
    return this.hasPageUrl_() || this.hasArgs_() || this.hasWebRequestInfo_();
  }

  private getFormattedTime_(): string {
    // Format the activity's time to HH:MM:SS.mmm format. Use ToLocaleString
    // for HH:MM:SS and padLeft for milliseconds.
    const activityDate = new Date(this.data.timestamp);
    const timeString = activityDate.toLocaleTimeString(undefined, {
      hour12: false,
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
    });

    const ms = activityDate.getMilliseconds().toString().padStart(3, '0');
    return `${timeString}.${ms}`;
  }

  private hasPageUrl_(): boolean {
    return !!this.data.pageUrl;
  }

  private hasArgs_(): boolean {
    return this.argsList_.length > 0;
  }

  private hasWebRequestInfo_(): boolean {
    return !!this.data.webRequestInfo && this.data.webRequestInfo !== '{}';
  }

  private computeArgsList_(): StreamArgItem[] {
    const parsedArgs = JSON.parse(this.data.args);
    if (!Array.isArray(parsedArgs)) {
      return [];
    }

    // Replace occurrences AFTER parsing then stringifying as the JSON
    // serializer on the C++ side escapes certain characters such as '<' and
    // parsing un-escapes these characters.
    // See EscapeSpecialCodePoint in base/json/string_escape.cc.
    return parsedArgs.map(
        (arg, i) => ({
          arg: JSON.stringify(arg).replace(
              ARG_URL_PLACEHOLDER_REGEX, `"${this.data.argUrl}"`),
          index: i + 1,
        }));
  }

  private onExpandClick_() {
    if (this.isExpandable_) {
      this.set('data.expanded', !this.data.expanded);
      this.dispatchEvent(new CustomEvent('resize-stream'));
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'activity-log-stream-item': ActivityLogStreamItemElement;
  }
}

customElements.define(
    ActivityLogStreamItemElement.is, ActivityLogStreamItemElement);
